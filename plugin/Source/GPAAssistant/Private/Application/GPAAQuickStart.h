// Copyright Game Production AI Assistant contributors 2026.
#pragma once
#include "CoreMinimal.h"
#include "GPAALocalChat.h"

enum class EGPAAQuickStartStep : int8
{
    NotStarted = -1,
    Settings,
    Download,
    Verification,
    Ready
};

// Game-thread controller. The panel drives Tick; closing it cancels its own work.
class FGPAAQuickStart : public TSharedFromThis<FGPAAQuickStart>
{
  public:
    ~FGPAAQuickStart();
    bool Start();
    void Tick();
    void Cancel();
    static bool IsRunning();
    bool Running() const { return bRunning; }
    bool Succeeded() const { return Step == EGPAAQuickStartStep::Ready; }
    int32 StepIndex() const { return static_cast<int32>(Step); }
    FString Message;

  private:
    EGPAAQuickStartStep Step = EGPAAQuickStartStep::NotStarted;
    static TWeakPtr<FGPAAQuickStart> Active;
    bool bRunning = false, bOwnDownload = false;
    TSharedPtr<FGPAALocalChat, ESPMode::ThreadSafe> Chat;
    FString Answer;
    void Fail(const FString &Error);
};
