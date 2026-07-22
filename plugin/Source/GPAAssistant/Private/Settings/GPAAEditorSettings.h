#pragma once

#include "CoreMinimal.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#include "Engine/DeveloperSettings.h"
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#include "Models/GPAAApiModels.h"
#include "GPAAEditorSettings.generated.h"

UCLASS(Config=EditorPerProjectUserSettings, DefaultConfig, DisplayName="Game Production AI Assistant")
class UGPAAEditorSettings final : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UGPAAEditorSettings();

    UPROPERTY(EditAnywhere, Config, Category="Backend")
    FString BackendUrl;

    UPROPERTY(EditAnywhere, Config, Category="Backend")
    bool bAutoStartBackend;

    UPROPERTY(EditAnywhere, Config, Category="Backend")
    bool bStopBackendOnEditorShutdown;

    UPROPERTY(EditAnywhere, Config, Category="Backend")
    FString BackendProjectPath;

    UPROPERTY(EditAnywhere, Config, Category="Backend")
    FString BackendExecutablePath;

    UPROPERTY(EditAnywhere, Config, Category="Backend")
    FString BackendLaunchArguments;

    UPROPERTY(EditAnywhere, Config, Category="Project")
    FString ProjectRootOverride;

    UPROPERTY(EditAnywhere, Config, Category="Chat")
    FString ChatProvider;

    UPROPERTY(EditAnywhere, Config, Category="Chat|OpenAI Compatible")
    FString OpenAIBaseUrl;

    UPROPERTY(EditAnywhere, Config, Category="Chat|OpenAI Compatible")
    FString OpenAIModel;

    UPROPERTY(EditAnywhere, Config, Category="Chat|OpenAI Compatible", meta=(PasswordField=true))
    FString OpenAIApiKey;

    UPROPERTY(EditAnywhere, Config, Category="Chat|OpenAI Compatible")
    FString OpenAIApiKeyEnvironmentVariable;

    UPROPERTY(EditAnywhere, Config, Category="Chat")
    int32 TimeoutSeconds;

    UPROPERTY(EditAnywhere, Config, Category="Chat")
    int32 MaxOutputTokens;

    UPROPERTY(EditAnywhere, Config, Category="Chat")
    float Temperature;

    UPROPERTY(EditAnywhere, Config, Category="Source Opener")
    FString SourceOpenerMode;

    UPROPERTY(EditAnywhere, Config, Category="Source Opener")
    FString CustomSourceOpenerCommand;

    UPROPERTY(EditAnywhere, Config, Category="Import / Export")
    FString ExportProfilePath;

    UPROPERTY(EditAnywhere, Config, Category="Import / Export")
    FString ImportProfilePath;

    UPROPERTY(EditAnywhere, Config, Category="Import / Export")
    bool bIncludeSecretsInExport;

    UPROPERTY(VisibleAnywhere, Transient, Category="Diagnostics")
    FString LastDiagnosticsResult;

    UFUNCTION(CallInEditor, Category="Diagnostics", meta=(DisplayName="Check Backend"))
    void CheckBackend();

    UFUNCTION(CallInEditor, Category="Diagnostics", meta=(DisplayName="Test Prompt IA"))
    void TestPromptIA();

    UFUNCTION(CallInEditor, Category="Import / Export")
    void ExportSettingsProfile();

    UFUNCTION(CallInEditor, Category="Import / Export")
    void ImportSettingsProfile();

    FString GetResolvedProjectRoot() const;
    FGPAAChatSettings ToChatSettings() const;
    void ApplyChatSettings(const FGPAAChatSettings& ChatSettings);

private:
    FString ResolveProfilePath(const FString& CandidatePath, const FString& DefaultFileName) const;
};
