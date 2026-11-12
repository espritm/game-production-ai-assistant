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
    virtual void PostInitProperties() override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
    void SelectAI(bool Local);
    void ResetToDefaults();
    UPROPERTY(EditAnywhere, Config, Category = "Project") FString ProjectRootOverride;
    UPROPERTY(Config)
    FString ChatProvider;
    UPROPERTY(EditAnywhere, Config, Category = "Cloud AI Provider", meta = (GetOptions = "GetChatProviderOptions"))
    FString CloudProvider = TEXT("OpenAI");
    UPROPERTY(EditAnywhere, Config, Category = "Cloud AI Provider") FString OpenAIBaseUrl;
    UPROPERTY(EditAnywhere, Config, Category = "Cloud AI Provider") FString OpenAIModel;
    UPROPERTY(
        EditAnywhere, Transient, Category = "Cloud AI Provider",
        meta =
            (PasswordField = true,
             ToolTip =
                 "Enter your personal key, then click Save Personal Key. It is never saved in project configuration."))
    FString OpenAIApiKey;
    UPROPERTY(Config)
    bool bCloudContextAccepted = false;
    UPROPERTY(EditAnywhere, Config, Category = "Cloud AI Provider", meta = (ClampMin = "5", ClampMax = "600"))
    int32 TimeoutSeconds = 120;
    UPROPERTY(EditAnywhere, Config, Category = "Cloud AI Provider", meta = (ClampMin = "64", ClampMax = "32768"))
    int32 MaxOutputTokens = 512;
    UPROPERTY(EditAnywhere, Config, Category = "Local AI", meta = (GetOptions = "GetLocalModelOptions"))
    FString LocalModelId;
    UPROPERTY(EditAnywhere, Config, Category = "Local AI", meta = (ToolTip = "Optional existing instruction GGUF. Empty uses the selected downloaded model. This machine-specific path is excluded from profiles."))
    FString LocalModelFile;
    UPROPERTY(EditAnywhere, Config, Category = "Local AI", meta = (ClampMin = "2048", ClampMax = "8192"))
    int32 LocalContextTokens = 4096;
    UPROPERTY(EditAnywhere, Config, Category = "Local AI", meta = (ClampMin = "1", ClampMax = "8", ToolTip = "CPU threads reserved for local inference. Unreal also needs CPU and RAM."))
    int32 LocalThreads = 4;
    UFUNCTION() TArray<FString> GetChatProviderOptions() const;
    UFUNCTION() TArray<FString> GetLocalModelOptions() const;
    void DownloadLocalModel();
    void CancelLocalDownload();
    void RemoveLocalModel();
    void ChooseLocalModelFile();
    void ClearLocalModelFile();
    FString GetLocalModelDescription() const;
    UPROPERTY(EditAnywhere, Config, Category = "Source Opener") FString SourceOpenerMode;
    UPROPERTY(EditAnywhere, Config, Category = "Source Opener") FString CustomSourceOpenerCommand;
    FString LastKeyResult;
    FString LastProfileResult;
    UFUNCTION(CallInEditor, Category = "Cloud AI Provider", meta = (DisplayName = "Save Personal Key")) void SavePersonalKey();
    UFUNCTION(CallInEditor, Category = "Cloud AI Provider", meta = (DisplayName = "Remove Personal Key")) void RemovePersonalKey();
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
