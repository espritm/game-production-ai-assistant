#include "Settings/GPAAEditorSettings.h"

#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Services/GPAABackendSupervisor.h"

namespace
{
    const TCHAR* DefaultBackendUrl = TEXT("http://localhost:5000");
    const TCHAR* DefaultChatProvider = TEXT("Ollama");
    const TCHAR* DefaultOpenAIBaseUrl = TEXT("https://api.openai.com/v1");
    const TCHAR* DefaultOpenAIModel = TEXT("gpt-4.1-mini");
    const TCHAR* DefaultOpenAIEnvironmentVariable = TEXT("OPENAI_API_KEY");
    const TCHAR* DefaultSourceOpenerMode = TEXT("Auto");
    const TCHAR* DefaultCustomSourceOpenerCommand = TEXT("code -g \"{file}:{line}\"");
    FString ReadString(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName, const FString& DefaultValue)
    {
        FString Value;
        return JsonObject->TryGetStringField(FieldName, Value) ? Value : DefaultValue;
    }

    int32 ReadInt(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName, int32 DefaultValue)
    {
        double Value = 0.0;
        return JsonObject->TryGetNumberField(FieldName, Value) ? static_cast<int32>(Value) : DefaultValue;
    }

    float ReadFloat(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName, float DefaultValue)
    {
        double Value = 0.0;
        return JsonObject->TryGetNumberField(FieldName, Value) ? static_cast<float>(Value) : DefaultValue;
    }

    FString NormalizeBackendUrl(const FString& BackendUrl)
    {
        FString Normalized = BackendUrl.TrimStartAndEnd();
        while (Normalized.EndsWith(TEXT("/")))
        {
            Normalized.LeftChopInline(1);
        }

        return Normalized.IsEmpty() ? DefaultBackendUrl : Normalized;
    }

    void SendSettingsJsonRequest(
        const FString& Url,
        const FString& Verb,
        const FString& Body,
        TFunction<void(bool, const FString&)> OnCompleted)
    {
        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
        Request->SetURL(Url);
        Request->SetVerb(Verb);
        Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
        Request->SetTimeout(120.0f);
        if (!Body.IsEmpty())
        {
            Request->SetContentAsString(Body);
        }

        Request->OnProcessRequestComplete().BindLambda([OnCompleted](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (!bWasSuccessful || !Response.IsValid())
            {
                OnCompleted(false, TEXT("The local backend did not respond."));
                return;
            }

            const int32 StatusCode = Response->GetResponseCode();
            const FString ResponseBody = Response->GetContentAsString();
            if (StatusCode < 200 || StatusCode >= 300)
            {
                OnCompleted(false, FGPAAApiModels::FormatHttpError(StatusCode, ResponseBody));
                return;
            }

            OnCompleted(true, ResponseBody);
        });

        if (!Request->ProcessRequest())
        {
            OnCompleted(false, FString::Printf(TEXT("Could not send request to %s."), *Url));
        }
    }

    void StoreAndShowDiagnostics(UGPAAEditorSettings* Settings, const FString& Message)
    {
        if (!Settings)
        {
            return;
        }

        Settings->LastDiagnosticsResult = Message;
        FMessageDialog::Open(
            EAppMsgType::Ok,
            FText::FromString(Message),
            FText::FromString(TEXT("Game Production AI Assistant")));
    }
}

UGPAAEditorSettings::UGPAAEditorSettings()
{
    BackendUrl = DefaultBackendUrl;
    bAutoStartBackend = true;
    bStopBackendOnEditorShutdown = true;
    BackendExecutablePath = FString();
    BackendLaunchArguments = FString();
    ChatProvider = DefaultChatProvider;
    OpenAIBaseUrl = DefaultOpenAIBaseUrl;
    OpenAIModel = DefaultOpenAIModel;
    OpenAIApiKeyEnvironmentVariable = DefaultOpenAIEnvironmentVariable;
    TimeoutSeconds = 120;
    MaxOutputTokens = 512;
    Temperature = 0.2f;
    SourceOpenerMode = DefaultSourceOpenerMode;
    CustomSourceOpenerCommand = DefaultCustomSourceOpenerCommand;
    bIncludeSecretsInExport = false;
}

