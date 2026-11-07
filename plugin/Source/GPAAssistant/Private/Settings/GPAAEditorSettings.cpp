// Copyright Game Production AI Assistant contributors 2026.
#include "Settings/GPAAEditorSettings.h"
#include "Application/GPAANativeRuntime.h"
#include "Infrastructure/GPAANativeStorage.h"
#include "GPAACloudChat.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
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
void UGPAAEditorSettings::CheckAssistant()
{
    LastDiagnosticsResult = TEXT("The assistant runs in Unreal. Refresh Knowledge validates local indexing. No "
                                 "companion application is required.");
}
void UGPAAEditorSettings::SavePersonalKey()
{
    const FString Key = OpenAIApiKey.TrimStartAndEnd();
    if (Key.IsEmpty() || Key.StartsWith(TEXT("http")) || Key.Contains(TEXT("\n")) || Key.Contains(TEXT("\r")))
    {
        LastDiagnosticsResult = TEXT("Enter a single-line secret API key, not a website URL.");
        return;
    }
    FGPAANativeSecrets Store;
    FString Error;
    if (Store.Write(FGPAANativeSecrets::Scope(OpenAIBaseUrl), Key, Error))
    {
        OpenAIApiKey.Reset();
        LastDiagnosticsResult = TEXT("Personal key saved in Windows Credential Manager.");
    }
    else
        LastDiagnosticsResult = Error;
}
void UGPAAEditorSettings::RemovePersonalKey()
{
    FGPAANativeSecrets Store;
    FString Error;
    OpenAIApiKey.Reset();
    LastDiagnosticsResult =
        Store.Remove(FGPAANativeSecrets::Scope(OpenAIBaseUrl), Error) ? TEXT("Personal key removed.") : Error;
}
void UGPAAEditorSettings::TestPromptIA()
{
    if (!bCloudContextAccepted)
    {
        LastDiagnosticsResult = TEXT("Enable Cloud Context Consent before testing the provider.");
        return;
    }
    if (ChatProvider != TEXT("OpenAI") && ChatProvider != TEXT("OpenAICompatible"))
    {
        LastDiagnosticsResult = TEXT("Select OpenAI or OpenAICompatible.");
        return;
    }
    FGPAAChatRequest Request;
    Request.Endpoint = OpenAIBaseUrl;
    Request.Model = OpenAIModel;
    Request.Responses = ChatProvider == TEXT("OpenAI");
    FGPAANativeSecrets Store;
    Request.Key = Store.Read(FGPAANativeSecrets::Scope(OpenAIBaseUrl));
    Request.Prompt = TEXT("Reply with OK.");
    Request.MaxOutputTokens = 64;
    Request.Timeout = TimeoutSeconds;
    auto Provider = MakeShared<FGPAACloudChat, ESPMode::ThreadSafe>();
    TWeakObjectPtr<UGPAAEditorSettings> Weak(this);
    LastDiagnosticsResult = TEXT("Testing provider (a billable API request)...");
    Provider->Start(
        Request, [](const FString &) {},
        [Weak, Provider](bool Ok, const FString &Error)
        {
            if (Weak.IsValid())
                Weak->LastDiagnosticsResult = Ok ? TEXT("Provider is ready.") : Error;
        });
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
void UGPAAEditorSettings::ExportSettingsProfile()
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
    const FString Path =
        ExportProfilePath.IsEmpty() ? FPaths::ProjectSavedDir() / TEXT("GPAASettingsProfile.json") : ExportProfilePath;
    LastDiagnosticsResult = FFileHelper::SaveStringToFile(Output, *Path)
                                ? TEXT("Profile exported without keys or cloud consent.")
                                : TEXT("Could not export profile.");
}
void UGPAAEditorSettings::ImportSettingsProfile()
{
    FString Input;
    TSharedPtr<FJsonObject> Json;
    if (!FFileHelper::LoadFileToString(Input, *ImportProfilePath) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input), Json) || !Json)
    {
        LastDiagnosticsResult = TEXT("Select a valid settings profile.");
        return;
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
    LastDiagnosticsResult =
        TEXT("Profile imported. Keys were ignored; review the provider and enable cloud consent again.");
}
void UGPAAEditorSettings::ImportLegacyChatHistory()
{
    TWeakObjectPtr<UGPAAEditorSettings> Weak(this);
    LastDiagnosticsResult = TEXT("Importing selected history...");
    FGPAANativeRuntime::Get()->ImportHistory(GetResolvedProjectRoot(), LegacyHistoryPath,
                                             [Weak](bool, const FString &Message)
                                             {
                                                 if (Weak.IsValid())
                                                     Weak->LastDiagnosticsResult = Message;
                                             });
}
