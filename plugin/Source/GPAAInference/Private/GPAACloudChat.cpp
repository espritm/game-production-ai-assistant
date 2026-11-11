// Copyright Game Production AI Assistant contributors 2026.
#include "GPAACloudChat.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "HttpManager.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Modules/ModuleManager.h"

namespace
{
struct FOwnedRequest
{
    FHttpRequestPtr Request;
    FGPAATaskPtr Task;
    TWeakPtr<FGPAACloudChat, ESPMode::ThreadSafe> Owner;
};
TArray<FOwnedRequest> ModuleRequests;
bool bHttpStopping = false;
}

void FGPAACloudChat::InitializeRequests()
{
    bHttpStopping = false;
}
void FGPAACloudChat::ShutdownRequests()
{
    if (bHttpStopping) return;
    bHttpStopping = true;
    auto Requests = MoveTemp(ModuleRequests);
    // Stop callbacks before touching transport; owners must release their request
    // references while HTTP is still alive, not in late object destructors.
    for (const auto& Entry : Requests)
    {
        if (Entry.Task) Entry.Task->Cancelled.Store(true);
        auto Owner = Entry.Owner.Pin();
        if (Owner && Owner->ActiveRequest == Entry.Request) Owner->ActiveRequest.Reset();
        // Keep the owner alive until its completion delegate has been released;
        // a delegate may itself hold the provider's last shared reference.
        if (Entry.Request) Entry.Request->OnProcessRequestComplete().Unbind();
    }
    auto* Http = FModuleManager::GetModulePtr<FHttpModule>(TEXT("HTTP"));
    bool Pending=false;
    if (Http)
    {
        for (const auto& Entry : Requests)
        {
            if (!Entry.Request) continue;
            Pending |= Http->GetHttpManager().IsValidRequest(Entry.Request.Get());
            Entry.Request->CancelRequest();
        }
        if (Pending) Http->GetHttpManager().Flush(EHttpFlushReason::Shutdown);
    }
    Requests.Reset();
    FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
    UE_LOG(LogTemp, Display, TEXT("GPAA HTTP request shutdown complete."));
}

bool FGPAASseDecoder::Event(const FString &Data, TArray<FString> &Deltas, FString &Error)
{
    if (Data == TEXT("[DONE]"))
    {
        Complete = true;
        return true;
    }
    TSharedPtr<FJsonObject> Object;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Data), Object) || !Object)
    {
        Error = TEXT("Provider returned an invalid streaming event.");
        return false;
    }
    FString Type;
    Object->TryGetStringField(TEXT("type"), Type);
    if (Type == TEXT("response.output_text.delta") || Type == TEXT("response.refusal.delta"))
    {
        FString Delta;
        if (Object->TryGetStringField(TEXT("delta"), Delta))
            Deltas.Add(Delta);
    }
    else if (Type == TEXT("response.completed"))
        Complete = true;
    else if (Type == TEXT("error") || Type == TEXT("response.failed") || Type == TEXT("response.incomplete") ||
             Object->HasField(TEXT("error")))
    {
        Error = TEXT("Provider interrupted the response. Check model access, quota and output limits.");
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>> *Choices = nullptr;
    if (Object->TryGetArrayField(TEXT("choices"), Choices))
    {
        for (const auto &Choice : *Choices)
        {
            const auto C = Choice->AsObject();
            const TSharedPtr<FJsonObject> *Delta = nullptr;
            FString Text;
            if (C && C->TryGetObjectField(TEXT("delta"), Delta) && (*Delta)->TryGetStringField(TEXT("content"), Text))
                Deltas.Add(Text);
            FString Finish;
            if (C && C->TryGetStringField(TEXT("finish_reason"), Finish) && Finish == TEXT("length"))
            {
                Error = TEXT("Response reached its output limit and is incomplete.");
                return false;
            }
        }
    }
    return true;
}

