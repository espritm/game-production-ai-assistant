#include "Misc/AutomationTest.h"

#include "Models/GPAAApiModels.h"
#include "Presentation/GPAAKnowledgePresenter.h"
#include "Services/GPAAChatSettingsValidator.h"
#include "Services/GPAABackendSupervisor.h"
#include "Settings/GPAAEditorSettings.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#if WITH_DEV_AUTOMATION_TESTS

// Opt-in integration test: the runner observes the packaged process while this
// latent command keeps the editor alive, then verifies shutdown externally.
DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FGPAAWaitForBackend, TSharedPtr<bool>, Ready, double, Deadline, FAutomationTestBase*, Test);
bool FGPAAWaitForBackend::Update()
{
    if (FPlatformTime::Seconds() < Deadline) { return false; }
    Test->TestTrue(TEXT("Backend readiness callback completed successfully before timeout"), *Ready);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAAPackagedBackendTest,
    "GPAA.Plugin.Integration.PackagedBackend",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPAAPackagedBackendTest::RunTest(const FString& Parameters)
{
    FString Url;
    if (!FParse::Value(FCommandLine::Get(), TEXT("GPAASmokeBackendUrl="), Url))
    {
        AddInfo(TEXT("Packaged backend integration requires GPAASmokeBackendUrl; skipped."));
        return true;
    }
    UGPAAEditorSettings* Settings = GetMutableDefault<UGPAAEditorSettings>();
    Settings->BackendUrl = Url;
    Settings->BackendExecutablePath.Empty();
    Settings->BackendLaunchArguments.Empty();
    Settings->bAutoStartBackend = true;
    Settings->bStopBackendOnEditorShutdown = true;
    auto Supervisor = MakeShared<FGPAABackendSupervisor, ESPMode::ThreadSafe>();
    auto Ready = MakeShared<bool>(false);
    Supervisor->EnsureBackendReady(*Settings, FGPAARequestCompleted(),
        FGPAARequestCompleted::CreateLambda([this, Supervisor, Ready](bool bSucceeded, const FString& Message)
        {
            *Ready = bSucceeded;
            TestTrue(TEXT("Packaged backend becomes healthy"), bSucceeded);
            AddInfo(Message);
        }));
    ADD_LATENT_AUTOMATION_COMMAND(FGPAAWaitForBackend(Ready, FPlatformTime::Seconds() + 30.0, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGPAAChatSettingsValidatorTest,
    "GPAA.Plugin.ChatSettingsValidator.RejectsPlatformUrlApiKey",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPAAChatSettingsValidatorTest::RunTest(const FString& Parameters)
{
    FGPAAChatSettings Settings;
    Settings.Provider = TEXT("OpenAICompatible");
    Settings.OpenAIBaseUrl = TEXT("https://api.openai.com/v1");
    Settings.OpenAIApiKey = TEXT("https://platform.openai.com/home");

    FString Error;
    TestFalse(TEXT("A platform URL is not accepted as an API key."), FGPAAChatSettingsValidator::Validate(Settings, Error));
    TestTrue(TEXT("Validation explains the API key issue."), Error.Contains(TEXT("API key")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGPAAApiModelsSearchResponseTest,
    "GPAA.Plugin.ApiModels.ParsesSearchResponse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPAAApiModelsSearchResponseTest::RunTest(const FString& Parameters)
{
    const FString Response = TEXT("[{\"relativePath\":\"Source/Test.cpp\",\"content\":\"line one\\nline two\",\"startLine\":4,\"endLine\":6,\"score\":0.75}]");

    FGPAASearchResponse SearchResponse;
    TestTrue(TEXT("Search response parses."), FGPAAApiModels::ParseSearchResponse(Response, SearchResponse));
    TestEqual(TEXT("One result is returned."), SearchResponse.Results.Num(), 1);
    TestEqual(TEXT("Source type defaults to code."), SearchResponse.Results[0].SourceType, FString(TEXT("Code")));
    TestEqual(TEXT("Content is normalized for display."), SearchResponse.Results[0].Content, FString(TEXT("line one line two")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGPAAApiModelsChatResponseTest,
    "GPAA.Plugin.ApiModels.ParsesChatResponse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPAAApiModelsChatResponseTest::RunTest(const FString& Parameters)
{
    const FString Response = TEXT("{\"answer\":\"Use the stamina component.\",\"sources\":[{\"relativePath\":\".gpaa/unreal-assets/BP_Player.json\",\"content\":\"asset\",\"startLine\":1,\"endLine\":1,\"score\":0.5}]}");

    FGPAAChatResponse ChatResponse;
    TestTrue(TEXT("Chat response parses."), FGPAAApiModels::ParseChatResponse(Response, ChatResponse));
    TestEqual(TEXT("Answer is read."), ChatResponse.Answer, FString(TEXT("Use the stamina component.")));
    TestEqual(TEXT("Source is returned."), ChatResponse.Sources.Num(), 1);
    TestEqual(TEXT("Asset source type is inferred."), ChatResponse.Sources[0].SourceType, FString(TEXT("Asset")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGPAAKnowledgePresenterTest,
    "GPAA.Plugin.KnowledgePresenter.DescribesNoOpIndex",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPAAKnowledgePresenterTest::RunTest(const FString& Parameters)
{
    FGPAAIndexStatistics Statistics;
    Statistics.FilesScanned = 12;
    Statistics.FilesIndexed = 12;
    Statistics.FilesReused = 12;
    Statistics.ChunksReused = 30;

    TestFalse(TEXT("No work is detected when only chunks are reused."), FGPAAKnowledgePresenter::DidIndexDoWork(Statistics));
    TestTrue(TEXT("Summary mentions indexed files."), FGPAAKnowledgePresenter::FormatIndexSummary(Statistics).Contains(TEXT("Files indexed: 12")));
    return true;
}

#endif
