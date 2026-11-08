// Copyright Game Production AI Assistant contributors 2026.
#include "Modules/ModuleManager.h"
#include "GPAACloudChat.h"
#include "HttpModule.h"
#include "Misc/CoreDelegates.h"
class FGPAAInferenceModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        FModuleManager::LoadModuleChecked<FHttpModule>(TEXT("HTTP"));
        FGPAACloudChat::InitializeRequests();
        PreExit = FCoreDelegates::OnEnginePreExit.AddStatic(&FGPAACloudChat::ShutdownRequests);
    }
    virtual void ShutdownModule() override
    {
        FCoreDelegates::OnEnginePreExit.Remove(PreExit);
        FGPAACloudChat::ShutdownRequests();
    }
    virtual bool SupportsDynamicReloading() override { return false; }
private:
    FDelegateHandle PreExit;
};
IMPLEMENT_MODULE(FGPAAInferenceModule, GPAAInference)
