// Copyright Game Production AI Assistant contributors 2026.
#include "Modules/ModuleManager.h"
#include "GPAACloudChat.h"
#include "GPAALocalChat.h"
#include "GPAALocalModels.h"
#include "HttpModule.h"
#include "Misc/CoreDelegates.h"
class FGPAAInferenceModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        FModuleManager::LoadModuleChecked<FHttpModule>(TEXT("HTTP"));
        FGPAACloudChat::InitializeRequests();
        FGPAALocalChat::Initialize();
        PreExit = FCoreDelegates::OnEnginePreExit.AddStatic(&FGPAAInferenceModule::StopInference);
    }
    virtual void ShutdownModule() override
    {
        FCoreDelegates::OnEnginePreExit.Remove(PreExit);
        StopInference();
    }
    virtual bool SupportsDynamicReloading() override { return false; }
private:
    static void StopInference()
    {
        FGPAALocalModels::Get().Shutdown();
        FGPAALocalChat::ShutdownAll();
        FGPAACloudChat::ShutdownRequests();
    }
    FDelegateHandle PreExit;
};
IMPLEMENT_MODULE(FGPAAInferenceModule, GPAAInference)