void UGPAAEditorSettings::CheckBackend()
{
    LastDiagnosticsResult = TEXT("Checking backend...");
    TWeakObjectPtr<UGPAAEditorSettings> WeakThis(this);
    const TSharedRef<FGPAABackendSupervisor, ESPMode::ThreadSafe> Supervisor = MakeShared<FGPAABackendSupervisor>();

    Supervisor->EnsureBackendReady(
        *this,
        FGPAARequestCompleted::CreateLambda([WeakThis](bool, const FString& Message)
        {
            if (WeakThis.IsValid())
            {
                WeakThis->LastDiagnosticsResult = Message;
            }
        }),
        FGPAARequestCompleted::CreateLambda([WeakThis, Supervisor](bool bReady, const FString& ReadyMessage)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }

            if (!bReady)
            {
                StoreAndShowDiagnostics(WeakThis.Get(), ReadyMessage);
                return;
            }

            const FString Url = NormalizeBackendUrl(WeakThis->BackendUrl) + TEXT("/runtime/status");
            SendSettingsJsonRequest(Url, TEXT("GET"), FString(), [WeakThis, Supervisor](bool bSucceeded, const FString& Message)
            {
                if (!WeakThis.IsValid())
                {
                    return;
                }

                StoreAndShowDiagnostics(WeakThis.Get(), bSucceeded
                    ? FString::Printf(TEXT("Backend OK: %s"), *Message)
                    : Message);
            });
        }));
}

void UGPAAEditorSettings::TestPromptIA()
{
    LastDiagnosticsResult = TEXT("Saving chat settings...");
    TWeakObjectPtr<UGPAAEditorSettings> WeakThis(this);
    const TSharedRef<FGPAABackendSupervisor, ESPMode::ThreadSafe> Supervisor = MakeShared<FGPAABackendSupervisor>();

    Supervisor->EnsureBackendReady(
        *this,
        FGPAARequestCompleted::CreateLambda([WeakThis](bool, const FString& Message)
        {
            if (WeakThis.IsValid())
            {
                WeakThis->LastDiagnosticsResult = Message;
            }
        }),
        FGPAARequestCompleted::CreateLambda([WeakThis, Supervisor](bool bReady, const FString& ReadyMessage)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }

            if (!bReady)
            {
                StoreAndShowDiagnostics(WeakThis.Get(), ReadyMessage);
                return;
            }

            const FString BaseUrl = NormalizeBackendUrl(WeakThis->BackendUrl);
            const FString SettingsBody = FGPAAApiModels::SerializeChatSettings(WeakThis->ToChatSettings());
            SendSettingsJsonRequest(
                BaseUrl + TEXT("/runtime/chat-settings"),
                TEXT("POST"),
                SettingsBody,
                [WeakThis, BaseUrl, Supervisor](bool bSettingsSaved, const FString& SettingsMessage)
                {
                    if (!WeakThis.IsValid())
                    {
                        return;
                    }

                    if (!bSettingsSaved)
                    {
                        StoreAndShowDiagnostics(WeakThis.Get(), SettingsMessage);
                        return;
                    }

                    WeakThis->LastDiagnosticsResult = TEXT("Testing Prompt IA provider...");
                    SendSettingsJsonRequest(
                        BaseUrl + TEXT("/runtime/chat-provider/test"),
                        TEXT("POST"),
                        TEXT("{}"),
                        [WeakThis, Supervisor](bool bTestSucceeded, const FString& TestMessage)
                        {
                            if (!WeakThis.IsValid())
                            {
                                return;
                            }

                            if (!bTestSucceeded)
                            {
                                StoreAndShowDiagnostics(WeakThis.Get(), TestMessage);
                                return;
                            }

                            FGPAAChatProviderTestResult Result;
                            if (FGPAAApiModels::ParseChatProviderTestResult(TestMessage, Result))
                            {
                                StoreAndShowDiagnostics(WeakThis.Get(), FString::Printf(
                                    TEXT("%s (%s) is ready. %s"),
                                    *Result.Provider,
                                    Result.Model.IsEmpty() ? TEXT("default model") : *Result.Model,
                                    *Result.Message));
                                return;
                            }

                            StoreAndShowDiagnostics(WeakThis.Get(), TEXT("Prompt IA provider responded."));
                        });
                });
        }));
}

