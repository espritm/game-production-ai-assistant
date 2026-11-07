// Copyright Game Production AI Assistant contributors 2026.
#include "Modules/ModuleManager.h"
#include "GPAACloudChat.h"
class FGPAAInferenceModule final : public IModuleInterface
{
public:
    virtual void ShutdownModule() override { FGPAACloudChat::ShutdownRequests(); }
    virtual bool SupportsDynamicReloading() override { return false; }
};
IMPLEMENT_MODULE(FGPAAInferenceModule, GPAAInference)
