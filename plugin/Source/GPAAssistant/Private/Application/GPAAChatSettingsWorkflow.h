#pragma once

#include "CoreMinimal.h"
#include "Models/GPAAApiModels.h"
#include "Services/GPAAHttpClient.h"

class FGPAAChatSettingsWorkflow
{
public:
    static void Save(
        TSharedPtr<FGPAAHttpClient> HttpClient,
        const FString& BackendUrl,
        const FGPAAChatSettings& Settings,
        FGPAARequestCompleted OnCompleted);

    static void SaveThenTest(
        TSharedPtr<FGPAAHttpClient> HttpClient,
        const FString& BackendUrl,
        const FGPAAChatSettings& Settings,
        FGPAARequestCompleted OnStatusChanged,
        FGPAARequestCompleted OnCompleted);
};
