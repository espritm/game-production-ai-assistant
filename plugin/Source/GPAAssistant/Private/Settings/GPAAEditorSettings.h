// Copyright Game Production AI Assistant contributors 2026.
#pragma once
#include "CoreMinimal.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
#include "Engine/DeveloperSettings.h"
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#include "Models/GPAAApiModels.h"
#include "GPAAEditorSettings.generated.h"

UCLASS(Config = EditorPerProjectUserSettings, DisplayName = "Game Production AI Assistant")
class UGPAAEditorSettings final : public UDeveloperSettings
{
    GENERATED_BODY()
  public:
    UGPAAEditorSettings();
    UPROPERTY(EditAnywhere, Config, Category = "Project") FString ProjectRootOverride;
    UPROPERTY(EditAnywhere, Config, Category = "Chat",
              meta = (ToolTip = "OpenAI uses Responses; OpenAICompatible uses streaming Chat Completions."))
    FString ChatProvider;
    UPROPERTY(EditAnywhere, Config, Category = "Chat") FString OpenAIBaseUrl;
    UPROPERTY(EditAnywhere, Config, Category = "Chat") FString OpenAIModel;
    UPROPERTY(
        EditAnywhere, Transient, Category = "Chat",
        meta =
            (PasswordField = true,
             ToolTip =
                 "Enter your personal key, then click Save Personal Key. It is never saved in project configuration."))
    FString OpenAIApiKey;
    UPROPERTY(EditAnywhere, Config, Category = "Chat",
              meta = (ToolTip = "Send your questions and selected project excerpts to your provider. API usage is billed by that provider."))
    bool bCloudContextAccepted = false;
    UPROPERTY(EditAnywhere, Config, Category = "Chat", meta = (ClampMin = "5", ClampMax = "600"))
    int32 TimeoutSeconds = 120;
    UPROPERTY(EditAnywhere, Config, Category = "Chat", meta = (ClampMin = "64", ClampMax = "32768"))
    int32 MaxOutputTokens = 512;
    UPROPERTY(EditAnywhere, Config, Category = "Source Opener") FString SourceOpenerMode;
    UPROPERTY(EditAnywhere, Config, Category = "Source Opener") FString CustomSourceOpenerCommand;
    FString LastKeyResult;
    FString LastProfileResult;
    UFUNCTION(CallInEditor, Category = "Chat", meta = (DisplayName = "Save Personal Key")) void SavePersonalKey();
    UFUNCTION(CallInEditor, Category = "Chat", meta = (DisplayName = "Remove Personal Key")) void RemovePersonalKey();
    UFUNCTION(CallInEditor, Category = "Import / Export") void ExportSettingsProfile();
    UFUNCTION(CallInEditor, Category = "Import / Export") void ImportSettingsProfile();
    bool ExportSettingsProfileToFile(const FString& Path);
    bool ImportSettingsProfileFromFile(const FString& Path);
    FString GetResolvedProjectRoot() const;
    FGPAAChatSettings ToChatSettings() const;
    void ApplyChatSettings(const FGPAAChatSettings &Settings);
  private:
    bool ChooseProfileFile(bool Save, FString& Path);
    FString LastProfileDirectory;
};