void UGPAAEditorSettings::ExportSettingsProfile()
{
    const FString ProfilePath = ResolveProfilePath(ExportProfilePath, TEXT("GPAASettingsProfile.json"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(ProfilePath), true);
    TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();

    JsonObject->SetStringField(TEXT("backendUrl"), BackendUrl);
    JsonObject->SetBoolField(TEXT("autoStartBackend"), bAutoStartBackend);
    JsonObject->SetBoolField(TEXT("stopBackendOnEditorShutdown"), bStopBackendOnEditorShutdown);
    JsonObject->SetStringField(TEXT("backendProjectPath"), BackendProjectPath);
    JsonObject->SetStringField(TEXT("backendExecutablePath"), BackendExecutablePath);
    JsonObject->SetStringField(TEXT("backendLaunchArguments"), BackendLaunchArguments);
    JsonObject->SetStringField(TEXT("projectRootOverride"), ProjectRootOverride);
    JsonObject->SetStringField(TEXT("chatProvider"), ChatProvider);
    JsonObject->SetStringField(TEXT("openAIBaseUrl"), OpenAIBaseUrl);
    JsonObject->SetStringField(TEXT("openAIModel"), OpenAIModel);
    JsonObject->SetStringField(TEXT("openAIApiKeyEnvironmentVariable"), OpenAIApiKeyEnvironmentVariable);
    JsonObject->SetNumberField(TEXT("timeoutSeconds"), TimeoutSeconds);
    JsonObject->SetNumberField(TEXT("maxOutputTokens"), MaxOutputTokens);
    JsonObject->SetNumberField(TEXT("temperature"), Temperature);
    JsonObject->SetStringField(TEXT("sourceOpenerMode"), SourceOpenerMode);
    JsonObject->SetStringField(TEXT("customSourceOpenerCommand"), CustomSourceOpenerCommand);
    JsonObject->SetBoolField(TEXT("includesSecrets"), bIncludeSecretsInExport);

    if (bIncludeSecretsInExport)
    {
        JsonObject->SetStringField(TEXT("openAIApiKey"), OpenAIApiKey);
    }

    FString Output;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(JsonObject, Writer);
    FFileHelper::SaveStringToFile(Output, *ProfilePath);
}

void UGPAAEditorSettings::ImportSettingsProfile()
{
    const FString ProfilePath = ResolveProfilePath(ImportProfilePath, TEXT("GPAASettingsProfile.json"));
    FString Input;
    if (!FFileHelper::LoadFileToString(Input, *ProfilePath))
    {
        return;
    }

    TSharedPtr<FJsonObject> JsonObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Input);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        return;
    }

    BackendUrl = ReadString(JsonObject, TEXT("backendUrl"), BackendUrl);
    bool bImportedAutoStart = bAutoStartBackend;
    if (JsonObject->TryGetBoolField(TEXT("autoStartBackend"), bImportedAutoStart))
    {
        bAutoStartBackend = bImportedAutoStart;
    }
    bool bImportedStopBackend = bStopBackendOnEditorShutdown;
    if (JsonObject->TryGetBoolField(TEXT("stopBackendOnEditorShutdown"), bImportedStopBackend))
    {
        bStopBackendOnEditorShutdown = bImportedStopBackend;
    }
    BackendProjectPath = ReadString(JsonObject, TEXT("backendProjectPath"), BackendProjectPath);
    BackendExecutablePath = ReadString(JsonObject, TEXT("backendExecutablePath"), BackendExecutablePath);
    BackendLaunchArguments = ReadString(JsonObject, TEXT("backendLaunchArguments"), BackendLaunchArguments);
    ProjectRootOverride = ReadString(JsonObject, TEXT("projectRootOverride"), ProjectRootOverride);
    ChatProvider = ReadString(JsonObject, TEXT("chatProvider"), ChatProvider);
    OpenAIBaseUrl = ReadString(JsonObject, TEXT("openAIBaseUrl"), OpenAIBaseUrl);
    OpenAIModel = ReadString(JsonObject, TEXT("openAIModel"), OpenAIModel);
    OpenAIApiKeyEnvironmentVariable = ReadString(JsonObject, TEXT("openAIApiKeyEnvironmentVariable"), OpenAIApiKeyEnvironmentVariable);
    TimeoutSeconds = ReadInt(JsonObject, TEXT("timeoutSeconds"), TimeoutSeconds);
    MaxOutputTokens = ReadInt(JsonObject, TEXT("maxOutputTokens"), MaxOutputTokens);
    Temperature = ReadFloat(JsonObject, TEXT("temperature"), Temperature);
    SourceOpenerMode = ReadString(JsonObject, TEXT("sourceOpenerMode"), SourceOpenerMode);
    CustomSourceOpenerCommand = ReadString(JsonObject, TEXT("customSourceOpenerCommand"), CustomSourceOpenerCommand);

    bool bImportedSecrets = false;
    if (JsonObject->TryGetBoolField(TEXT("includesSecrets"), bImportedSecrets) && bImportedSecrets)
    {
        OpenAIApiKey = ReadString(JsonObject, TEXT("openAIApiKey"), OpenAIApiKey);
    }

    SaveConfig();
}

