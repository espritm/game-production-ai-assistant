#include "GPAAssistantModule.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#include "ISettingsModule.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#include "Misc/CoreDelegates.h"
#include "Settings/GPAAEditorSettings.h"
#include "Services/GPAABackendSupervisor.h"
#include "UI/SGPAAssistantWidget.h"

#define LOCTEXT_NAMESPACE "FGPAAssistantModule"

namespace
{
    const FName AssistantTabName("GPAAssistant");
}

void FGPAAssistantModule::StartupModule()
{
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
            LOCTEXT("SettingsDescription", "Configure the local backend, chat provider, source opening, and portable GPAA settings profiles."),
            GetMutableDefault<UGPAAEditorSettings>());
    }
}

void FGPAAssistantModule::ShutdownModule()
{
    FGPAABackendSupervisor::ShutdownLaunchedBackend();
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
    FGPAABackendSupervisor::ShutdownLaunchedBackend();
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
