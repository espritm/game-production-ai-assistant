// Copyright Game Production AI Assistant contributors 2026.
#include "Misc/AutomationTest.h"
#include "GPAACore.h"
#include "GPAAEmbeddings.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "GPAACloudChat.h"
#include "Infrastructure/GPAANativeStorage.h"
#include "Application/GPAANativeRuntime.h"
#include "Settings/GPAAEditorSettings.h"
#include "UObject/UnrealType.h"
#include "UI/SGPAAssistantWidget.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpServerResponse.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "IPAddress.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAANativePanelTest, "GPAA.Native.UI.FirstLaunchPanel",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGPAANativePanelTest::RunTest(const FString &Parameters)
{
    auto Panel = SNew(SGPAAssistantWidget);
    Panel->SlatePrepass();
    TestTrue(TEXT("First-launch assistant panel builds without a backend client"),
             Panel->GetDesiredSize().X > 0 && Panel->GetDesiredSize().Y > 0);
    return true;
}

struct FGPAACloudHttpState
{
    bool Done = false, Ok = false, Contract = false;
    FString Text, Error;
    TSharedPtr<FGPAACloudChat, ESPMode::ThreadSafe> Provider;
    TSharedPtr<IHttpRouter> Router;
    FHttpRouteHandle Route;
};
DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FGPAAWaitForCloudHttp, TSharedPtr<FGPAACloudHttpState>, State, double,
                                                 Deadline, FAutomationTestBase *, Test);
