// Copyright Game Production AI Assistant contributors 2026.
#include "GPAAEmbeddings.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
THIRD_PARTY_INCLUDES_START
#include "onnxruntime_c_api.h"
#include <unicode/unorm2.h>
#include <unicode/uchar.h>
THIRD_PARTY_INCLUDES_END

struct FGPAAMiniLM::FImpl
{
    FString Root;
    FCriticalSection Mutex;
    TMap<FString, int64> Vocabulary;
    void *Library = nullptr;
    const OrtApi *Api = nullptr;
    OrtEnv *Env = nullptr;
    OrtSession *Session = nullptr;
    OrtMemoryInfo *Memory = nullptr;
    bool Check(OrtStatus *Status, FString &Error)
    {
        if (!Status)
        {
            return true;
        }
        Error = UTF8_TO_TCHAR(Api->GetErrorMessage(Status));
        Api->ReleaseStatus(Status);
        return false;
    }
    ~FImpl()
    {
        if (Api)
        {
            if (Memory)
                Api->ReleaseMemoryInfo(Memory);
            if (Session)
                Api->ReleaseSession(Session);
            if (Env)
                Api->ReleaseEnv(Env);
        }
        if (Library)
            FPlatformProcess::FreeDllHandle(Library);
    }
    bool Load(FString &Error)
    {
        if (Session)
        {
            return true;
        }
        const FString ThirdParty = FPaths::Combine(Root, TEXT("Source/ThirdParty"));
        if (!Library)
        {
            Library = FPlatformProcess::GetDllHandle(
                *FPaths::Combine(ThirdParty, TEXT("OnnxRuntime/Win64/gpaa_onnxruntime.dll")));
        }
        if (!Library)
        {
            Error = TEXT("Bundled ONNX runtime is missing. Reinstall the complete native plugin package.");
            return false;
        }
        auto GetApi = reinterpret_cast<const OrtApiBase *(ORT_API_CALL *)()>(
            FPlatformProcess::GetDllExport(Library, TEXT("OrtGetApiBase")));
        if (!GetApi || !(Api = GetApi()->GetApi(ORT_API_VERSION)))
        {
            Error = TEXT("ONNX runtime ABI is incompatible.");
            return false;
        }
        if (!Env && !Check(Api->CreateEnv(ORT_LOGGING_LEVEL_ERROR, "GPAA", &Env), Error))
            return false;
        OrtSessionOptions *Options = nullptr;
        if (!Check(Api->CreateSessionOptions(&Options), Error))
            return false;
        bool Ok =
            Check(Api->SetIntraOpNumThreads(Options, 2), Error) &&
            Check(Api->SetSessionGraphOptimizationLevel(Options, ORT_ENABLE_ALL), Error) &&
            Check(Api->CreateSession(Env, *FPaths::Combine(ThirdParty, TEXT("MiniLM/model.onnx")), Options, &Session),
                  Error);
        Api->ReleaseSessionOptions(Options);
        if (!Ok)
            return false;
        if (!Check(Api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &Memory), Error))
        {
            Api->ReleaseSession(Session);
            Session = nullptr;
            return false;
        }
        return true;
    }
    bool Tokens(const FString &Text, TArray<int64> &Output, FString &Error)
    {
        if (Vocabulary.Num() == 0)
        {
            TArray<FString> Lines;
            if (!FFileHelper::LoadFileToStringArray(Lines,
                                                    *FPaths::Combine(Root, TEXT("Source/ThirdParty/MiniLM/vocab.txt"))))
            {
                Error = TEXT("Bundled WordPiece vocabulary is missing.");
                return false;
            }
            for (int32 I = 0; I < Lines.Num(); ++I)
                Vocabulary.Add(Lines[I].TrimEnd(), I);
            if (Vocabulary.FindRef(TEXT("[CLS]")) != 101 || Vocabulary.FindRef(TEXT("[SEP]")) != 102)
            {
                Error = TEXT("WordPiece vocabulary does not match MiniLM.");
                return false;
            }
        }
        UErrorCode Status = U_ZERO_ERROR;
        const UNormalizer2 *Normalizer = unorm2_getNFDInstance(&Status);
        FString Lower = Text.ToLower();
        TArray<UChar> Normalized;
        Normalized.SetNum(Lower.Len() * 4 + 16);
        int32 Count = unorm2_normalize(Normalizer, reinterpret_cast<const UChar *>(*Lower), Lower.Len(),
                                       Normalized.GetData(), Normalized.Num(), &Status);
        if (U_FAILURE(Status))
        {
            Error = TEXT("Unicode normalization failed.");
            return false;
        }
        TArray<FString> Words;
        FString Word;
        auto Flush = [&]()
        {
            if (!Word.IsEmpty())
            {
                Words.Add(MoveTemp(Word));
                Word.Reset();
            }
        };
        for (int32 I = 0; I < Count; ++I)
        {
            UChar C = Normalized[I];
            if (u_charType(C) == U_NON_SPACING_MARK)
                continue;
            if (u_isUWhiteSpace(C))
            {
                Flush();
                continue;
            }
            if (u_iscntrl(C))
                continue;
            if (u_ispunct(C) || (C >= 33 && C <= 47) || (C >= 58 && C <= 64) || (C >= 91 && C <= 96) ||
                (C >= 123 && C <= 126) || (C >= 0x4E00 && C <= 0x9FFF))
            {
                Flush();
                Words.Add(FString::Chr(C));
            }
            else
                Word.AppendChar(C);
        }
        Flush();
        Output.Reset();
        Output.Add(101);
        for (const FString &W : Words)
        {
            if (Output.Num() >= 255)
                break;
            TArray<int64> Pieces;
            bool Unknown = W.Len() > 100;
            for (int32 Start = 0; Start < W.Len() && !Unknown;)
            {
                int32 End = W.Len();
                const int64 *Id = nullptr;
                while (End > Start)
                {
                    const FString Piece = (Start == 0 ? TEXT("") : TEXT("##")) + W.Mid(Start, End - Start);
                    Id = Vocabulary.Find(Piece);
                    if (Id)
                        break;
                    --End;
                }
                if (!Id)
                {
                    Unknown = true;
                    break;
                }
                Pieces.Add(*Id);
                Start = End;
            }
            if (Unknown)
                Pieces = {100};
            for (int64 Id : Pieces)
            {
                if (Output.Num() >= 255)
                    break;
                Output.Add(Id);
            }
        }
        Output.Add(102);
        return true;
    }
};

