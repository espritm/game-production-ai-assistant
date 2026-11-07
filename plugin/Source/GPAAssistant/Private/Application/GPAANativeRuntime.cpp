// Copyright Game Production AI Assistant contributors 2026.
#include "Application/GPAANativeRuntime.h"
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
    }
    return Instance.ToSharedRef();
}
void FGPAANativeRuntime::Shutdown()
{
    if (!Instance)
        return;
    Instance->Cancel();
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
                  SourcesJson = MoveTemp(ChatSources);
    auto Self = AsShared();
    Jobs.Add(Async(EAsyncExecution::ThreadPool,
                   [Self, Root, Question, Answer, Complete, SourcesJson, OnSaved]()
                   {
                       FString Error;
                       {
                           FScopeLock Lock(&Self->IndexMutex);
                           FGPAANativeStorage Store(DatabasePath(Root));
                           if (!Store.AddHistory(Question, Answer, Complete, Error, SourcesJson))
                               UE_LOG(LogTemp, Warning, TEXT("GPAA could not save local conversation history: %s"),
                                      *Error);
                       }
                       if (OnSaved)
                           AsyncTask(ENamedThreads::GameThread, [OnSaved, Error]() { OnSaved(Error); });
                   }));
}
void FGPAANativeRuntime::Cancel()
{
    if (CurrentTask)
        CurrentTask->Cancelled.Store(true);
    if (Cloud)
        Cloud->Cancel();
    StoreHistory(false);
}
void FGPAANativeRuntime::Run(const FString &ProjectRoot, const FString &AssetSnapshot, const FString &Query, bool Ask,
                             bool RefreshOnly, TFunction<void(const FString &)> OnStatus,
                             TFunction<void(const FString &)> OnText,
                             TFunction<void(bool, const FString &, const FGPAANativeResult &)> OnFinished)
{
    Cancel();
    Jobs.RemoveAll([](TFuture<void> &Job) { return Job.IsReady(); });
    CurrentTask = MakeShared<FGPAATask, ESPMode::ThreadSafe>();
    auto Task = CurrentTask;
    auto Self = AsShared();
    TWeakPtr<FGPAANativeRuntime, ESPMode::ThreadSafe> Weak = Self;
    FGPAAChatRequest ChatRequest;
    if (Ask)
    {
        const auto *Settings = GetDefault<UGPAAEditorSettings>();
        if (Settings->ChatProvider != TEXT("OpenAI") && Settings->ChatProvider != TEXT("OpenAICompatible"))
        {
            OnFinished(
                false,
                TEXT("Native chat currently supports OpenAI or OpenAICompatible. Select a cloud provider in Settings."),
                {});
            return;
        }
        ChatRequest.Endpoint = Settings->OpenAIBaseUrl;
        ChatRequest.Model = Settings->OpenAIModel;
        ChatRequest.Responses = Settings->ChatProvider == TEXT("OpenAI");
        ChatRequest.MaxOutputTokens = Settings->MaxOutputTokens;
        ChatRequest.Timeout = Settings->TimeoutSeconds;
        FGPAANativeSecrets Secrets;
        ChatRequest.Key = Secrets.Read(FGPAANativeSecrets::Scope(ChatRequest.Endpoint));
        if (ChatRequest.Key.IsEmpty())
        {
            OnFinished(false, TEXT("Open Settings, enter your personal API key and use Save Personal Key."), {});
            return;
        }
        if (!Settings->bCloudContextAccepted)
        {
            OnFinished(
                false,
                TEXT("Enable Cloud Context Consent in Settings before sending project excerpts to your provider."), {});
            return;
        }
    }
    OnStatus(TEXT("Refreshing local project index..."));
    Jobs.Add(Async(
        EAsyncExecution::ThreadPool,
        [Self, Weak, Task, ProjectRoot, AssetSnapshot, Query, Ask, RefreshOnly, ChatRequest, OnStatus, OnText,
         OnFinished]() mutable
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
                    Ok = FGPAACore::Search(Query, TEXT("All"), 8, *Self->Embeddings, Store, Task, Sources, Error);
                if (Ok && Ask)
                {
                    int32 Characters = 0, Keep = 0;
                    for (const auto &P : Sources)
                    {
                        int32 Size = FString::Printf(TEXT("Source: %s:%d-%d\n%s\n"), *P.Path, P.FirstLine, P.LastLine,
                                                     *P.Content)
                                         .Len();
                        if (Characters + Size > 12000)
                            break;
                        Characters += Size;
                        ++Keep;
                    }
                    Sources.SetNum(Keep);
                }
            }
            FGPAANativeResult Result = Convert(Report, Sources);
            ChatRequest.Prompt = FGPAACore::Prompt(Query, Sources);
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
                        OnFinished(Ok, Ok ? TEXT("Local project knowledge is ready.") : Error, Result);
                        return;
                    }
                    Pinned->ChatQuestion = Query;
                    Pinned->ChatRoot = ProjectRoot;
                    Pinned->ChatAnswer.Reset();
                    TArray<TSharedPtr<FJsonValue>> References;
                    for (const auto &Source : Result.Sources)
                        References.Add(MakeShared<FJsonValueString>(
                            FString::Printf(TEXT("%s:%d-%d"), *Source.RelativePath, Source.StartLine, Source.EndLine)));
                    Pinned->ChatSources.Reset();
                    FJsonSerializer::Serialize(References, TJsonWriterFactory<>::Create(&Pinned->ChatSources));
                    Pinned->Cloud->Start(
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
