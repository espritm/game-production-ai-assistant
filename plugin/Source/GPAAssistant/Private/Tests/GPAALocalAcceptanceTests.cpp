// Copyright Game Production AI Assistant contributors 2026.
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "GPAALocalChat.h"
#include "GPAALocalModels.h"
#include "Application/GPAANativeRuntime.h"
#include "Infrastructure/GPAANativeStorage.h"
#include "Settings/GPAAEditorSettings.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
struct FGPAALocalAcceptanceState
{
    FString Directory, ModelPath, Answer, Error;
    TSharedPtr<FGPAALocalChat, ESPMode::ThreadSafe> Provider;
    TArray<TSharedPtr<FJsonValue>> Results;
    double Deadline = 0, RequestStarted = 0, CancelledAt = 0;
    int32 Phase = 0, Case = 0, LateCallbacks = 0;
    bool Done = false, Ok = false, CancelSent = false;
    FString SavedProvider, SavedModelFile;
    int32 SavedTokens = 0;
    bool SavedConsent = false;
};
namespace
{
void RestoreLocalSettings(const TSharedPtr<FGPAALocalAcceptanceState>& State)
{
    auto* Settings = GetMutableDefault<UGPAAEditorSettings>();
    Settings->ChatProvider = State->SavedProvider; Settings->LocalModelFile = State->SavedModelFile;
    Settings->MaxOutputTokens = State->SavedTokens; Settings->bCloudContextAccepted = State->SavedConsent;
}
FGPAAChatRequest LocalFixtureRequest(const TSharedPtr<FGPAALocalAcceptanceState>& State)
{
    FGPAAChatRequest Request;
    Request.Local = true; Request.LocalCustomModel = false; Request.LocalModelPath = State->ModelPath;
    Request.Model = TEXT("qwen2.5-coder-1.5b-q4km"); Request.Timeout = 180;
    Request.MaxOutputTokens = 256; Request.LocalContextTokens = 4096; Request.LocalThreads = 4;
    return Request;
}
void StartLocalCase(const TSharedPtr<FGPAALocalAcceptanceState>& State)
{
    State->Answer.Reset(); State->Error.Reset(); State->Done = State->Ok = false;
    FGPAAPassage Source; Source.FirstLine = 1; Source.LastLine = 3;
    FString Question;
    if (State->Case == 0 || State->Case == 2)
    {
        Source.Path = TEXT("Source/StaminaComponent.cpp");
        Source.Content = TEXT("void UStaminaComponent::Tick(float DeltaSeconds) { Stamina = FMath::Min(MaxStamina, Stamina + RegenRate * DeltaSeconds); }");
        Question = State->Case == 2 ? TEXT("Quel fichier exact gère la régénération de stamina ? Donne le chemin et une courte explication.")
            : TEXT("Which exact file implements stamina regeneration? Give its path and briefly explain it.");
    }
    else
    {
        Source.Path = TEXT("asset:/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Reload.MM_Pistol_Reload");
        Source.Content = TEXT("{\"objectPath\":\"/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Reload.MM_Pistol_Reload\",\"assetClass\":\"/Script/Engine.AnimSequence\",\"assetName\":\"MM_Pistol_Reload\"}");
        Question = TEXT("Find the pistol reload animation and give the exact object path from the source.");
    }
    auto Request = LocalFixtureRequest(State);
    if (State->Case == 3) Question = TEXT("Which exact file implements the inventory system? If no supporting source was supplied, say it was not found. Do not invent a filename.");
    Request.SourceEvidence = Source.Path + TEXT("\n") + Source.Content;
    Request.RequireProjectEvidence = true; Request.HasProjectEvidence = State->Case != 3;
    Request.Prompt = FGPAACore::Prompt(Question, State->Case == 3 ? TArray<FGPAAPassage>() : TArray<FGPAAPassage>{Source});
    State->RequestStarted = FPlatformTime::Seconds();
    State->Provider->Start(Request, [State=State](const FString& Text) { State->Answer += Text; },
        [State=State](bool Ok, const FString& Error) { State->Ok = Ok; State->Error = Error; State->Done = true; });
}
}
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FGPAAWaitForLocalAcceptance, TSharedPtr<FGPAALocalAcceptanceState>, State, FAutomationTestBase*, Test);
bool FGPAAWaitForLocalAcceptance::Update()
{
    if (FPlatformTime::Seconds() > State->Deadline)
    {
        FGPAALocalModels::Get().Cancel(); State->Provider->Cancel();
        RestoreLocalSettings(State); Test->AddError(TEXT("Local AI acceptance timed out.")); return true;
    }
    const auto& Spec = FGPAALocalModels::Catalogue()[0];
    const FString Partial = State->ModelPath + TEXT(".partial");
    if (State->Phase == 0)
    {
        if (FGPAALocalModels::Get().IsBusy())
        {
            if (!State->CancelSent && IFileManager::Get().FileSize(*Partial) >= 4 * 1024 * 1024)
            { FGPAALocalModels::Get().Cancel(); State->CancelSent = true; }
            return false;
        }
        if (State->CancelSent)
        {
            Test->TestTrue(TEXT("Cancelled download retained a partial file"), IFileManager::Get().FileSize(*Partial) > 0);
            Test->TestFalse(TEXT("Cancelled download never activated a model"), IFileManager::Get().FileExists(*State->ModelPath));
            FGPAALocalModels::Get().Download(Spec.Id, State->Directory);
            State->Phase = 1; return false;
        }
        if (!IFileManager::Get().FileExists(*State->ModelPath))
        { Test->AddError(FGPAALocalModels::Get().Status()); RestoreLocalSettings(State); return true; }
        State->Phase = 1;
    }
    if (State->Phase == 1)
    {
        if (FGPAALocalModels::Get().IsBusy()) return false;
        if (!IFileManager::Get().FileExists(*State->ModelPath))
        { Test->AddError(FGPAALocalModels::Get().Status()); RestoreLocalSettings(State); return true; }
        Test->TestEqual(TEXT("Pinned model has the full expected size"), IFileManager::Get().FileSize(*State->ModelPath), Spec.Bytes);
        StartLocalCase(State); State->Phase = 2; return false;
    }
    if (State->Phase == 2)
    {
        if (!State->Done || State->Provider->IsBusy()) return false;
        Test->TestTrue(TEXT("Local generation completed without API credentials"), State->Ok);
        if (!State->Error.IsEmpty()) Test->AddError(State->Error);
        const FString Expected = (State->Case == 0 || State->Case == 2) ? TEXT("Source/StaminaComponent.cpp") : TEXT("/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Reload.MM_Pistol_Reload");
        const bool Grounded = State->Case == 3
            ? !State->Answer.IsEmpty() && !State->Answer.Contains(TEXT(".cpp")) && !State->Answer.Contains(TEXT("/Game/"))
            : State->Answer.Contains(Expected);
        Test->TestTrue(TEXT("Answer is grounded in the supplied context"), Grounded);
        auto Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("case"), State->Case == 0 ? TEXT("code_en") : State->Case == 1 ? TEXT("asset_en") : State->Case == 2 ? TEXT("code_fr") : TEXT("no_sources"));
        Entry->SetStringField(TEXT("answer"), State->Answer);
        Entry->SetBoolField(TEXT("handledWithoutGeneration"), State->Case == 3);
        Entry->SetNumberField(TEXT("elapsedSeconds"), FPlatformTime::Seconds() - State->RequestStarted);
        Entry->SetBoolField(TEXT("groundingCheckPassed"), Grounded);
        State->Results.Add(MakeShared<FJsonValueObject>(Entry));
        if (++State->Case < 4) { StartLocalCase(State); return false; }
        State->Answer.Reset(); State->Done = false;
        auto* Settings = GetMutableDefault<UGPAAEditorSettings>();
        Settings->ChatProvider = TEXT("Local"); Settings->LocalModelFile = State->ModelPath;
        Settings->bCloudContextAccepted = false; Settings->MaxOutputTokens = 256;
        FGPAANativeRuntime::Get()->Run(FPaths::ProjectDir(), TEXT(""), TEXT("Which exact file describes stamina regeneration? Cite its path."), true, false,
            [](const FString&) {}, [State=State](const FString& Delta) { State->Answer += Delta; },
            [State=State](bool Ok, const FString& Error, const FGPAANativeResult&) { State->Ok = Ok; State->Error = Error; State->Done = true; });
        State->Phase = 3; return false;
    }
    if (State->Phase == 3)
    {
        if (!State->Done || FGPAANativeRuntime::Get()->IsBusy()) return false;
        Test->TestTrue(TEXT("Local provider integrates with native retrieval without cloud consent"), State->Ok);
        if (!State->Ok) Test->AddError(State->Error);
        FGPAANativeStorage Store(FGPAANativeRuntime::DatabasePath(FPaths::ProjectDir()));
        FString Error; const FString History = Store.History(Error);
        Test->TestTrue(TEXT("Generated local answer and model identity persisted"), History.Contains(State->Answer) && History.Contains(TEXT("Provider: Local | Model:")));
        RestoreLocalSettings(State);
        State->Done = false;
        auto Request = LocalFixtureRequest(State);
        Request.Prompt = TEXT("Write every number from 1 to 1000, one number per line."); Request.MaxOutputTokens = 512;
        State->Provider->Start(Request, [State=State](const FString&)
            { if (State->CancelledAt == 0) { State->CancelledAt = FPlatformTime::Seconds(); State->Provider->Cancel(); } else ++State->LateCallbacks; },
            [State=State](bool, const FString&) { ++State->LateCallbacks; });
        State->Phase = 4; return false;
    }
    if (State->Phase == 4)
    {
        if (State->Provider->IsBusy()) return false;
        Test->TestTrue(TEXT("Actual local token streaming was cancelled"), State->CancelledAt > 0);
        FGPAALocalChat::ShutdownAll();
        auto Late = MakeShared<FGPAALocalChat, ESPMode::ThreadSafe>();
        bool Rejected = false;
        Late->Start(LocalFixtureRequest(State), [](const FString&) {}, [&Rejected](bool Ok, const FString& Error)
            { Rejected = !Ok && Error.Contains(TEXT("shutting down")); });
        Test->TestTrue(TEXT("Late local generation rejected after shutdown"), Rejected);
        Test->TestEqual(TEXT("No callbacks delivered after cancellation"), State->LateCallbacks, 0);
        FString Json; FJsonSerializer::Serialize(State->Results, TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json, *(FPaths::ProjectSavedDir() / TEXT("GPAA/local-quality-results.json")));
        FGPAANativeRuntime::Shutdown();
        return true;
    }
    return false;
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FGPAALocalAcceptanceTest, "GPAA.Acceptance.LocalAI", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FGPAALocalAcceptanceTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
    if (FParse::Param(FCommandLine::Get(), TEXT("GPAALocalAcceptance")))
    { Names.Add(TEXT("DownloadResumeAndGeneration")); Commands.Add(TEXT("DownloadResumeAndGeneration")); }
}
bool FGPAALocalAcceptanceTest::RunTest(const FString& Parameters)
{
    auto State = MakeShared<FGPAALocalAcceptanceState>();
    if (!FParse::Value(FCommandLine::Get(), TEXT("GPAALocalCache="), State->Directory))
    { AddError(TEXT("An explicit test model cache is required.")); return false; }
    State->Directory = FPaths::ConvertRelativePathToFull(State->Directory);
    State->Provider = MakeShared<FGPAALocalChat, ESPMode::ThreadSafe>();
    State->ModelPath = FGPAALocalModels::ModelPath(FGPAALocalModels::Catalogue()[0], State->Directory);
    State->Deadline = FPlatformTime::Seconds() + 1100;
    const auto* Settings = GetDefault<UGPAAEditorSettings>();
    State->SavedProvider = Settings->ChatProvider; State->SavedModelFile = Settings->LocalModelFile;
    State->SavedTokens = Settings->MaxOutputTokens; State->SavedConsent = Settings->bCloudContextAccepted;
    if (IFileManager::Get().FileExists(*State->ModelPath))
    {
        FString Error;
        if (!FGPAALocalModels::ValidateFile(State->ModelPath, &FGPAALocalModels::Catalogue()[0], nullptr, Error))
        { AddError(Error); return false; }
        State->Phase = 1;
    }
    else
    {
        if (FParse::Param(FCommandLine::Get(), TEXT("GPAALocalOfflineOnly")))
        { AddError(TEXT("Offline-only acceptance requires an already downloaded model.")); return false; }
        if (!FGPAALocalModels::Get().Download(FGPAALocalModels::Catalogue()[0].Id, State->Directory)) return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FGPAAWaitForLocalAcceptance(State, this));
    return true;
}
#endif
