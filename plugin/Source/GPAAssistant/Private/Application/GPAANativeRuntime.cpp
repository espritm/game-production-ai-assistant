// Copyright Game Production AI Assistant contributors 2026.
#include "Application/GPAANativeRuntime.h"
#include "GPAALocalModels.h"
#include "Infrastructure/GPAANativeStorage.h"
#include "Settings/GPAAEditorSettings.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonSerializer.h"

namespace
{
TSharedPtr<FGPAANativeRuntime, ESPMode::ThreadSafe> Instance;
bool Scan(const FString &Root, const FString &Assets, const FGPAATaskPtr &Task, TArray<FGPAADocument> &Documents,
          FString &Error)
{
    const TSet<FString> Extensions = {TEXT("h"),   TEXT("hpp"),     TEXT("cpp"),     TEXT("cs"),
                                      TEXT("ini"), TEXT("uplugin"), TEXT("uproject")};
    const TSet<FString> Ignored = {TEXT(".git"),       TEXT(".vs"),          TEXT(".vscode"), TEXT(".gpaa"),
                                   TEXT("Binaries"),   TEXT("Intermediate"), TEXT("Saved"),   TEXT("DerivedDataCache"),
                                   TEXT("ThirdParty"), TEXT("node_modules")};
    const FString FullRoot = FPaths::ConvertRelativePathToFull(Root) / TEXT("");
    TArray<FString> Folders = {FPaths::ConvertRelativePathToFull(Root)};
    while (!Folders.IsEmpty())
    {
        if (Task->Cancelled.Load())
        {
            Error = TEXT("Indexing cancelled.");
            return false;
        }
        FString Folder = Folders.Pop(EAllowShrinking::No);
        TArray<FString> Names;
        IFileManager::Get().FindFiles(Names, *(Folder / TEXT("*")), false, true);
        for (const FString &Name : Names)
        {
            // Do not traverse directory links or index the assistant itself.
            const FString Path = Folder / Name;
            if (!Ignored.Contains(Name) && Name != TEXT("GameProductionAIAssistant") &&
                FPlatformFileManager::Get().GetPlatformFile().IsSymlink(*Path) != ESymlinkResult::Symlink)
                Folders.Add(Path);
        }
        Names.Reset();
        IFileManager::Get().FindFiles(Names, *(Folder / TEXT("*")), true, false);
        for (const FString &Name : Names)
        {
            if (!Extensions.Contains(FPaths::GetExtension(Name).ToLower()) ||
                Name.Contains(TEXT("EditorPerProjectUserSettings")))
                continue;
            if (Documents.Num() >= 10000)
            {
                Error = TEXT("Project exceeds the 10000-document native index limit.");
                return false;
            }
            FString Path = Folder / Name;
            if (IFileManager::Get().FileSize(*Path) > 512 * 1024)
            {
                Error = TEXT("Source file exceeds the 512 KiB limit: ") + Name;
                return false;
            }
            FGPAADocument Doc;
            Doc.Path = Path;
            FPaths::MakePathRelativeTo(Doc.Path, *FullRoot);
            Doc.Path.ReplaceInline(TEXT("\\"), TEXT("/"));
            if (!FFileHelper::LoadFileToString(Doc.Content, *Path))
            {
                Error = TEXT("Could not read ") + Doc.Path;
                return false;
            }
            Documents.Add(MoveTemp(Doc));
        }
    }
    TSharedPtr<FJsonObject> Snapshot;
    if (!Assets.IsEmpty() && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Assets), Snapshot) && Snapshot)
    {
        const TArray<TSharedPtr<FJsonValue>> *Array = nullptr;
        if (Snapshot->TryGetArrayField(TEXT("assets"), Array))
            for (const auto &Value : *Array)
            {
                const auto Asset = Value->AsObject();
                if (!Asset)
                    continue;
                FGPAADocument Doc;
                Doc.Kind = TEXT("Assets");
                FString ObjectPath;
                Asset->TryGetStringField(TEXT("objectPath"), ObjectPath);
                Doc.Path = TEXT("asset:") + ObjectPath;
                FJsonSerializer::Serialize(Asset.ToSharedRef(), TJsonWriterFactory<>::Create(&Doc.Content));
                Documents.Add(MoveTemp(Doc));
            }
    }
    Documents.Sort([](const FGPAADocument &A, const FGPAADocument &B) { return A.Path < B.Path; });
    return true;
}
FGPAANativeResult Convert(const FGPAAIndexReport &Report, const TArray<FGPAAPassage> &Sources)
{
    FGPAANativeResult Result;
    auto &S = Result.Statistics;
    S.FilesScanned = S.FilesIndexed = Report.Files;
    S.ChunksIndexed = Report.Chunks;
    S.FilesReused = Report.Reused;
    S.FilesChanged = Report.Files - Report.Reused;
    S.FilesRemoved = Report.Removed;
    S.ChunksEmbedded = Report.Embedded;
    S.ChunksReused = Report.Chunks - Report.Embedded;
    S.EmbeddingModel = Report.Model;
    S.IndexedAt = FDateTime::UtcNow().ToIso8601();
    for (const auto &P : Sources)
    {
        FGPAASourceReference Ref;
        Ref.RelativePath = P.Path;
        Ref.Content = P.Content;
        Ref.SourceType = P.Kind == TEXT("Assets") ? TEXT("Asset") : TEXT("Code");
        Ref.StartLine = P.FirstLine;
        Ref.EndLine = P.LastLine;
        Ref.Score = P.Score;
        Result.Sources.Add(MoveTemp(Ref));
    }
    return Result;
}
} // namespace