FGPAAMiniLM::FGPAAMiniLM(const FString &PluginRoot) : Impl(MakeUnique<FImpl>())
{
    Impl->Root = PluginRoot;
}
FGPAAMiniLM::~FGPAAMiniLM() = default;
FString FGPAAMiniLM::Identity() const
{
    return TEXT("minilm-l6-v2@1110a243/onnx-fp32/wordpiece-uncased-nfd/256/mean-mask/l2/384/source-context-v1/schema2");
}
bool FGPAAMiniLM::Tokenize(const FString &Text, TArray<int64> &Tokens, FString &Error)
{
    FScopeLock Lock(&Impl->Mutex);
    return Impl->Tokens(Text, Tokens, Error);
}
bool FGPAAMiniLM::Encode(const FString &Text, TArray<float> &Result, FString &Error)
{
    FScopeLock Lock(&Impl->Mutex);
    if (!Impl->Load(Error))
        return false;
    TArray<int64> Ids;
    if (!Impl->Tokens(Text, Ids, Error))
        return false;
    TArray<int64> Mask, Types;
    Mask.Init(1, Ids.Num());
    Types.Init(0, Ids.Num());
    const int64 Shape[2] = {1, Ids.Num()};
    OrtValue *Inputs[3] = {nullptr, nullptr, nullptr};
    OrtValue *Output = nullptr;
    const char *Names[] = {"input_ids", "attention_mask", "token_type_ids"};
    const char *OutputNames[] = {"last_hidden_state"};
    TArray<int64> *Arrays[] = {&Ids, &Mask, &Types};
    bool Ok = true;
    for (int32 I = 0; I < 3 && Ok; ++I)
        Ok = Impl->Check(Impl->Api->CreateTensorWithDataAsOrtValue(Impl->Memory, Arrays[I]->GetData(),
                                                                   Arrays[I]->Num() * sizeof(int64), Shape, 2,
                                                                   ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, &Inputs[I]),
                         Error);
    if (Ok)
        Ok = Impl->Check(Impl->Api->Run(Impl->Session, nullptr, Names, Inputs, 3, OutputNames, 1, &Output), Error);
    float *Values = nullptr;
    if (Ok)
    {
        OrtTensorTypeAndShapeInfo *Info = nullptr;
        size_t Rank = 0;
        int64 Dimensions[3] = {0, 0, 0};
        Ok = Impl->Check(Impl->Api->GetTensorTypeAndShape(Output, &Info), Error);
        if (Ok)
            Ok = Impl->Check(Impl->Api->GetDimensionsCount(Info, &Rank), Error);
        if (Ok && Rank == 3)
            Ok = Impl->Check(Impl->Api->GetDimensions(Info, Dimensions, 3), Error);
        if (Info)
            Impl->Api->ReleaseTensorTypeAndShapeInfo(Info);
        if (Ok && (Rank != 3 || Dimensions[0] != 1 || Dimensions[1] != Ids.Num() || Dimensions[2] != 384))
        {
            Error = TEXT("Embedding output shape does not match the pinned MiniLM pipeline.");
            Ok = false;
        }
    }
    if (Ok)
        Ok = Impl->Check(Impl->Api->GetTensorMutableData(Output, reinterpret_cast<void **>(&Values)), Error);
    if (Ok)
    {
        Result.Init(0, 384);
        for (int32 T = 0; T < Ids.Num(); ++T)
            for (int32 D = 0; D < 384; ++D)
                Result[D] += Values[T * 384 + D] / Ids.Num();
        double Norm = 0;
        for (float V : Result)
            Norm += V * V;
        Norm = FMath::Sqrt(Norm);
        if (Norm > 0)
            for (float &V : Result)
                V /= Norm;
        else
        {
            Error = TEXT("Embedding model returned a zero vector.");
            Ok = false;
        }
    }
    if (Output)
        Impl->Api->ReleaseValue(Output);
    for (OrtValue *Input : Inputs)
        if (Input)
            Impl->Api->ReleaseValue(Input);
    return Ok;
}