bool FGPAASseDecoder::Feed(const uint8 *Bytes, int32 Length, TArray<FString> &Deltas, FString &Error)
{
    if (Complete)
        return true;
    if (Length < 0 || Pending.Num() + Length > 1024 * 1024)
    {
        Error = TEXT("Provider streaming event exceeded the size limit.");
        return false;
    }
    Pending.Append(Bytes, Length);
    for (;;)
    {
        int32 End = INDEX_NONE, Separator = 0;
        for (int32 I = 0; I + 1 < Pending.Num(); ++I)
        {
            if (Pending[I] == '\n' && Pending[I + 1] == '\n')
            {
                End = I;
                Separator = 2;
                break;
            }
            if (I + 3 < Pending.Num() && Pending[I] == '\r' && Pending[I + 1] == '\n' && Pending[I + 2] == '\r' &&
                Pending[I + 3] == '\n')
            {
                End = I;
                Separator = 4;
                break;
            }
        }
        if (End == INDEX_NONE)
            break;
        FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR *>(Pending.GetData()), End);
        FString Block(Converted.Length(), Converted.Get());
        Pending.RemoveAt(0, End + Separator, EAllowShrinking::No);
        TArray<FString> Lines;
        Block.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
        Block.ParseIntoArray(Lines, TEXT("\n"), false);
        FString Data;
        for (const FString &Line : Lines)
            if (Line.StartsWith(TEXT("data:")))
            {
                FString Part = Line.Mid(5);
                if (Part.StartsWith(TEXT(" ")))
                    Part.RightChopInline(1);
                if (!Data.IsEmpty())
                    Data += TEXT("\n");
                Data += Part;
            }
        if (!Data.IsEmpty() && !Event(Data, Deltas, Error))
            return false;
        if (Complete)
            break;
    }
    return true;
}

