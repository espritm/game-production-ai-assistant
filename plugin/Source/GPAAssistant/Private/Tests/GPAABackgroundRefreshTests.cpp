// Copyright Game Production AI Assistant contributors 2026.
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Application/GPAANativeRuntime.h"
#include "Infrastructure/GPAANativeStorage.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FGPAAWaitForBackgroundIndex, FString, Root, double, Deadline,
    FAutomationTestBase*, Test);
bool FGPAAWaitForBackgroundIndex::Update()
{
    if (FGPAANativeRuntime::Get()->IsBusy() && FPlatformTime::Seconds() < Deadline) return false;
    Test->TestFalse(TEXT("Background index finishes"), FGPAANativeRuntime::Get()->IsBusy());
    FGPAANativeStorage Store(FGPAANativeRuntime::DatabasePath(Root));
    FGPAASnapshot Snapshot;
    FString Error;
    Test->TestTrue(TEXT("Background index persisted"), Store.Load(Snapshot, Error));
    Test->TestEqual(TEXT("Source indexed without a search or cloud request"), Snapshot.Files.Num(), 1);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAABackgroundIndexTest, "GPAA.Native.Workflow.BackgroundIndexNonPreemptive",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGPAABackgroundIndexTest::RunTest(const FString& Parameters)
{
    const FString Root = FPaths::ProjectSavedDir() / TEXT("GPAA/AutoIndex") / FGuid::NewGuid().ToString();
    IFileManager::Get().MakeDirectory(*(Root / TEXT("Source")), true);
    FFileHelper::SaveStringToFile(TEXT("// Regenerate stamina after a cooldown."), *(Root / TEXT("Source/Stamina.cpp")));
    auto Runtime = FGPAANativeRuntime::Get();
    TestTrue(TEXT("Idle background refresh starts without a panel"), Runtime->RefreshIfIdle(Root, TEXT("")));
    TestFalse(TEXT("Second timer cannot interrupt the active refresh"), Runtime->RefreshIfIdle(Root, TEXT("")));
    ADD_LATENT_AUTOMATION_COMMAND(FGPAAWaitForBackgroundIndex(Root, FPlatformTime::Seconds() + 60, this));
    return true;
}
#endif
