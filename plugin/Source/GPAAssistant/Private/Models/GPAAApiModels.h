#pragma once

#include "CoreMinimal.h"

struct FGPAASourceReference
{
    FString RelativePath;
    FString Content;
    FString SourceType;
    int32 StartLine = 0;
    int32 EndLine = 0;
    double Score = 0.0;
};

struct FGPAASearchResponse
{
    TArray<FGPAASourceReference> Results;
};

struct FGPAAChatResponse
{
    FString Answer;
    TArray<FGPAASourceReference> Sources;
};

struct FGPAAChatHistoryEntry
{
    FString Question;
    FString Answer;
    FString CreatedAt;
};

struct FGPAAErrorResponse
{
    FString Title;
    FString Detail;
};

struct FGPAAChatSettings
{
    FString Provider;
    FString OpenAIBaseUrl;
    FString OpenAIModel;
    FString OpenAIApiKey;
    FString OpenAIApiKeyEnvironmentVariable;
    int32 TimeoutSeconds = 120;
    int32 MaxOutputTokens = 512;
    float Temperature = 0.2f;
};

struct FGPAAIndexStatistics
{
    int32 FilesScanned = 0;
    int32 FilesIndexed = 0;
    int32 ChunksIndexed = 0;
    FString IndexedAt;
    int32 FilesReused = 0;
    int32 FilesChanged = 0;
    int32 FilesRemoved = 0;
    int32 ChunksReused = 0;
    int32 ChunksEmbedded = 0;
    bool bFullRebuild = false;
    FString EmbeddingModel;
};

struct FGPAAKnowledgeStatus
{
    FGPAAIndexStatistics Statistics;
    FString EmbeddingModel;
    int32 CodeFilesIndexed = 0;
    int32 AssetsSynced = 0;
    int32 AssetDocumentsIndexed = 0;
    bool bHasIndex = false;
};

struct FGPAAChatProviderTestResult
{
    FString Provider;
    FString Model;
    FString Message;
};

class FGPAAApiModels
{
public:
    static FString SerializeChatSettings(const FGPAAChatSettings& Settings);
    static bool ParseErrorResponse(const FString& ResponseBody, FGPAAErrorResponse& OutError);
    static FString FormatHttpError(int32 StatusCode, const FString& ResponseBody);
    static bool ParseSearchResponse(const FString& ResponseBody, FGPAASearchResponse& OutResponse);
    static bool ParseChatResponse(const FString& ResponseBody, FGPAAChatResponse& OutResponse);
    static bool ParseChatHistoryResponse(const FString& ResponseBody, TArray<FGPAAChatHistoryEntry>& OutEntries);
    static bool ParseChatSettings(const FString& ResponseBody, FGPAAChatSettings& OutSettings);
    static bool ParseKnowledgeStatus(const FString& ResponseBody, FGPAAKnowledgeStatus& OutStatus);
    static bool ParseChatProviderTestResult(const FString& ResponseBody, FGPAAChatProviderTestResult& OutResult);
    static bool ParseIndexStatisticsResponse(const FString& ResponseBody, FGPAAIndexStatistics& OutStatistics);
};
