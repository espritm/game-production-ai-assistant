// Copyright Game Production AI Assistant contributors 2026.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"

class FGPAAssistantModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    virtual bool SupportsDynamicReloading() override { return false; }

private:
    void RegisterMenus();
    void HandleEnginePreExit();
    bool TickAutomaticKnowledge(float DeltaTime);
    FTSTicker::FDelegateHandle AutomaticKnowledgeTicker;
    double NextKnowledgeRefresh = 0;
    bool bStopping = false;
    void OpenAssistantTab();
    TSharedRef<class SDockTab> SpawnAssistantTab(const class FSpawnTabArgs& Args);

    FDelegateHandle EnginePreExitHandle;
};
