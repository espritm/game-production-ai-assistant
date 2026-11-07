// Copyright Game Production AI Assistant contributors 2026.
#pragma once
#include "GPAACore.h"
#include "GPAACloudChat.h"
#include "GPAAEmbeddings.h"
#include "Async/Future.h"
#include "Models/GPAAApiModels.h"

struct FGPAANativeResult
{
    FGPAAIndexStatistics Statistics;
    TArray<FGPAASourceReference> Sources;
    FString Answer;
};
class FGPAANativeRuntime : public TSharedFromThis<FGPAANativeRuntime, ESPMode::ThreadSafe>
{
  public:
    static TSharedRef<FGPAANativeRuntime, ESPMode::ThreadSafe> Get();
    static void Shutdown();
    static void CancelIfRunning();
    void Run(const FString &ProjectRoot, const FString &AssetSnapshot, const FString &Query, bool Ask, bool RefreshOnly,
             TFunction<void(const FString &)> OnStatus, TFunction<void(const FString &)> OnText,
             TFunction<void(bool, const FString &, const FGPAANativeResult &)> OnFinished);
    void LoadHistory(const FString &ProjectRoot, TFunction<void(bool, const FString &)> OnFinished);
    void ImportHistory(const FString &ProjectRoot, const FString &JsonPath,
                       TFunction<void(bool, const FString &)> OnFinished);
    void Cancel();
    static FString DatabasePath(const FString &ProjectRoot);

  private:
    FGPAATaskPtr CurrentTask;
    TArray<TFuture<void>> Jobs;
    FCriticalSection IndexMutex;
    TSharedPtr<FGPAAMiniLM, ESPMode::ThreadSafe> Embeddings;
    TSharedPtr<FGPAACloudChat, ESPMode::ThreadSafe> Cloud;
    FString ChatQuestion, ChatAnswer, ChatRoot, ChatSources;
    void StoreHistory(bool Complete, TFunction<void(const FString &)> OnSaved = {});
};
