// Copyright Game Production AI Assistant contributors 2026.
#include "Settings/GPAAEditorSettings.h"
#include "Application/GPAANativeRuntime.h"
#include "Infrastructure/GPAANativeStorage.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"

UGPAAEditorSettings::UGPAAEditorSettings()
{
    ChatProvider = TEXT("OpenAI");
    OpenAIBaseUrl = TEXT("https://api.openai.com/v1");
    OpenAIModel = TEXT("gpt-4.1-mini");
    SourceOpenerMode = TEXT("Auto");
    CustomSourceOpenerCommand = TEXT("code -g \"{file}:{line}\"");
}
FString UGPAAEditorSettings::GetResolvedProjectRoot() const
{
    return ProjectRootOverride.IsEmpty() ? FPaths::ConvertRelativePathToFull(FPaths::ProjectDir())
                                         : FPaths::ConvertRelativePathToFull(ProjectRootOverride);
}
void UGPAAEditorSettings::SavePersonalKey()
{
    const FString Key = OpenAIApiKey.TrimStartAndEnd();
    if (Key.IsEmpty() || Key.StartsWith(TEXT("http")) || Key.Contains(TEXT("\n")) || Key.Contains(TEXT("\r")))
    {
        LastKeyResult = TEXT("Enter a single-line secret API key, not a website URL.");
        return;
    }
    FGPAANativeSecrets Store;
    FString Error;
    if (Store.Write(FGPAANativeSecrets::Scope(OpenAIBaseUrl), Key, Error))
    {
        OpenAIApiKey.Reset();
        LastKeyResult = TEXT("Personal key saved in Windows Credential Manager.");
    }
    else
        LastKeyResult = Error;
}
void UGPAAEditorSettings::RemovePersonalKey()
{
    FGPAANativeSecrets Store;
    FString Error;
    OpenAIApiKey.Reset();
    LastKeyResult =
        Store.Remove(FGPAANativeSecrets::Scope(OpenAIBaseUrl), Error) ? TEXT("Personal key removed.") : Error;
}
FGPAAChatSettings UGPAAEditorSettings::ToChatSettings() const
{
    FGPAAChatSettings Result;
    Result.Provider = ChatProvider;
    Result.OpenAIBaseUrl = OpenAIBaseUrl;
    Result.OpenAIModel = OpenAIModel;
    Result.TimeoutSeconds = TimeoutSeconds;
    Result.MaxOutputTokens = MaxOutputTokens;
    return Result;
}
void UGPAAEditorSettings::ApplyChatSettings(const FGPAAChatSettings &Value)
{
    ChatProvider = Value.Provider;
    OpenAIBaseUrl = Value.OpenAIBaseUrl;
    OpenAIModel = Value.OpenAIModel;
    TimeoutSeconds = FMath::Clamp(Value.TimeoutSeconds, 5, 600);
    MaxOutputTokens = FMath::Clamp(Value.MaxOutputTokens, 64, 32768);
    SaveConfig();
}
bool UGPAAEditorSettings::ChooseProfileFile(bool Save, FString& Path)
{
    auto* Desktop = FDesktopPlatformModule::Get();
    if (!Desktop || FApp::IsUnattended())
    {
        LastProfileResult = TEXT("File dialogs are unavailable in this session.");
        return false;
    }
    TArray<FString> Files;
    const void* Parent = FSlateApplication::IsInitialized()
        ? FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr) : nullptr;
    const FString Directory = LastProfileDirectory.IsEmpty() ? FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()) : LastProfileDirectory;
    const FString Filter = TEXT("GPAA settings profile (*.json)|*.json");
    const bool Chosen = Save
        ? Desktop->SaveFileDialog(Parent, TEXT("Export Settings Profile"), Directory, TEXT("GPAASettingsProfile.json"), Filter, 0, Files)
        : Desktop->OpenFileDialog(Parent, TEXT("Import Settings Profile"), Directory, TEXT(""), Filter, 0, Files);
    if (!Chosen || Files.Num() != 1)
    {
        LastProfileResult = Save ? TEXT("Export cancelled.") : TEXT("Import cancelled.");
        return false;
    }
    Path = FPaths::ConvertRelativePathToFull(Files[0]);
    if (Save && FPaths::GetExtension(Path).IsEmpty()) Path += TEXT(".json");
    LastProfileDirectory = FPaths::GetPath(Path);
    return true;
}
void UGPAAEditorSettings::ExportSettingsProfile()
{
    FString Path;
    if (ChooseProfileFile(true, Path)) ExportSettingsProfileToFile(Path);
}
void UGPAAEditorSettings::ImportSettingsProfile()
{
    FString Path;
    if (ChooseProfileFile(false, Path)) ImportSettingsProfileFromFile(Path);
}
bool UGPAAEditorSettings::ExportSettingsProfileToFile(const FString& Path)

{
    auto Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("chatProvider"), ChatProvider);
    Json->SetStringField(TEXT("openAIBaseUrl"), OpenAIBaseUrl);
    Json->SetStringField(TEXT("openAIModel"), OpenAIModel);
    Json->SetNumberField(TEXT("timeoutSeconds"), TimeoutSeconds);
    Json->SetNumberField(TEXT("maxOutputTokens"), MaxOutputTokens);
    Json->SetStringField(TEXT("sourceOpenerMode"), SourceOpenerMode);
    Json->SetStringField(TEXT("customSourceOpenerCommand"), CustomSourceOpenerCommand);
    Json->SetBoolField(TEXT("includesSecrets"), false);
    FString Output;
    FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Output));
    const bool Saved = !Path.IsEmpty() && FFileHelper::SaveStringToFile(Output, *Path);
    LastProfileResult = Saved ? TEXT("Profile exported without keys or cloud consent.") : TEXT("Could not export profile.");
    return Saved;
}
bool UGPAAEditorSettings::ImportSettingsProfileFromFile(const FString& Path)
{
    FString Input;
    TSharedPtr<FJsonObject> Json;
    if (Path.IsEmpty() || IFileManager::Get().FileSize(*Path) > 1024 * 1024 || !FFileHelper::LoadFileToString(Input, *Path) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input), Json) || !Json)
    {
        LastProfileResult = TEXT("Select a valid settings profile (at most 1 MiB).");
        return false;
    }
    auto Read = [&](const TCHAR *Name, FString &Value)
    {
        FString Candidate;
        if (Json->TryGetStringField(Name, Candidate))
            Value = Candidate;
    };
    Read(TEXT("chatProvider"), ChatProvider);
    Read(TEXT("openAIBaseUrl"), OpenAIBaseUrl);
    Read(TEXT("openAIModel"), OpenAIModel);
    Read(TEXT("sourceOpenerMode"), SourceOpenerMode);
    Read(TEXT("customSourceOpenerCommand"), CustomSourceOpenerCommand);
    int32 Value;
    if (Json->TryGetNumberField(TEXT("timeoutSeconds"), Value))
        TimeoutSeconds = FMath::Clamp(Value, 5, 600);
    if (Json->TryGetNumberField(TEXT("maxOutputTokens"), Value))
        MaxOutputTokens = FMath::Clamp(Value, 64, 32768);
    bCloudContextAccepted = false;
    SaveConfig();
    LastProfileResult =
        TEXT("Profile imported. Keys were ignored; review the provider and enable cloud consent again.");
    return true;
}
