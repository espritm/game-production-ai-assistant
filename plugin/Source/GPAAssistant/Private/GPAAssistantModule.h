#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FGPAAssistantModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();
    void HandleEnginePreExit();
    void OpenAssistantTab();
    TSharedRef<class SDockTab> SpawnAssistantTab(const class FSpawnTabArgs& Args);

    FDelegateHandle EnginePreExitHandle;
};
