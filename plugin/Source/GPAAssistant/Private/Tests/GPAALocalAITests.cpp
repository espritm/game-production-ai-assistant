// Copyright Game Production AI Assistant contributors 2026.
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "GPAALocalChat.h"
#include "GPAALocalModels.h"
#include "Application/GPAANativeRuntime.h"
#include "Infrastructure/GPAANativeStorage.h"
#include "Settings/GPAAEditorSettings.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAALocalUtf8Test, "GPAA.Native.LocalAI.StreamingUtf8", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGPAALocalUtf8Test::RunTest(const FString& Parameters)
{
    const FString Expected = TEXT("Héllo 世界 🙂");
    FTCHARToUTF8 Bytes(*Expected);
    FGPAALocalUtf8 Decoder; FString Actual;
    for (int32 I = 0; I < Bytes.Length(); ++I)
    {
        FString Delta;
        TestTrue(TEXT("Split token piece is accepted"), Decoder.Append(Bytes.Get() + I, 1, Delta));
        Actual += Delta;
    }
    TestEqual(TEXT("No split UTF-8 corruption"), Actual, Expected);
    TestTrue(TEXT("All bytes consumed"), Decoder.IsEmpty());
    FGPAALocalUtf8 Invalid; FString Text;
    const char Bad[] = {static_cast<char>(0xC0), static_cast<char>(0x80)};
    TestFalse(TEXT("Overlong encodings are rejected"), Invalid.Append(Bad, 2, Text));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAALocalIntegrityTest, "GPAA.Native.LocalAI.ModelIntegrity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGPAALocalIntegrityTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Curated catalogue is bounded"), FGPAALocalModels::Catalogue().Num(), 2);
    TestTrue(TEXT("Default is a pinned public model"), FGPAALocalModels::Find(TEXT("qwen2.5-coder-1.5b-q4km")) != nullptr);
    TestTrue(TEXT("Correct resumed range accepted"), FGPAALocalModels::ValidateRange(TEXT("bytes 8-15/64"), 8, 15, 64));
    TestFalse(TEXT("Wrong offset is rejected"), FGPAALocalModels::ValidateRange(TEXT("bytes 0-7/64"), 8, 15, 64));
    TestFalse(TEXT("Wrong total is rejected"), FGPAALocalModels::ValidateRange(TEXT("bytes 8-15/65"), 8, 15, 64));
    const FString Path = FPaths::ProjectSavedDir() / TEXT("GPAA") / (FGuid::NewGuid().ToString() + TEXT(".gguf"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    TArray<uint8> Bytes; Bytes.SetNumZeroed(128); FMemory::Memcpy(Bytes.GetData(), "GGUF", 4); Bytes[4] = 3;
    FFileHelper::SaveArrayToFile(Bytes, *Path);
    FGPAALocalModelSpec Spec; Spec.Bytes = Bytes.Num(); Spec.Sha256 = TEXT("19bf4fe3c6086e969b112e223982a0a076724124824592a90d1b971051b9d042");
    auto Task = MakeShared<FGPAATask, ESPMode::ThreadSafe>(); FString Error;
    TestTrue(TEXT("Matching model bytes are accepted"), FGPAALocalModels::ValidateFile(Path, &Spec, Task, Error));
    Bytes[80] = 1; FFileHelper::SaveArrayToFile(Bytes, *Path);
    TestFalse(TEXT("Same-size corruption rejected"), FGPAALocalModels::ValidateFile(Path, &Spec, Task, Error));
    TestTrue(TEXT("User-selected GGUF gets header validation"), FGPAALocalModels::ValidateFile(Path, nullptr, Task, Error));
    Bytes[0] = 0; FFileHelper::SaveArrayToFile(Bytes, *Path);
    TestFalse(TEXT("Non-GGUF override rejected"), FGPAALocalModels::ValidateFile(Path, nullptr, Task, Error));
    FString ReferenceError;
    TestTrue(TEXT("Known reference and line range accepted"), FGPAALocalChat::ValidateReferences(TEXT("Source/StaminaComponent.cpp:1-3"), TEXT("Source/StaminaComponent.cpp"), ReferenceError));
    TestFalse(TEXT("Additional invented asset filename is flagged"), FGPAALocalChat::ValidateReferences(TEXT("/path/to/project/Pistol.animsequence"), TEXT("/Game/Pistol.Pistol"), ReferenceError));
    TestFalse(TEXT("Invented C++ basename is flagged"), FGPAALocalChat::ValidateReferences(TEXT("InventorySystem.cpp"), TEXT("Source/StaminaComponent.cpp"), ReferenceError));
    return true;
}
struct FGPAAEmptyEvidenceState
{
    TSharedPtr<FGPAALocalChat, ESPMode::ThreadSafe> Provider;
    FString Answer;
    bool Done = false, Ok = false;
    double Deadline = 0;
};
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FGPAAWaitForEmptyEvidence, TSharedPtr<FGPAAEmptyEvidenceState>, State, FAutomationTestBase*, Test);
bool FGPAAWaitForEmptyEvidence::Update()
{
    if (!State->Done && FPlatformTime::Seconds() < State->Deadline) return false;
    Test->TestTrue(TEXT("No-evidence response succeeds without loading a model"), State->Done && State->Ok);
    Test->TestTrue(TEXT("Missing project evidence yields a deterministic refusal"), State->Answer.Contains(TEXT("No matching implementation")));
    Test->TestFalse(TEXT("No invented implementation path"), State->Answer.Contains(TEXT(".cpp")));
    State->Provider->Cancel();
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAAEmptyEvidenceTest, "GPAA.Native.LocalAI.EmptyEvidenceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGPAAEmptyEvidenceTest::RunTest(const FString& Parameters)
{
    auto State = MakeShared<FGPAAEmptyEvidenceState>();
    State->Provider = MakeShared<FGPAALocalChat, ESPMode::ThreadSafe>(); State->Deadline = FPlatformTime::Seconds() + 5;
    FGPAAChatRequest Request; Request.Local = true; Request.RequireProjectEvidence = true;
    Request.Prompt = TEXT("Invent an inventory file path.");
    State->Provider->Start(Request, [State](const FString& Delta) { State->Answer += Delta; },
        [State](bool Ok, const FString&) { State->Ok = Ok; State->Done = true; });
    ADD_LATENT_AUTOMATION_COMMAND(FGPAAWaitForEmptyEvidence(State, this));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAALocalRoutingTest, "GPAA.Native.LocalAI.NoCloudFallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGPAALocalRoutingTest::RunTest(const FString& Parameters)
{
    auto* Settings = GetMutableDefault<UGPAAEditorSettings>();
    const FString PreviousProvider = Settings->ChatProvider, PreviousPath = Settings->LocalModelFile;
    const bool Consent = Settings->bCloudContextAccepted;
    Settings->ChatProvider = TEXT("Local"); Settings->bCloudContextAccepted = false;
    Settings->LocalModelFile = FPaths::ProjectSavedDir() / FGuid::NewGuid().ToString() / TEXT("missing.gguf");
    bool Called = false; FString Error;
    FGPAANativeRuntime::Get()->Run(FPaths::ProjectDir(), TEXT(""), TEXT("Question"), true, false,
        [](const FString&) {}, [](const FString&) {}, [&Called, &Error](bool Ok, const FString& Message, const FGPAANativeResult&)
        { Called = !Ok; Error = Message; });
    TestTrue(TEXT("Missing local model rejected before retrieval"), Called);
    TestTrue(TEXT("No API key or cloud consent requested"), Error.Contains(TEXT("Local model is missing")));
    Settings->ChatProvider = PreviousProvider; Settings->LocalModelFile = PreviousPath; Settings->bCloudContextAccepted = Consent;
    FGPAANativeRuntime::Shutdown();
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAALocalProfileTest, "GPAA.Native.LocalAI.PortableProfiles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGPAALocalProfileTest::RunTest(const FString& Parameters)
{
    auto* Source = NewObject<UGPAAEditorSettings>();
    Source->ChatProvider = TEXT("Local"); Source->LocalThreads = 2; Source->LocalContextTokens = 2048;
    Source->LocalModelFile = TEXT("C:/private-machine-only/model.gguf");
    const FString Path = FPaths::ProjectSavedDir() / TEXT("GPAA/local-profile.json");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    TestTrue(TEXT("Export local preferences"), Source->ExportSettingsProfileToFile(Path));
    FString Json; FFileHelper::LoadFileToString(Json, *Path);
    TestFalse(TEXT("Machine-specific model path excluded"), Json.Contains(TEXT("private-machine-only")));
    auto* Destination = NewObject<UGPAAEditorSettings>();
    TestTrue(TEXT("Import local preferences"), Destination->ImportSettingsProfileFromFile(Path));
    TestEqual(TEXT("Local provider preserved"), Destination->ChatProvider, FString(TEXT("Local")));
    TestEqual(TEXT("CPU budget preserved"), Destination->LocalThreads, 2);
    TestEqual(TEXT("Context budget preserved"), Destination->LocalContextTokens, 2048);
    return true;
}
#endif
