// Copyright Game Production AI Assistant contributors 2026.
#pragma once
#include "GPAACore.h"
#include "Interfaces/IHttpRequest.h"

// Byte buffering is deliberate: UTF-8 may be split at any network boundary.
class GPAAINFERENCE_API FGPAASseDecoder
{
  public:
    bool Feed(const uint8 *Bytes, int32 Length, TArray<FString> &Deltas, FString &Error);
    bool IsComplete() const
    {
        return Complete;
    }

  private:
    TArray<uint8> Pending;
    bool Complete = false;
    bool Event(const FString &Data, TArray<FString> &Deltas, FString &Error);
};

class GPAAINFERENCE_API FGPAACloudChat final : public IGPAAChatProvider,
                                               public TSharedFromThis<FGPAACloudChat, ESPMode::ThreadSafe>
{
  public:
    static void ShutdownRequests();
    static void InitializeRequests();
    virtual ~FGPAACloudChat() override;
    virtual void Start(const FGPAAChatRequest &Request, FGPAATextEvent OnText, FGPAAChatFinished OnFinished) override;
    virtual void Cancel() override;

  private:
    FHttpRequestPtr ActiveRequest;
    FGPAATaskPtr ActiveTask;
};
