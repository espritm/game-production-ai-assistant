#include "GPAAssistantModule.h"

#include "ToolMenus.h"
#include "UI/SGPAAssistantWidget.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FGPAAssistantModule"

namespace
{
    const FName AssistantTabName("GPAAssistant");
}

void FGPAAssistantModule::StartupModule()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        AssistantTabName,
        FOnSpawnTab::CreateRaw(this, &FGPAAssistantModule::SpawnAssistantTab))
        .SetDisplayName(LOCTEXT("AssistantTabTitle", "AI Assistant"))
        .SetMenuType(ETabSpawnerMenuType::Hidden);

    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FGPAAssistantModule::RegisterMenus));
}

void FGPAAssistantModule::ShutdownModule()
{
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AssistantTabName);
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