FGPAACloudChat::~FGPAACloudChat()
{
    Cancel();
}
void FGPAACloudChat::Cancel()
{
    if (ActiveTask)
        ActiveTask->Cancelled.Store(true);
    if (ActiveRequest)
    {
        ActiveRequest->OnProcessRequestComplete().Unbind();
        if (!bHttpStopping && !IsEngineExitRequested() && FModuleManager::GetModulePtr<FHttpModule>(TEXT("HTTP")))
            ActiveRequest->CancelRequest();
        ActiveRequest.Reset();
    }
}
void FGPAACloudChat::Start(const FGPAAChatRequest &Request, FGPAATextEvent OnText, FGPAAChatFinished OnFinished)
{
    Cancel();
    if (bHttpStopping || IsEngineExitRequested())
    {
        OnFinished(false, TEXT("The assistant HTTP provider is shutting down."));
        return;
    }
    auto* Http = FModuleManager::GetModulePtr<FHttpModule>(TEXT("HTTP"));
    if (!Http)
    {
        OnFinished(false, TEXT("The HTTP module is unavailable."));
        return;
    }
    FString Base = Request.Endpoint.TrimStartAndEnd();
    Base.RemoveFromEnd(TEXT("/"));
    if ((!Base.StartsWith(TEXT("https://")) && !Base.StartsWith(TEXT("http://127.0.0.1:"))) ||
        Base.Contains(TEXT("@")) || Base.Contains(TEXT("?")) || Request.Key.IsEmpty() || Request.Model.IsEmpty())
    {
        OnFinished(false, TEXT("Set a valid HTTPS provider URL, model and personal API key."));
        return;
    }
    TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("model"), Request.Model);
    Body->SetBoolField(TEXT("stream"), true);
    if (Request.Responses)
    {
        Body->SetStringField(TEXT("input"), Request.Prompt);
        Body->SetBoolField(TEXT("store"), false);
        Body->SetNumberField(TEXT("max_output_tokens"), FMath::Clamp(Request.MaxOutputTokens, 64, 32768));
    }
    else
    {
        TSharedRef<FJsonObject> Message = MakeShared<FJsonObject>();
        Message->SetStringField(TEXT("role"), TEXT("user"));
        Message->SetStringField(TEXT("content"), Request.Prompt);
        Body->SetArrayField(TEXT("messages"), {MakeShared<FJsonValueObject>(Message)});
        Body->SetNumberField(TEXT("max_tokens"), FMath::Clamp(Request.MaxOutputTokens, 64, 32768));
    }
    FString Json;
    FJsonSerializer::Serialize(Body, TJsonWriterFactory<>::Create(&Json));
    ActiveTask = MakeShared<FGPAATask, ESPMode::ThreadSafe>();
    const auto Task = ActiveTask;
    auto Decoder = MakeShared<FGPAASseDecoder, ESPMode::ThreadSafe>();
    auto Mutex = MakeShared<FCriticalSection, ESPMode::ThreadSafe>();
    auto ParseError = MakeShared<FString, ESPMode::ThreadSafe>();
    ActiveRequest = Http->CreateRequest();
    ModuleRequests.RemoveAll([Http](const FOwnedRequest& Item){return !Http->GetHttpManager().IsValidRequest(Item.Request.Get());});
    ModuleRequests.Add({ActiveRequest, Task, AsShared()});
    ActiveRequest->SetURL(Base + (Request.Responses ? TEXT("/responses") : TEXT("/chat/completions")));
    ActiveRequest->SetVerb(TEXT("POST"));
    ActiveRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    ActiveRequest->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Request.Key);
    ActiveRequest->SetTimeout(FMath::Clamp(Request.Timeout, 5.0f, 600.0f));
    ActiveRequest->SetContentAsString(Json);
    ActiveRequest->SetResponseBodyReceiveStreamDelegateV2(FHttpRequestStreamDelegateV2::CreateLambda(
        [Task, Decoder, Mutex, ParseError, OnText](void *Ptr, int64 &Length)
        {
            if (Task->Cancelled.Load())
            {
                Length = 0;
                return;
            }
            TArray<FString> Deltas;
            {
                FScopeLock Lock(&Mutex.Get());
                if (!Decoder->Feed(static_cast<const uint8 *>(Ptr), static_cast<int32>(Length), Deltas, *ParseError))
                {
                    Length = 0;
                    return;
                }
            }
            for (const FString &Delta : Deltas)
                AsyncTask(ENamedThreads::GameThread,
                          [Task, OnText, Delta]()
                          {
                              if (!Task->Cancelled.Load())
                                  OnText(Delta);
                          });
        }));
    TWeakPtr<FGPAACloudChat, ESPMode::ThreadSafe> Weak = AsShared();
    ActiveRequest->OnProcessRequestComplete().BindLambda(
        [Weak, Task, Decoder, Mutex, ParseError, OnFinished](FHttpRequestPtr Req, FHttpResponsePtr Response,
                                                             bool TransportOk)
        {
            if (Task->Cancelled.Load())
                return;
            const int32 Code = Response ? Response->GetResponseCode() : 0;
            FString Error;
            if (Code == 401 || Code == 403)
                Error = TEXT("Authentication failed. Check the personal API key and model access.");
            else if (Code == 429)
                Error = TEXT("Provider quota or rate limit reached. Check billing and retry later.");
            else if (Code < 200 || Code >= 300)
                Error = FString::Printf(TEXT("Provider request failed (HTTP %d). No automatic retry was made."), Code);
            else
            {
                FScopeLock Lock(&Mutex.Get());
                Error = *ParseError;
                if (Error.IsEmpty() && (!TransportOk || !Decoder->IsComplete()))
                    Error = TEXT("Connection ended before a complete response was received.");
            }
            if (auto Owner = Weak.Pin())
                Owner->ActiveRequest.Reset();
            OnFinished(Error.IsEmpty(), Error);
        });
    if (!ActiveRequest->ProcessRequest())
        OnFinished(false, TEXT("Could not start provider request."));
}