FString UGPAAEditorSettings::GetResolvedProjectRoot() const
{
    return ProjectRootOverride.IsEmpty()
        ? FPaths::ConvertRelativePathToFull(FPaths::ProjectDir())
        : ProjectRootOverride;
}

FGPAAChatSettings UGPAAEditorSettings::ToChatSettings() const
{
    FGPAAChatSettings Settings;
    Settings.Provider = ChatProvider;
    Settings.OpenAIBaseUrl = OpenAIBaseUrl;
    Settings.OpenAIModel = OpenAIModel;
    Settings.OpenAIApiKey = OpenAIApiKey;
    Settings.OpenAIApiKeyEnvironmentVariable = OpenAIApiKeyEnvironmentVariable;
    Settings.TimeoutSeconds = TimeoutSeconds;
    Settings.MaxOutputTokens = MaxOutputTokens;
    Settings.Temperature = Temperature;
    return Settings;
}

void UGPAAEditorSettings::ApplyChatSettings(const FGPAAChatSettings& ChatSettings)
{
    ChatProvider = ChatSettings.Provider;
    OpenAIBaseUrl = ChatSettings.OpenAIBaseUrl;
    OpenAIModel = ChatSettings.OpenAIModel;
    OpenAIApiKey = ChatSettings.OpenAIApiKey;
    OpenAIApiKeyEnvironmentVariable = ChatSettings.OpenAIApiKeyEnvironmentVariable;
    TimeoutSeconds = ChatSettings.TimeoutSeconds;
    MaxOutputTokens = ChatSettings.MaxOutputTokens;
    Temperature = ChatSettings.Temperature;
    SaveConfig();
}

FString UGPAAEditorSettings::ResolveProfilePath(const FString& CandidatePath, const FString& DefaultFileName) const
{
    if (!CandidatePath.IsEmpty())
    {
        return FPaths::ConvertRelativePathToFull(CandidatePath);
    }

    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GPAA"), DefaultFileName);
}
