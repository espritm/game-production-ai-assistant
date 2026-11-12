// Copyright Game Production AI Assistant contributors 2026.
#pragma once
#include "CoreMinimal.h"
#include "GPAALocalChat.h"

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
    bool Succeeded() const { return Step == 3; }
    int32 Step = -1;
    FString Message;
private:
    static TWeakPtr<FGPAAQuickStart> Active;
    bool bRunning = false, bOwnDownload = false;
    TSharedPtr<FGPAALocalChat, ESPMode::ThreadSafe> Chat;
    FString Answer;
    void Fail(const FString& Error);
};
