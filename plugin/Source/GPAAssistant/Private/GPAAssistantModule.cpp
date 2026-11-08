// Copyright Game Production AI Assistant contributors 2026.
#include "GPAAssistantModule.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#include "ISettingsModule.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#include "Misc/CoreDelegates.h"
#include "Settings/GPAAEditorSettings.h"
#include "Settings/GPAAEditorSettingsDetails.h"
#include "PropertyEditorModule.h"
#include "Editor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Services/GPAAAssetMetadataRequestBuilder.h"
#include "Misc/CommandLine.h"
#include "UI/SGPAAssistantWidget.h"
#include "Application/GPAANativeRuntime.h"

#define LOCTEXT_NAMESPACE "FGPAAssistantModule"

namespace
{
    const FName AssistantTabName("GPAAssistant");
}

void FGPAAssistantModule::StartupModule()
{
    // Automation owns the runtime explicitly; do not add unrelated jobs to its fixtures.
    if (!IsRunningCommandlet() && !FString(FCommandLine::Get()).Contains(TEXT("Automation RunTests")))
        AutomaticKnowledgeTicker = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateRaw(this, &FGPAAssistantModule::TickAutomaticKnowledge), 1.0f);
    auto& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    PropertyEditor.RegisterCustomClassLayout(UGPAAEditorSettings::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(&FGPAAEditorSettingsDetails::MakeInstance));
    EnginePreExitHandle = FCoreDelegates::OnEnginePreExit.AddRaw(this, &FGPAAssistantModule::HandleEnginePreExit);

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        AssistantTabName,
        FOnSpawnTab::CreateRaw(this, &FGPAAssistantModule::SpawnAssistantTab))
        .SetDisplayName(LOCTEXT("AssistantTabTitle", "AI Assistant"))
        .SetMenuType(ETabSpawnerMenuType::Hidden);

    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FGPAAssistantModule::RegisterMenus));

    if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(TEXT("Settings")))
    {
        SettingsModule->RegisterSettings(
            TEXT("Project"),
            TEXT("Plugins"),
            TEXT("GameProductionAIAssistant"),
            LOCTEXT("SettingsName", "Game Production AI Assistant"),
            LOCTEXT("SettingsDescription", "Configure local search, cloud chat, personal credentials and portable settings profiles."),
            GetMutableDefault<UGPAAEditorSettings>());
    }
}

void FGPAAssistantModule::ShutdownModule()
{
    bStopping = true;
    FTSTicker::GetCoreTicker().RemoveTicker(AutomaticKnowledgeTicker);
    FGPAANativeRuntime::Shutdown();
    if (auto* PropertyEditor = FModuleManager::GetModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor")))
        PropertyEditor->UnregisterCustomClassLayout(UGPAAEditorSettings::StaticClass()->GetFName());
    FCoreDelegates::OnEnginePreExit.Remove(EnginePreExitHandle);

    if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(TEXT("Settings")))
    {
        SettingsModule->UnregisterSettings(
            TEXT("Project"),
            TEXT("Plugins"),
            TEXT("GameProductionAIAssistant"));
    }

    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AssistantTabName);
}

void FGPAAssistantModule::HandleEnginePreExit()
{
    bStopping = true;
    FTSTicker::GetCoreTicker().RemoveTicker(AutomaticKnowledgeTicker);
    FGPAANativeRuntime::Shutdown();
}

bool FGPAAssistantModule::TickAutomaticKnowledge(float DeltaTime)
{
    if (bStopping) return false;
    const double Now = FPlatformTime::Seconds();
    if (!GEditor || Now < NextKnowledgeRefresh) return true;
    auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    if (Registry.Get().IsLoadingAssets()) return true;
    auto Runtime = FGPAANativeRuntime::Get();
    if (Runtime->IsBusy()) return true;
    const FString Root = GetDefault<UGPAAEditorSettings>()->GetResolvedProjectRoot();
    if (Runtime->RefreshIfIdle(Root, FGPAAAssetMetadataRequestBuilder::BuildSyncRequest(Root)))
    {
        UE_LOG(LogTemp, Display, TEXT("GPAA automatic index cycle started: clockMs=%lld"), static_cast<int64>(Now * 1000.0));
        NextKnowledgeRefresh = Now + 60.0;
    }
    return true;
}

void FGPAAssistantModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
    FToolMenuSection& Section = Menu->FindOrAddSection("GPAAssistant");
    Section.AddMenuEntry(
        "OpenGPAAssistant",
        LOCTEXT("OpenAssistantLabel", "Game Production AI Assistant"),
        LOCTEXT("OpenAssistantTooltip", "Open the local AI assistant panel."),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateRaw(this, &FGPAAssistantModule::OpenAssistantTab)));
}

void FGPAAssistantModule::OpenAssistantTab()
{
    FGlobalTabmanager::Get()->TryInvokeTab(AssistantTabName);
}

TSharedRef<SDockTab> FGPAAssistantModule::SpawnAssistantTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SGPAAssistantWidget)
        ];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGPAAssistantModule, GPAAssistant)