TSharedRef<FGPAANativeRuntime, ESPMode::ThreadSafe> FGPAANativeRuntime::Get()
{
    check(IsInGameThread());
    if (!Instance)
    {
        Instance = MakeShared<FGPAANativeRuntime, ESPMode::ThreadSafe>();
        Instance->Embeddings = MakeShared<FGPAAMiniLM, ESPMode::ThreadSafe>(
            IPluginManager::Get().FindPlugin(TEXT("GameProductionAIAssistant"))->GetBaseDir());
        Instance->Cloud = MakeShared<FGPAACloudChat, ESPMode::ThreadSafe>();
        Instance->Local = MakeShared<FGPAALocalChat, ESPMode::ThreadSafe>();
    }
    return Instance.ToSharedRef();
}
void FGPAANativeRuntime::Shutdown()
{
    if (!Instance)
        return;
    Instance->Cancel();
    if (Instance->Local) Instance->Local->Wait();
    for (auto &Job : Instance->Jobs)
        Job.Wait();
    Instance->Jobs.Reset();
    FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
    Instance.Reset();
}
void FGPAANativeRuntime::CancelIfRunning()
{
    if (Instance)
        Instance->Cancel();
}
FString FGPAANativeRuntime::DatabasePath(const FString &ProjectRoot)
{
    return FPaths::ConvertRelativePathToFull(ProjectRoot) / TEXT("Saved/GPAA/native-v1.sqlite");
}
void FGPAANativeRuntime::StoreHistory(bool Complete, TFunction<void(const FString &)> OnSaved)
{
    if (ChatAnswer.IsEmpty())
    {
        ChatQuestion.Reset();
        if (OnSaved)
            OnSaved(TEXT(""));
        return;
    }
    const FString Root = ChatRoot, Question = MoveTemp(ChatQuestion), Answer = MoveTemp(ChatAnswer),
                  SourcesJson = MoveTemp(ChatSources), ProviderName = MoveTemp(ChatProviderName), ModelName = MoveTemp(ChatModelName);
    auto Self = AsShared();
    Jobs.Add(Async(EAsyncExecution::ThreadPool,
                   [Self, Root, Question, Answer, Complete, SourcesJson, ProviderName, ModelName, OnSaved]()
                   {
                       FString Error;
                       {
                           FScopeLock Lock(&Self->IndexMutex);
                           FGPAANativeStorage Store(DatabasePath(Root));
                           if (!Store.AddHistory(Question, Answer, Complete, Error, SourcesJson, TEXT("Prompt"), TEXT("All"), ProviderName, ModelName))
                               UE_LOG(LogTemp, Warning, TEXT("GPAA could not save local conversation history: %s"),
                                      *Error);
                       }
                       if (OnSaved)
                           AsyncTask(ENamedThreads::GameThread, [OnSaved, Error]() { OnSaved(Error); });
                   }));
}
void FGPAANativeRuntime::Cancel()
{
    bRunning = false;
    if (CurrentTask)
        CurrentTask->Cancelled.Store(true);
    if (Cloud)
        Cloud->Cancel();
    if (Local) Local->Cancel();
    StoreHistory(false);
}
bool FGPAANativeRuntime::IsBusy() const
{
    if (bRunning || (Local && Local->IsBusy())) return true;
    for (const auto& Job : Jobs) if (!Job.IsReady()) return true;
    return false;
}
bool FGPAANativeRuntime::RefreshIfIdle(const FString& ProjectRoot, const FString& AssetSnapshot)
{
    if (IsBusy()) return false;
    Run(ProjectRoot, AssetSnapshot, TEXT(""), false, true,
        [](const FString&) {}, [](const FString&) {},
        [](bool Ok, const FString& Message, const FGPAANativeResult& Result)
        {
            if (!Ok)
            {
                UE_LOG(LogTemp, Warning, TEXT("GPAA automatic index refresh failed: %s"), *Message);
            }
            else
            {
                UE_LOG(LogTemp, Display, TEXT("GPAA automatic index ready: files=%d changed=%d reused=%d"),
                    Result.Statistics.FilesIndexed, Result.Statistics.FilesChanged, Result.Statistics.FilesReused);
            }
        });
    return true;
}
void FGPAANativeRuntime::Run(const FString &ProjectRoot, const FString &AssetSnapshot, const FString &Query, bool Ask,
                             bool RefreshOnly, TFunction<void(const FString &)> OnStatus,
                             TFunction<void(const FString &)> OnText,
                             TFunction<void(bool, const FString &, const FGPAANativeResult &)> OnFinished,
                             const FString& SearchScope)
{
    Cancel();
    Jobs.RemoveAll([](TFuture<void> &Job) { return Job.IsReady(); });
    CurrentTask = MakeShared<FGPAATask, ESPMode::ThreadSafe>();
    auto Task = CurrentTask;
    auto Self = AsShared();
    TWeakPtr<FGPAANativeRuntime, ESPMode::ThreadSafe> Weak = Self;
    bRunning = true;
    auto Completion = MoveTemp(OnFinished);
    OnFinished = [Weak, Task, Completion](bool Ok, const FString& Message, const FGPAANativeResult& Result)
    {
        if (auto Live = Weak.Pin())
            if (Live->CurrentTask == Task) Live->bRunning = false;
        Completion(Ok, Message, Result);
    };
    FGPAAChatRequest ChatRequest;
    ChatRequest.OnStatus = [Weak, Task, OnStatus](const FString& Message)
    { if (Weak.IsValid() && !Task->Cancelled.Load()) OnStatus(Message); };
    if (Ask)
    {
        const auto *Settings = GetDefault<UGPAAEditorSettings>();
        ChatRequest.Local = Settings->ChatProvider == TEXT("Local");
        ChatRequest.RequireProjectEvidence = ChatRequest.Local;
        ChatRequest.MaxOutputTokens = Settings->MaxOutputTokens;
        ChatRequest.Timeout = Settings->TimeoutSeconds;
        if (ChatRequest.Local)
        {
            const auto* Model = FGPAALocalModels::Find(Settings->LocalModelId);
            ChatRequest.LocalCustomModel = !Settings->LocalModelFile.IsEmpty();
            ChatRequest.Model = ChatRequest.LocalCustomModel ? FPaths::GetCleanFilename(Settings->LocalModelFile) : Settings->LocalModelId;
            if (!ChatRequest.LocalCustomModel && !Model)
            { OnFinished(false, TEXT("Select a supported local model in Settings."), {}); return; }
            ChatRequest.LocalModelPath = ChatRequest.LocalCustomModel ? Settings->LocalModelFile : FGPAALocalModels::ModelPath(*Model);
            ChatRequest.LocalContextTokens = Settings->LocalContextTokens;
            ChatRequest.LocalThreads = Settings->LocalThreads;
            if (!IFileManager::Get().FileExists(*ChatRequest.LocalModelPath))
            { OnFinished(false, TEXT("Local model is missing. In Settings > Local AI, download the selected model or browse to an existing GGUF. No cloud fallback was made."), {}); return; }
        }
        else
        {
            if (Settings->ChatProvider != TEXT("OpenAI") && Settings->ChatProvider != TEXT("OpenAICompatible"))
            { OnFinished(false, TEXT("Setup required. Use Quick Start or select Local AI / Cloud AI Provider in Settings."), {}); return; }
            ChatRequest.Endpoint = Settings->OpenAIBaseUrl;
            ChatRequest.Model = Settings->OpenAIModel;
            ChatRequest.Responses = Settings->ChatProvider == TEXT("OpenAI");
            FGPAANativeSecrets Secrets;
            ChatRequest.Key = Secrets.Read(FGPAANativeSecrets::Scope(ChatRequest.Endpoint));
            if (ChatRequest.Key.IsEmpty())
            { OnFinished(false, TEXT("Open Settings, enter your personal API key and use Save Personal Key."), {}); return; }
        }
    }

    OnStatus(TEXT("Refreshing local project index..."));
    Jobs.Add(Async(
        EAsyncExecution::ThreadPool,
        [Self, Weak, Task, ProjectRoot, AssetSnapshot, Query, Ask, RefreshOnly, ChatRequest, OnStatus, OnText,
         OnFinished, SearchScope]() mutable
        {
            FString Error;
            FGPAAIndexReport Report;
            TArray<FGPAAPassage> Sources;
            bool Ok = false;
            {
                FScopeLock Lock(&Self->IndexMutex);
                TArray<FGPAADocument> Documents;
                FGPAANativeStorage Store(DatabasePath(ProjectRoot));
                Ok =
                    Scan(ProjectRoot, AssetSnapshot, Task, Documents, Error) &&
                    FGPAACore::Index(Documents, *Self->Embeddings, Store, Task, Report, Error,
                                     [Task, OnStatus](int32 Done, int32 Total)
                                     {
                                         if (Done % 10 == 0 || Done == Total)
                                             AsyncTask(ENamedThreads::GameThread,
                                                       [Task, OnStatus, Done, Total]()
                                                       {
                                                           if (!Task->Cancelled.Load())
                                                               OnStatus(FString::Printf(
                                                                   TEXT("Indexing locally: %d/%d files"), Done, Total));
                                                       });
                                     });
                if (Ok && !RefreshOnly)
                    Ok = FGPAACore::Search(Query, SearchScope, 8, *Self->Embeddings, Store, Task, Sources, Error);
                if (Ok && !Ask && !RefreshOnly && !Task->Cancelled.Load())
                {
                    TArray<TSharedPtr<FJsonValue>> References;
                    for (const auto& Source : Sources)
                        References.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%s:%d-%d (score %.3f)"),
                            *Source.Path, Source.FirstLine, Source.LastLine, Source.Score)));
                    FString Json, StorageError;
                    FJsonSerializer::Serialize(References, TJsonWriterFactory<>::Create(&Json));
                    if (!Store.AddHistory(Query, FString::Printf(TEXT("%d result(s)."), Sources.Num()), true,
                            StorageError, Json, TEXT("Search"), SearchScope))
                        Error = TEXT("Search complete, but local history could not be saved: ") + StorageError;
                }
                if (Ok && Ask)
                {
                    int32 Characters = 0, Keep = 0;
                    const int32 ContextCharacters = ChatRequest.Local
                        ? FMath::Clamp((ChatRequest.LocalContextTokens - ChatRequest.MaxOutputTokens - 512) * 2 - Query.Len(), 0, 12000)
                        : 12000;
                    for (const auto &P : Sources)
                    {
                        int32 Size = FString::Printf(TEXT("Source: %s:%d-%d\n%s\n"), *P.Path, P.FirstLine, P.LastLine,
                                                     *P.Content)
                                         .Len();
                        if (Characters + Size > ContextCharacters)
                            break;
                        Characters += Size;
                        ++Keep;
                    }
                    if (ChatRequest.Local && Keep == 0 && !Sources.IsEmpty())
                    { Ok = false; Error = TEXT("The local context budget cannot fit a retrieved passage. Shorten the question, lower Max Output Tokens or increase Local Context Tokens."); }
                    Sources.SetNum(Keep);
                }
            }
            FGPAANativeResult Result = Convert(Report, Sources);
            ChatRequest.Prompt = FGPAACore::Prompt(Query, Sources);
            ChatRequest.HasProjectEvidence = !Sources.IsEmpty();
            for (const auto& Source : Sources) ChatRequest.SourceEvidence += Source.Path + TEXT("\n") + Source.Content + TEXT("\n");
            AsyncTask(
                ENamedThreads::GameThread,
                [Weak, Task, ProjectRoot, Query, Ask, RefreshOnly, ChatRequest, OnText, OnFinished, Result, Error,
                 Ok]() mutable
                {
                    auto Pinned = Weak.Pin();
                    if (!Pinned || Task->Cancelled.Load())
                        return;
                    if (!Ok || !Ask || RefreshOnly)
                    {
                        OnFinished(Ok, Ok && Error.IsEmpty() ? TEXT("Local project knowledge is ready.") : Error, Result);
                        return;
                    }
                    Pinned->ChatProviderName = ChatRequest.Local ? TEXT("Local") : (ChatRequest.Responses ? TEXT("OpenAI") : TEXT("OpenAICompatible"));
                    Pinned->ChatModelName = ChatRequest.Model;
                    Pinned->ChatQuestion = Query;
                    Pinned->ChatRoot = ProjectRoot;
                    Pinned->ChatAnswer.Reset();
                    TArray<TSharedPtr<FJsonValue>> References;
                    for (const auto &Source : Result.Sources)
                        References.Add(MakeShared<FJsonValueString>(
                            FString::Printf(TEXT("%s:%d-%d"), *Source.RelativePath, Source.StartLine, Source.EndLine)));
                    Pinned->ChatSources.Reset();
                    FJsonSerializer::Serialize(References, TJsonWriterFactory<>::Create(&Pinned->ChatSources));
                    TSharedPtr<IGPAAChatProvider, ESPMode::ThreadSafe> Provider = ChatRequest.Local
                        ? StaticCastSharedPtr<IGPAAChatProvider>(Pinned->Local) : StaticCastSharedPtr<IGPAAChatProvider>(Pinned->Cloud);
                    Provider->Start(
                        ChatRequest,
                        [Weak, Task, OnText](const FString &Delta)
                        {
                            auto Live = Weak.Pin();
                            if (Live && !Task->Cancelled.Load())
                            {
                                Live->ChatAnswer += Delta;
                                OnText(Delta);
                            }
                        },
                        [Weak, Task, OnFinished, Result](bool Complete, const FString &Message) mutable
                        {
                            auto Live = Weak.Pin();
                            if (!Live || Task->Cancelled.Load())
                                return;
                            Result.Answer = Live->ChatAnswer;
                            Live->StoreHistory(
                                Complete,
                                [Task, OnFinished, Complete, Message, Result](const FString &StorageError)
                                {
                                    if (Task->Cancelled.Load())
                                        return;
                                    const FString FinalMessage =
                                        !StorageError.IsEmpty()
                                            ? TEXT("Answer received, but local history could not be saved.")
                                            : (Complete
                                                   ? TEXT(
                                                         "Answer complete. Context and history remain indexed locally.")
                                                   : Message);
                                    OnFinished(Complete, FinalMessage, Result);
                                });
                        });
                });
        }));
}
void FGPAANativeRuntime::LoadHistory(const FString &ProjectRoot, TFunction<void(bool, const FString &)> OnFinished)
{
    auto Self = AsShared();
    Jobs.Add(Async(EAsyncExecution::ThreadPool,
                   [Self, ProjectRoot, OnFinished]()
                   {
                       FString Error, History;
                       {
                           FScopeLock Lock(&Self->IndexMutex);
                           FGPAANativeStorage Store(DatabasePath(ProjectRoot));
                           History = Store.History(Error);
                       }
                       AsyncTask(ENamedThreads::GameThread, [OnFinished, Error, History]()
                                 { OnFinished(Error.IsEmpty(), Error.IsEmpty() ? History : Error); });
                   }));
}
void FGPAANativeRuntime::ImportHistory(const FString &ProjectRoot, const FString &JsonPath,
                                       TFunction<void(bool, const FString &)> OnFinished)
{
    auto Self = AsShared();
    Jobs.Add(Async(
        EAsyncExecution::ThreadPool,
        [Self, ProjectRoot, JsonPath, OnFinished]()
        {
            FString Error;
            bool Ok;
            {
                FScopeLock Lock(&Self->IndexMutex);
                FGPAANativeStorage Store(DatabasePath(ProjectRoot));
                Ok = Store.ImportLegacyHistory(JsonPath, Error);
            }
            AsyncTask(ENamedThreads::GameThread,
                      [OnFinished, Ok, Error]()
                      {
                          OnFinished(Ok, Ok ? TEXT("Legacy history imported. Keys and embeddings were not imported.")
                                            : Error);
                      });
        }));
}
