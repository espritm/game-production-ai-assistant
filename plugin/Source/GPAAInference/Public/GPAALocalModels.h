// Copyright Game Production AI Assistant contributors 2026.
#pragma once
#include "GPAACore.h"
#include "Async/Future.h"

struct FGPAALocalModelSpec
{
    FString Id, Name, FileName, Url, Sha256;
    int64 Bytes = 0;
};

// Download operations never carry a prompt, source excerpt or provider credential.
class GPAAINFERENCE_API FGPAALocalModels
{
public:
    static FGPAALocalModels& Get();
    static const TArray<FGPAALocalModelSpec>& Catalogue();
    static const FGPAALocalModelSpec* Find(const FString& Id);
    static FString CacheDirectory();
    static FString ModelPath(const FGPAALocalModelSpec& Spec, const FString& Directory = FString());
    static bool ValidateFile(const FString& Path, const FGPAALocalModelSpec* Spec,
        const FGPAATaskPtr& Task, FString& Error);
    static bool ValidateRange(const FString& Header, int64 First, int64 Last, int64 Total);
    bool Download(const FString& Id, const FString& Directory = FString());
    bool Remove(const FString& Id);
    void Cancel();
    bool IsBusy() const;
    FString Status() const;
    void Shutdown();
private:
    mutable FCriticalSection Mutex;
    FString Message;
    FGPAATaskPtr Task;
    TFuture<void> Work;
    bool Stopping = false;
    void SetStatus(const FString& Value);
};