bool FGPAAWaitForCloudHttp::Update()
{
    if (!State->Done && FPlatformTime::Seconds() < Deadline)
        return false;
    if (!State->Done)
        State->Provider->Cancel();
    State->Router->UnbindRoute(State->Route);
    State->Route.Reset();
    Test->TestTrue(TEXT("HTTP Responses contract"), State->Contract);
    Test->TestTrue(TEXT("HTTP streaming completes"), State->Done && State->Ok);
    Test->TestEqual(TEXT("HTTP delta text"), State->Text, FString(TEXT("Hello from the simulated provider.")));
    if (!State->Error.IsEmpty())
        Test->AddError(State->Error);
    return true;
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FGPAANativeCloudHttpTest, "GPAA.Native.Cloud.HttpContract",
                                  EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FGPAANativeCloudHttpTest::GetTests(TArray<FString> &Names, TArray<FString> &Commands) const
{
    Names.Add(TEXT("Responses"));
    Commands.Add(TEXT("responses"));
    Names.Add(TEXT("ChatCompletions"));
    Commands.Add(TEXT("compatible"));
}
bool FGPAANativeCloudHttpTest::RunTest(const FString &Parameters)
{
    const bool Responses = Parameters != TEXT("compatible");
    auto State = MakeShared<FGPAACloudHttpState>();
    // Ask Windows for an available ephemeral port; random ports can fall into
    // excluded ranges or collide with another local editor/test process.
    auto* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    FSocket* Probe = Sockets->CreateSocket(NAME_Stream, TEXT("GPAA test port"), false);
    auto Address = Sockets->CreateInternetAddr();
    bool ValidAddress = false;
    Address->SetIp(TEXT("127.0.0.1"), ValidAddress);
    Address->SetPort(0);
    const bool Bound = Probe && ValidAddress && Probe->Bind(*Address);
    const int32 Port = Bound ? Probe->GetPortNo() : 0;
    if (Probe)
    {
        Probe->Close();
        Sockets->DestroySocket(Probe);
    }
    if (!Port)
    {
        AddError(TEXT("Could not allocate an ephemeral loopback test port"));
        return false;
    }
    State->Router = FHttpServerModule::Get().GetHttpRouter(Port, true);
    if (!State->Router)
    {
        AddError(TEXT("Could not bind loopback test provider"));
        return false;
    }
    State->Route = State->Router->BindRoute(
        FHttpPath(Responses ? TEXT("/v1/responses") : TEXT("/v1/chat/completions")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda(
            [State, Responses](const FHttpServerRequest &Request, const FHttpResultCallback &Reply)
            {
                FUTF8ToTCHAR Utf8(reinterpret_cast<const ANSICHAR *>(Request.Body.GetData()), Request.Body.Num());
                TSharedPtr<FJsonObject> Json;
                bool Parsed = FJsonSerializer::Deserialize(
                    TJsonReaderFactory<>::Create(FString(Utf8.Length(), Utf8.Get())), Json);
                FString Model, Prompt;
                bool Stream = false, Store = true;
                bool Authenticated = false;
                for (const auto &Header : Request.Headers)
                    if (Header.Key.Equals(TEXT("authorization"), ESearchCase::IgnoreCase) &&
                        Header.Value.Contains(TEXT("Bearer not-a-real-api-key")))
                        Authenticated = true;
                State->Contract = Parsed && Json && Json->TryGetStringField(TEXT("model"), Model) &&
                                  Model == TEXT("fake-model") && Json->TryGetBoolField(TEXT("stream"), Stream) &&
                                  Stream && Authenticated;
                if (State->Contract && Responses)
                    State->Contract = Json->TryGetStringField(TEXT("input"), Prompt) &&
                                      Prompt == TEXT("fixture prompt") && Json->TryGetBoolField(TEXT("store"), Store) &&
                                      !Store;
                else if (State->Contract)
                {
                    const TArray<TSharedPtr<FJsonValue>> *Messages = nullptr;
                    State->Contract = Json->TryGetArrayField(TEXT("messages"), Messages) && Messages->Num() == 1 &&
                                      (*Messages)[0]->AsObject()->TryGetStringField(TEXT("content"), Prompt) &&
                                      Prompt == TEXT("fixture prompt");
                }
                const FString Body =
                    Responses ? TEXT("data: {\"type\":\"response.output_text.delta\",\"delta\":\"Hello from the "
                                     "simulated provider.\"}\n\ndata: {\"type\":\"response.completed\"}\n\n")
                              : TEXT("data: {\"choices\":[{\"delta\":{\"content\":\"Hello from the simulated "
                                     "provider.\"}}]}\n\ndata: [DONE]\n\n");
                Reply(FHttpServerResponse::Create(Body, FString(TEXT("text/event-stream"))));
                return true;
            }));
    FHttpServerModule::Get().StartAllListeners();
    State->Provider = MakeShared<FGPAACloudChat, ESPMode::ThreadSafe>();
    FGPAAChatRequest Request;
    Request.Endpoint = FString::Printf(TEXT("http://127.0.0.1:%d/v1"), Port);
    Request.Model = TEXT("fake-model");
    Request.Key = TEXT("not-a-real-api-key");
    Request.Prompt = TEXT("fixture prompt");
    Request.Timeout = 10;
    Request.Responses = Responses;
    State->Provider->Start(
        Request, [State](const FString &Delta) { State->Text += Delta; },
        [State](bool Ok, const FString &Error)
        {
            State->Ok = Ok;
            State->Error = Error;
            State->Done = true;
        });
    ADD_LATENT_AUTOMATION_COMMAND(FGPAAWaitForCloudHttp(State, FPlatformTime::Seconds() + 15, this));
    return true;
}

namespace
{
class FTestEmbeddings : public IGPAAEmbeddingProvider
{
  public:
    int32 Calls = 0;
    FString Revision = TEXT("test384-v1");
    virtual FString Identity() const override
    {
        return Revision;
    }
    virtual bool Encode(const FString &Text, TArray<float> &Result, FString &Error) override
    {
        ++Calls;
        Result.Init(0, 384);
        Result[Text.Contains(TEXT("stamina")) ? 0 : 1] = 1;
        return true;
    }
};
class FTestRepository : public IGPAAKnowledgeRepository
{
  public:
    FGPAASnapshot Snapshot;
    int32 Writes = 0;
    virtual bool Load(FGPAASnapshot &Result, FString &Error) override
    {
        Result = Snapshot;
        return true;
    }
    virtual bool Replace(const FGPAASnapshot &Value, FString &Error) override
    {
        Snapshot = Value;
        ++Writes;
        return true;
    }
};
} // namespace
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAANativeIndexTest, "GPAA.Native.Core.IncrementalCancellationAndModelIdentity",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGPAANativeIndexTest::RunTest(const FString &Parameters)
{
    FTestRepository Store;
    FTestEmbeddings Model;
    FGPAAIndexReport Report;
    FString Error;
    auto Task = MakeShared<FGPAATask, ESPMode::ThreadSafe>();
    FGPAADocument Doc;
    Doc.Path = TEXT("Source/Stamina.cpp");
    Doc.Content = TEXT("stamina recovery");
    TestTrue(TEXT("Initial indexing"), FGPAACore::Index({Doc}, Model, Store, Task, Report, Error));
    int32 Calls = Model.Calls;
    TestTrue(TEXT("Incremental indexing"), FGPAACore::Index({Doc}, Model, Store, Task, Report, Error));
    TestEqual(TEXT("Unchanged files reuse embeddings"), Model.Calls, Calls);
    TestEqual(TEXT("One file reused"), Report.Reused, 1);
    Task->Cancelled.Store(true);
    int32 Writes = Store.Writes;
    Doc.Content = TEXT("changed");
    TestFalse(TEXT("Cancellation rejects replacement"), FGPAACore::Index({Doc}, Model, Store, Task, Report, Error));
    TestEqual(TEXT("Cancellation retains prior index"), Store.Writes, Writes);
    Task->Cancelled.Store(false);
    Model.Revision = TEXT("test384-v2");
    TestTrue(TEXT("Changed model rebuilds vectors"), FGPAACore::Index({Doc}, Model, Store, Task, Report, Error));
    TestEqual(TEXT("New identity recorded"), Store.Snapshot.Identity, Model.Revision);
    TestEqual(TEXT("Nothing reused across models"), Report.Reused, 0);
    TestTrue(TEXT("Removed files update index"), FGPAACore::Index({}, Model, Store, Task, Report, Error));
    TestEqual(TEXT("Removed count"), Report.Removed, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAANativeSecretTest, "GPAA.Native.Secrets.CredentialIsolation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGPAANativeSecretTest::RunTest(const FString &Parameters)
{
    FGPAANativeSecrets Secrets;
    FString Error;
    const FString Scope = TEXT("GPAA/Test/") + FGuid::NewGuid().ToString();
    TestTrue(TEXT("Store disposable test credential"), Secrets.Write(Scope, TEXT("not-a-real-api-key"), Error));
    TestEqual(TEXT("Read credential"), Secrets.Read(Scope), FString(TEXT("not-a-real-api-key")));
    TestTrue(TEXT("Remove test credential"), Secrets.Remove(Scope, Error));
    TestTrue(TEXT("Removed credential is absent"), Secrets.Read(Scope).IsEmpty());
    TestTrue(TEXT("Provider credentials are scoped"), FGPAANativeSecrets::Scope(TEXT("https://one.example/v1")) !=
                                                          FGPAANativeSecrets::Scope(TEXT("https://two.example/v1")));
    const FProperty *Key = FindFProperty<FProperty>(UGPAAEditorSettings::StaticClass(), TEXT("OpenAIApiKey"));
    TestTrue(TEXT("API keys are transient and not config properties"),
             Key && Key->HasAnyPropertyFlags(CPF_Transient) && !Key->HasAnyPropertyFlags(CPF_Config));
    auto *Settings = NewObject<UGPAAEditorSettings>();
    const FString ExportPath =
        FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("GPAA/test-profile.json"));
    Settings->OpenAIApiKey = TEXT("not-a-real-secret");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(ExportPath), true);
    TestTrue(TEXT("Export selected destination"), Settings->ExportSettingsProfileToFile(ExportPath));
    FString Export;
    FFileHelper::LoadFileToString(Export, *ExportPath);
    TestFalse(TEXT("Profiles always exclude keys"), Export.Contains(TEXT("not-a-real-secret")));
    auto* Imported = NewObject<UGPAAEditorSettings>();
    Imported->bCloudContextAccepted = true;
    Imported->OpenAIModel = TEXT("different-model");
    TestTrue(TEXT("Import selected profile"), Imported->ImportSettingsProfileFromFile(ExportPath));
    TestEqual(TEXT("Model round trip"), Imported->OpenAIModel, Settings->OpenAIModel);
    TestFalse(TEXT("Import resets cloud consent"), Imported->bCloudContextAccepted);
    TestTrue(TEXT("Import does not populate a key"), Imported->OpenAIApiKey.IsEmpty());
    FFileHelper::SaveStringToFile(TEXT("invalid JSON"), *ExportPath);
    TestFalse(TEXT("Malformed profiles are rejected"), Imported->ImportSettingsProfileFromFile(ExportPath));
    TestEqual(TEXT("Malformed import preserves settings"), Imported->OpenAIModel, Settings->OpenAIModel);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAANativeSseTest, "GPAA.Native.Cloud.StreamingBoundaries",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGPAANativeSseTest::RunTest(const FString &Parameters)
{
    FGPAASseDecoder Decoder;
    FString Error, Text;
    const FString Stream = TEXT(": heartbeat\r\n\r\ndata: {\"type\":\"response.output_text.delta\",\"delta\":\"Héllo "
                                "世界\"}\r\n\r\ndata: {\"type\":\"response.completed\"}\n\n");
    FTCHARToUTF8 Utf8(*Stream);
    for (int32 I = 0; I < Utf8.Length(); ++I)
    {
        TArray<FString> Deltas;
        TestTrue(TEXT("Each byte boundary parses"),
                 Decoder.Feed(reinterpret_cast<const uint8 *>(Utf8.Get()) + I, 1, Deltas, Error));
        for (const auto &Delta : Deltas)
            Text += Delta;
    }
    TestEqual(TEXT("UTF8 streamed text"), Text, FString(TEXT("Héllo 世界")));
    TestTrue(TEXT("Explicit completion required"), Decoder.IsComplete());
    FGPAASseDecoder Failed;
    TArray<FString> Deltas;
    const auto Failure = StringCast<UTF8CHAR>(TEXT("data: {\"type\":\"response.incomplete\"}\n\n"));
    TestFalse(TEXT("Incomplete response is an error"),
              Failed.Feed(reinterpret_cast<const uint8 *>(Failure.Get()), Failure.Length(), Deltas, Error));
    FGPAASseDecoder Compatible;
    const FTCHARToUTF8 Chat(TEXT("data: {\"choices\":[{\"delta\":{\"content\":\"OK\"}}]}\n\ndata: [DONE]\n\n"));
    Deltas.Reset();
    TestTrue(TEXT("Compatible chat parses"),
             Compatible.Feed(reinterpret_cast<const uint8 *>(Chat.Get()), Chat.Length(), Deltas, Error));
    TestTrue(TEXT("Compatible stream completed"), Compatible.IsComplete());
    TestEqual(TEXT("Compatible delta"), FString::Join(Deltas, TEXT("")), FString(TEXT("OK")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAANativeStorageTest, "GPAA.Native.Storage.AtomicRollbackAndHistory",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGPAANativeStorageTest::RunTest(const FString &Parameters)
{
    FString Error;
    FGPAANativeStorage Store(FPaths::ProjectSavedDir() / TEXT("GPAA/Tests") / FGuid::NewGuid().ToString() /
                             TEXT("index.sqlite"));
    FGPAASnapshot Snapshot;
    Snapshot.Identity = TEXT("test384");
    FGPAAStoredFile File;
    File.Hash = TEXT("content-hash");
    FGPAAPassage P;
    P.Path = TEXT("Source/O'Brien.cpp");
    P.Kind = TEXT("Code");
    P.Content = TEXT("quoted ' data");
    P.Vector.Init(0, 384);
    P.Vector[0] = 1;
    File.Passages.Add(P);
    Snapshot.Files.Add(P.Path, File);
    if (!TestTrue(TEXT("Atomic index write"), Store.Replace(Snapshot, Error)))
    {
        AddError(Error);
        return false;
    }
    FGPAASnapshot Loaded;
    if (!TestTrue(TEXT("Read index"), Store.Load(Loaded, Error)))
    {
        AddError(Error);
        return false;
    }
    TestEqual(TEXT("Bound SQL preserves quoted paths"), Loaded.Files.Num(), 1);
    Snapshot.Files[P.Path].Passages[0].Vector.SetNum(2);
    TestFalse(TEXT("Invalid vectors reject transaction"), Store.Replace(Snapshot, Error));
    Error.Reset();
    Loaded = {};
    TestTrue(TEXT("Prior snapshot remains readable"), Store.Load(Loaded, Error));
    if (!Loaded.Files.Contains(P.Path) || Loaded.Files[P.Path].Passages.IsEmpty())
    {
        AddError(TEXT("Rollback lost original passages"));
        return false;
    }
    TestEqual(TEXT("Rollback preserves vector dimensions"), Loaded.Files[P.Path].Passages[0].Vector.Num(), 384);
    TestTrue(TEXT("History write"), Store.AddHistory(TEXT("Where?"), TEXT("Here."), false, Error));
    TestTrue(TEXT("Partial responses marked interrupted"), Store.History(Error).Contains(TEXT("[interrupted]")));
    FGPAADocument Long;
    Long.Path = TEXT("long.cpp");
    Long.Content = FString::ChrN(4000, 'a') + TEXT("TAIL");
    const auto Chunks = FGPAACore::Chunk(Long, 40, 0, 1000);
    FString Recombined;
    for (const auto &Chunk : Chunks)
    {
        Recombined += Chunk.Content;
        TestTrue(TEXT("Chunk respects size"), Chunk.Content.Len() <= 1000);
    }
    TestTrue(TEXT("Long lines retain tail"), Recombined.Contains(TEXT("TAIL")));
    return true;
}

struct FGPAANativeWorkflowState
{
    bool Done = false, Ok = false;
    FString Error;
    FGPAANativeResult Result;
};
struct FGPAABenchmarkState
{
    bool Done = false, Ok = false;
    FString Error;
    int32 Reused = 0;
    double Indexed = 0, Queried = 0;
};
DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FGPAAWaitForBenchmark, TSharedPtr<FGPAABenchmarkState>, State, double,
                                                 Deadline, FAutomationTestBase *, Test);
bool FGPAAWaitForBenchmark::Update()
{
    if (!State->Done && FPlatformTime::Seconds() < Deadline)
        return false;
    Test->TestTrue(TEXT("Synthetic 1000-file benchmark completes"), State->Done && State->Ok);
    Test->TestEqual(TEXT("Second query reuses all file embeddings"), State->Reused, 1000);
    Test->TestTrue(TEXT("Synthetic initial index below five-minute target"), State->Indexed < 300);
    Test->TestTrue(TEXT("Synthetic incremental search below five-second target"), State->Queried < 5);
    const FString Evidence = FString::Printf(
        TEXT("{\"syntheticFiles\":1000,\"initialIndexSeconds\":%f,\"incrementalSearchSeconds\":%f,\"filesReused\":%d}"),
        State->Indexed, State->Queried, State->Reused);
    FFileHelper::SaveStringToFile(Evidence, *(FPaths::ProjectSavedDir() / TEXT("GPAA/native-benchmark.json")));
    Test->AddInfo(Evidence);
    if (!State->Error.IsEmpty() && !State->Ok)
        Test->AddError(State->Error);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAANativeBenchmarkTest, "GPAA.Native.Performance.Synthetic1000Files",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGPAANativeBenchmarkTest::RunTest(const FString &Parameters)
{
    const FString Root = FPaths::ProjectSavedDir() / TEXT("GPAA/Benchmarks") / FGuid::NewGuid().ToString();
    IFileManager::Get().MakeDirectory(*(Root / TEXT("Source")), true);
    const TArray<FString> Names = {TEXT("Stamina"), TEXT("Perception"), TEXT("Inventory"), TEXT("SaveGame"),
                                   TEXT("Material")};
    const TArray<FString> Texts = {
        TEXT("StaminaComponent regenerates stamina after a cooldown. Recovery adds stamina each second."),
        TEXT("Enemy perception detects hostile actors with sight and hearing."),
        TEXT("Inventory stores weapons and equipment. Add items to player inventory."),
        TEXT("Save game serializes player progress and restores checkpoints."),
        TEXT("Material instances configure texture samples and surface roughness.")};
    for (int32 I = 0; I < 1000; ++I)
        FFileHelper::SaveStringToFile(
            TEXT("// ") + Texts[I % 5] + TEXT("\nvoid UpdateComponent() {}"),
            *(Root / TEXT("Source") / FString::Printf(TEXT("%s_%04d.cpp"), *Names[I % 5], I)));
    auto State = MakeShared<FGPAABenchmarkState>();
    const double Start = FPlatformTime::Seconds();
    FGPAANativeRuntime::Get()->Run(
        Root, TEXT(""), TEXT(""), false, true, [](const FString &) {}, [](const FString &) {},
        [State, Root, Start](bool Ok, const FString &Error, const FGPAANativeResult &Result)
        {
            State->Indexed = FPlatformTime::Seconds() - Start;
            if (!Ok)
            {
                State->Done = true;
                State->Error = Error;
                return;
            }
            const double QueryStart = FPlatformTime::Seconds();
            FGPAANativeRuntime::Get()->Run(
                Root, TEXT(""), TEXT("stamina regeneration after cooldown"), false, false, [](const FString &) {},
                [](const FString &) {},
                [State, QueryStart](bool SearchOk, const FString &SearchError, const FGPAANativeResult &Search)
                {
                    State->Queried = FPlatformTime::Seconds() - QueryStart;
                    State->Reused = Search.Statistics.FilesReused;
                    State->Ok = SearchOk && !Search.Sources.IsEmpty() &&
                                Search.Sources[0].RelativePath.Contains(TEXT("Stamina"));
                    State->Error = SearchError;
                    State->Done = true;
                });
        });
    ADD_LATENT_AUTOMATION_COMMAND(FGPAAWaitForBenchmark(State, FPlatformTime::Seconds() + 120, this));
    return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FGPAAWaitForNativeWorkflow, TSharedPtr<FGPAANativeWorkflowState>,
                                                 State, double, Deadline, FAutomationTestBase *, Test);
bool FGPAAWaitForNativeWorkflow::Update()
{
    if (!State->Done && FPlatformTime::Seconds() < Deadline)
        return false;
    Test->TestTrue(TEXT("Native search completes without a backend"), State->Done && State->Ok);
    if (!State->Error.IsEmpty() && !State->Ok)
        Test->AddError(State->Error);
    Test->TestTrue(TEXT("Native search returns cited source"),
                   State->Result.Sources.Num() > 0 && State->Result.Sources[0].RelativePath.Contains(TEXT("Stamina")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAANativeWorkflowTest, "GPAA.Native.Workflow.OfflineIndexAndSearch",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGPAANativeWorkflowTest::RunTest(const FString &Parameters)
{
    const FString Root = FPaths::ProjectSavedDir() / TEXT("GPAA/Fixtures") / FGuid::NewGuid().ToString();
    IFileManager::Get().MakeDirectory(*(Root / TEXT("Source")), true);
    FFileHelper::SaveStringToFile(
        TEXT("// StaminaComponent regenerates stamina after a cooldown.\nvoid RegenerateStamina() {}"),
        *(Root / TEXT("Source/Stamina.cpp")));
    FFileHelper::SaveStringToFile(
        TEXT("// InventoryComponent adds and removes weapons and equipment.\nvoid AddInventoryItem() {}"),
        *(Root / TEXT("Source/Inventory.cpp")));
    auto State = MakeShared<FGPAANativeWorkflowState>();
    FGPAANativeRuntime::Get()->Run(
        Root, TEXT(""), TEXT("Where does stamina regenerate?"), false, false, [](const FString &) {},
        [](const FString &) {},
        [State](bool Ok, const FString &Error, const FGPAANativeResult &Result)
        {
            State->Ok = Ok;
            State->Error = Error;
            State->Result = Result;
            State->Done = true;
        });
    TestFalse(TEXT("Periodic refresh cannot preempt a foreground search"),
        FGPAANativeRuntime::Get()->RefreshIfIdle(Root, TEXT("")));
    ADD_LATENT_AUTOMATION_COMMAND(FGPAAWaitForNativeWorkflow(State, FPlatformTime::Seconds() + 60, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAANativeEmbeddingsTest, "GPAA.Native.Embeddings.Corpus",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGPAANativeEmbeddingsTest::RunTest(const FString &Parameters)
{
    FGPAAMiniLM Model(IPluginManager::Get().FindPlugin(TEXT("GameProductionAIAssistant"))->GetBaseDir());
    FString Error;
    TArray<int64> Tokens, Accented;
    TestTrue(TEXT("WordPiece tokenizer loads"), Model.Tokenize(TEXT("Hello, world!"), Tokens, Error));
    TestTrue(TEXT("BERT tokenizer matches known IDs"), Tokens == TArray<int64>({101, 7592, 1010, 2088, 999, 102}));
    Model.Tokenize(TEXT("Héllo, world!"), Accented, Error);
    TestTrue(TEXT("Lowercase and accent normalization match"), Tokens == Accented);
    const TArray<FString> Documents = {
        TEXT("StaminaComponent restores stamina after a cooldown. RegenerateStamina adds recovery per second."),
        TEXT("EnemyPerceptionComponent detects nearby enemies using sight radius and hearing stimuli."),
        TEXT("InventoryComponent stores items and equipment. AddItem puts a weapon into the inventory."),
        TEXT("Texture metadata: T_Rock_Normal is a normal map texture under Content/Environment/Rocks."),
        TEXT("SaveGameSubsystem serializes player progress and restores checkpoints from a save slot.")};
    const TArray<FString> Queries = {TEXT("Where does stamina recover?"), TEXT("How are enemies detected?"),
                                     TEXT("Where can I add an inventory item?"), TEXT("Find the rock normal texture"),
                                     TEXT("How is player progress saved?")};
    TArray<TArray<float>> Vectors;
    const double Start = FPlatformTime::Seconds();
    for (const FString &Document : Documents)
    {
        TArray<float> V;
        if (!Model.Encode(Document, V, Error))
        {
            AddError(Error);
            return false;
        }
        TestEqual(TEXT("Embedding dimensions"), V.Num(), 384);
        Vectors.Add(MoveTemp(V));
    }
    int32 Correct = 0;
    for (int32 Q = 0; Q < Queries.Num(); ++Q)
    {
        TArray<float> V;
        if (!Model.Encode(Queries[Q], V, Error))
        {
            AddError(Error);
            return false;
        }
        int32 Best = 0;
        for (int32 I = 1; I < Vectors.Num(); ++I)
            if (FGPAACore::Similarity(V, Vectors[I]) > FGPAACore::Similarity(V, Vectors[Best]))
                Best = I;
        if (Best == Q)
            ++Correct;
        TestEqual(*Queries[Q], Best, Q);
    }
    const FString Summary = FString::Printf(TEXT("{\"queries\":5,\"recallAt1\":%f,\"seconds\":%f,\"identity\":\"%s\"}"),
                                            Correct / 5.0, FPlatformTime::Seconds() - Start, *Model.Identity());
    IFileManager::Get().MakeDirectory(*FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GPAA")), true);
    FFileHelper::SaveStringToFile(Summary,
                                  *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GPAA/embedding-prototype.json")));
    AddInfo(Summary);
    return true;
}
#endif
