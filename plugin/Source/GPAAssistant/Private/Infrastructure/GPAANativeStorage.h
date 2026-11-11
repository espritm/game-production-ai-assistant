// Copyright Game Production AI Assistant contributors 2026.
#pragma once
#include "GPAACore.h"
#include "SQLiteDatabase.h"

class FGPAANativeStorage final : public IGPAAKnowledgeRepository
{
  public:
    explicit FGPAANativeStorage(const FString &DatabasePath);
    virtual ~FGPAANativeStorage() override;
    virtual bool Load(FGPAASnapshot &Result, FString &Error) override;
    virtual bool Replace(const FGPAASnapshot &Value, FString &Error) override;
    bool AddHistory(const FString &Question, const FString &Answer, bool Complete, FString &Error,
                    const FString &SourcesJson = TEXT("[]"), const FString& Kind = TEXT("Prompt"),
                    const FString& Scope = TEXT("All"));
    bool ImportLegacyHistory(const FString &JsonPath, FString &Error);
    FString History(FString &Error);
    bool IsReady(FString &Error) const;

  private:
    FSQLiteDatabase Database;
    FString InitializationError;
};

class FGPAANativeSecrets final : public IGPAASecretStore
{
  public:
    virtual bool Write(const FString &Scope, const FString &Secret, FString &Error) override;
    virtual FString Read(const FString &Scope) override;
    virtual bool Remove(const FString &Scope, FString &Error) override;
    static FString Scope(const FString &Endpoint);
};
