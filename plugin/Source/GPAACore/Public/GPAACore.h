// Copyright Game Production AI Assistant contributors 2026.
#pragma once
#include "CoreMinimal.h"
#include "Templates/Atomic.h"

struct FGPAATask
{
    TAtomic<bool> Cancelled{false};
    FGuid Id = FGuid::NewGuid();
};
using FGPAATaskPtr = TSharedPtr<FGPAATask, ESPMode::ThreadSafe>;

struct FGPAADocument
{
    FString Path;
    FString Content;
    FString Kind = TEXT("Code");
    FString Hash;
};
struct FGPAAPassage
{
    FString Path, Content, Kind;
    int32 FirstLine = 1, LastLine = 1;
    TArray<float> Vector;
    double Score = 0;
};
struct FGPAAIndexReport
{
    int32 Files = 0, Chunks = 0, Reused = 0, Removed = 0, Embedded = 0;
    FString Model;
};
struct FGPAAStoredFile
{
    FString Hash;
    TArray<FGPAAPassage> Passages;
};
struct FGPAASnapshot
{
    FString Identity;
    TMap<FString, FGPAAStoredFile> Files;
};

class GPAACORE_API IGPAAEmbeddingProvider
{
  public:
    virtual ~IGPAAEmbeddingProvider() = default;
    virtual FString Identity() const = 0;
    virtual bool Encode(const FString &Text, TArray<float> &Result, FString &Error) = 0;
};
class GPAACORE_API IGPAAKnowledgeRepository
{
  public:
    virtual ~IGPAAKnowledgeRepository() = default;
    virtual bool Load(FGPAASnapshot &Result, FString &Error) = 0;
    virtual bool Replace(const FGPAASnapshot &Value, FString &Error) = 0;
};
class GPAACORE_API IGPAASecretStore
{
  public:
    virtual ~IGPAASecretStore() = default;
    virtual bool Write(const FString &Scope, const FString &Secret, FString &Error) = 0;
    virtual FString Read(const FString &Scope) = 0;
    virtual bool Remove(const FString &Scope, FString &Error) = 0;
};
struct FGPAAChatRequest
{
    FString Endpoint, Model, Key, Prompt;
    bool Responses = true;
    bool Local = false, LocalCustomModel = false;
    bool RequireProjectEvidence = false, HasProjectEvidence = false;
    FString LocalModelPath;
    FString SourceEvidence;
    TFunction<void(const FString&)> OnStatus;
    int32 LocalContextTokens = 4096, LocalThreads = 4;
    int32 MaxOutputTokens = 512;
    float Timeout = 120;
};
using FGPAATextEvent = TFunction<void(const FString &)>;
using FGPAAChatFinished = TFunction<void(bool, const FString &)>;
class GPAACORE_API IGPAAChatProvider
{
  public:
    virtual ~IGPAAChatProvider() = default;
    virtual void Start(const FGPAAChatRequest &Request, FGPAATextEvent OnText, FGPAAChatFinished OnFinished) = 0;
    virtual void Cancel() = 0;
};

class GPAACORE_API FGPAACore
{
  public:
    static FString Hash(const FString &Text);
    static FString ExtractContext(const FString &Content);
    static TArray<FGPAAPassage> Chunk(const FGPAADocument &Document, int32 MaxLines = 40, int32 Overlap = 8,
                                      int32 MaxCharacters = 1800);
    static double Similarity(const TArray<float> &A, const TArray<float> &B);
    static bool Index(const TArray<FGPAADocument> &Documents, IGPAAEmbeddingProvider &Embeddings,
                      IGPAAKnowledgeRepository &Repository, const FGPAATaskPtr &Task, FGPAAIndexReport &Report,
                      FString &Error, TFunction<void(int32, int32)> Progress = {});
    static bool Search(const FString &Query, const FString &Filter, int32 TopK, IGPAAEmbeddingProvider &Embeddings,
                       IGPAAKnowledgeRepository &Repository, const FGPAATaskPtr &Task, TArray<FGPAAPassage> &Results,
                       FString &Error);
    static FString Prompt(const FString &Question, const TArray<FGPAAPassage> &Sources,
                          int32 ContextCharacters = 12000);
};
