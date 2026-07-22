#pragma once

#include "CoreMinimal.h"
#include "Models/GPAAApiModels.h"
#include "Services/GPAAHttpClient.h"

DECLARE_DELEGATE_OneParam(FGPAAWorkflowStatusChanged, const FString& /*Message*/);
DECLARE_DELEGATE_OneParam(FGPAAIndexUpdated, const FGPAAIndexStatistics& /*Statistics*/);

class FGPAAProjectWorkflow
{
public:
    static void Search(
        TSharedPtr<FGPAAHttpClient> HttpClient,
        const FString& BackendUrl,
        const FString& ProjectRoot,
        const FString& AssetMetadataRequest,
        const FString& Query,
        const FString& SourceFilter,
        FGPAAWorkflowStatusChanged OnStatusChanged,
        FGPAAIndexUpdated OnIndexUpdated,
        FGPAASearchCompleted OnCompleted);

    static void Ask(
        TSharedPtr<FGPAAHttpClient> HttpClient,
        const FString& BackendUrl,
        const FString& ProjectRoot,
        const FString& AssetMetadataRequest,
        const FString& Question,
        const FString& SourceFilter,
        const FString& ChatProviderName,
        FGPAAWorkflowStatusChanged OnStatusChanged,
        FGPAAIndexUpdated OnIndexUpdated,
        FGPAAChatCompleted OnCompleted);

    static void RefreshKnowledge(
        TSharedPtr<FGPAAHttpClient> HttpClient,
        const FString& BackendUrl,
        const FString& ProjectRoot,
        const FString& AssetMetadataRequest,
        FGPAAWorkflowStatusChanged OnStatusChanged,
        FGPAAIndexUpdated OnIndexUpdated,
        FGPAARequestCompleted OnCompleted);

private:
    static bool TryParseIndexResponse(const FString& Message, FGPAAIndexStatistics& OutStatistics, FGPAARequestCompleted OnFailure);
};
