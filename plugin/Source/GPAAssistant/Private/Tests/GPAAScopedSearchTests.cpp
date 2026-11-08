// Copyright Game Production AI Assistant contributors 2026.
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Application/GPAANativeRuntime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

struct FGPAAScopedSearchState { bool Done=false, Ok=false; FString Error; TArray<FGPAASourceReference> Sources; };
DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FGPAAWaitForScopedSearch, TSharedPtr<FGPAAScopedSearchState>, State,
    double, Deadline, FAutomationTestBase*, Test);
bool FGPAAWaitForScopedSearch::Update()
{
    if (!State->Done && FPlatformTime::Seconds() < Deadline) return false;
    Test->TestTrue(TEXT("Scoped search completes"), State->Done && State->Ok);
    Test->TestEqual(TEXT("Only the matching asset is retrieved, despite code in the index"), State->Sources.Num(), 1);
    for (const auto& Source : State->Sources)
        Test->TestEqual(TEXT("No code leaks into asset-only retrieval"), Source.SourceType, FString(TEXT("Asset")));
    if (!State->Error.IsEmpty()) Test->AddError(State->Error);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAAScopedSearchTest, "GPAA.Native.Workflow.ScopedAssetSearch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGPAAScopedSearchTest::RunTest(const FString& Parameters)
{
    const FString Root = FPaths::ProjectSavedDir() / TEXT("GPAA/ScopedSearch") / FGuid::NewGuid().ToString();
    IFileManager::Get().MakeDirectory(*(Root / TEXT("Source")), true);
    FFileHelper::SaveStringToFile(TEXT("// Stamina regeneration after a cooldown."), *(Root / TEXT("Source/Stamina.cpp")));
    const FString Metadata = TEXT("{\"assets\":[{\"objectPath\":\"/Game/Stamina.Stamina\",\"assetName\":\"Stamina regeneration\"}]}");
    auto State = MakeShared<FGPAAScopedSearchState>();
    FGPAANativeRuntime::Get()->Run(Root, Metadata, TEXT("stamina regeneration"), false, false,
        [](const FString&) {}, [](const FString&) {},
        [State](bool Ok, const FString& Message, const FGPAANativeResult& Result)
        { State->Ok=Ok; if(!Ok) State->Error=Message; State->Sources=Result.Sources; State->Done=true; }, TEXT("Assets"));
    ADD_LATENT_AUTOMATION_COMMAND(FGPAAWaitForScopedSearch(State, FPlatformTime::Seconds()+60, this));
    return true;
}
#endif
