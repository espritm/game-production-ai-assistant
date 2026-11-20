// Copyright Game Production AI Assistant contributors 2026.
#pragma once
#include "GPAACore.h"

class GPAAINFERENCE_API FGPAAMiniLM final : public IGPAAEmbeddingProvider
{
  public:
    explicit FGPAAMiniLM(const FString &PluginRoot);
    virtual ~FGPAAMiniLM() override;
    virtual FString Identity() const override;
    virtual bool Encode(const FString &Text, TArray<float> &Result, FString &Error) override;
    bool Tokenize(const FString &Text, TArray<int64> &Tokens, FString &Error);

  private:
    struct FImpl;
    TUniquePtr<FImpl> Impl;
};
