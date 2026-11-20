// Copyright Game Production AI Assistant contributors 2026.
#pragma once

#include "CoreMinimal.h"
#include "Models/GPAAApiModels.h"

class FGPAAKnowledgePresenter
{
public:
    static FString FormatKnowledgeStatus(const FGPAAKnowledgeStatus& Status);
    static FString FormatIndexSummary(const FGPAAIndexStatistics& Statistics);
    static FString FormatIndexActivity(const FGPAAIndexStatistics& Statistics);
    static bool DidIndexDoWork(const FGPAAIndexStatistics& Statistics);
};
