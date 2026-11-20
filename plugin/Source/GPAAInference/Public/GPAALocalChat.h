// Copyright Game Production AI Assistant contributors 2026.
#pragma once
#include "GPAACore.h"
#include "Async/Future.h"

// Token pieces can split UTF-8 code points. Only complete code points reach Slate.
class GPAAINFERENCE_API FGPAALocalUtf8
{
public:
    bool Append(const char* Data, int32 Count, FString& Text);
    bool IsEmpty() const { return Pending.IsEmpty(); }
private:
    TArray<uint8> Pending;
};

class GPAAINFERENCE_API FGPAALocalChat final : public IGPAAChatProvider,
    public TSharedFromThis<FGPAALocalChat, ESPMode::ThreadSafe>
{
public:
    virtual ~FGPAALocalChat() override;
    static void Initialize();
    static void ShutdownAll();
    static bool IsAnyBusy();
    static bool ValidateReferences(const FString& Answer, const FString& Evidence, FString& Error);
    bool IsBusy() const { return Work.IsValid() && !Work.IsReady(); }
    virtual void Start(const FGPAAChatRequest& Request, FGPAATextEvent OnText, FGPAAChatFinished OnFinished) override;
    virtual void Cancel() override;
    void Wait();
private:
    FGPAATaskPtr Task;
    TFuture<void> Work;
};
