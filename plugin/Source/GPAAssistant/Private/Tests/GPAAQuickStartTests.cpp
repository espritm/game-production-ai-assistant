// Copyright Game Production AI Assistant contributors 2026.
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Application/GPAAQuickStart.h"
#include "Application/GPAANativeRuntime.h"
#include "Settings/GPAAEditorSettings.h"
#include "GPAALocalModels.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAASetupChoiceTest, "GPAA.Native.Setup.ExplicitProviderChoice",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGPAASetupChoiceTest::RunTest(const FString& Parameters)
{
    auto* Settings = GetMutableDefault<UGPAAEditorSettings>();
    const FString Previous = Settings->ChatProvider;
    Settings->ChatProvider.Reset();
    bool Rejected = false;
    FGPAANativeRuntime::Get()->Run(FPaths::ProjectDir(), TEXT(""), TEXT("Test"), true, false,
        [](const FString&) {}, [](const FString&) {}, [&Rejected](bool Ok, const FString& Message, const FGPAANativeResult&)
        { Rejected = !Ok && Message.Contains(TEXT("Setup required")); });
    TestTrue(TEXT("No provider choice blocks generation before contacting a provider"), Rejected);
    Settings->ChatProvider = Previous;
    FGPAANativeRuntime::Shutdown();
    auto Quick = MakeShared<FGPAAQuickStart>();
    TestTrue(TEXT("Start accepted while idle"), Quick->Start());
    TestTrue(TEXT("Only one wizard may own setup"), FGPAAQuickStart::IsRunning());
    Quick->Cancel(); Quick->Tick();
    TestFalse(TEXT("Cancel before setup prevents work"), Quick->Running());
    TestEqual(TEXT("Cancelled setup preserves provider"), Settings->ChatProvider, Previous);
    TestFalse(TEXT("Cancelled wizard releases ownership"), FGPAAQuickStart::IsRunning());
    return true;
}
struct FGPAAQuickAcceptanceState
{
    TSharedPtr<FGPAAQuickStart> Quick;
    FString Provider, Model, File;
    int32 Context = 0, Threads = 0;
    bool Consent = false;
    double Deadline = 0;
};
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FGPAAWaitQuickStart, TSharedPtr<FGPAAQuickAcceptanceState>, State, FAutomationTestBase*, Test);
bool FGPAAWaitQuickStart::Update()
{
    State->Quick->Tick();
    if (State->Quick->Running() && FPlatformTime::Seconds() < State->Deadline) return false;
    Test->TestTrue(*State->Quick->Message, State->Quick->Succeeded());
    auto* Settings = GetMutableDefault<UGPAAEditorSettings>();
    Test->TestEqual(TEXT("Quick Start selects Local"), Settings->ChatProvider, FString(TEXT("Local")));
    Test->TestTrue(TEXT("Quick Start clears custom file override"), Settings->LocalModelFile.IsEmpty());
    State->Quick->Cancel();
    Settings->ChatProvider = State->Provider; Settings->LocalModelId = State->Model; Settings->LocalModelFile = State->File;
    Settings->LocalContextTokens = State->Context; Settings->LocalThreads = State->Threads; Settings->bCloudContextAccepted = State->Consent;
    Settings->SaveConfig();
    return true;
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FGPAAQuickAcceptanceTest, "GPAA.Acceptance.QuickStart",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FGPAAQuickAcceptanceTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
    if (FParse::Param(FCommandLine::Get(), TEXT("GPAAQuickStartAcceptance")))
    { Names.Add(TEXT("CachedModelAndRealPrompt")); Commands.Add(TEXT("CachedModelAndRealPrompt")); }
}
bool FGPAAQuickAcceptanceTest::RunTest(const FString& Parameters)
{
    const auto* Model = FGPAALocalModels::Find(TEXT("qwen2.5-coder-1.5b-q4km"));
    if (!IFileManager::Get().FileExists(*FGPAALocalModels::ModelPath(*Model)))
    { AddError(TEXT("Quick Start acceptance requires an already cached Light model; no test download is authorized.")); return false; }
    auto State = MakeShared<FGPAAQuickAcceptanceState>();
    auto* Settings = GetMutableDefault<UGPAAEditorSettings>();
    State->Provider = Settings->ChatProvider; State->Model = Settings->LocalModelId; State->File = Settings->LocalModelFile;
    State->Context = Settings->LocalContextTokens; State->Threads = Settings->LocalThreads; State->Consent = Settings->bCloudContextAccepted;
    State->Quick = MakeShared<FGPAAQuickStart>(); State->Deadline = FPlatformTime::Seconds() + 180;
    if (!State->Quick->Start()) { AddError(State->Quick->Message); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FGPAAWaitQuickStart(State, this));
    return true;
}
#endif
