#include "Application/GPAAProjectWorkflow.h"

#include "Presentation/GPAAKnowledgePresenter.h"

namespace
{
    FString FormatSearchStatus(const FGPAAIndexStatistics& Statistics)
    {
        return FGPAAKnowledgePresenter::DidIndexDoWork(Statistics)
            ? FString::Printf(TEXT("%s Searching project..."), *FGPAAKnowledgePresenter::FormatIndexActivity(Statistics))
            : TEXT("Searching project...");
    }

    FString FormatAskStatus(const FGPAAIndexStatistics& Statistics, const FString& ChatProviderName)
    {
        return FGPAAKnowledgePresenter::DidIndexDoWork(Statistics)
            ? FString::Printf(TEXT("%s Asking %s..."), *FGPAAKnowledgePresenter::FormatIndexActivity(Statistics), *ChatProviderName)
            : FString::Printf(TEXT("Asking %s..."), *ChatProviderName);
    }
}

void FGPAAProjectWorkflow::Search(
    TSharedPtr<FGPAAHttpClient> HttpClient,
    const FString& BackendUrl,
    const FString& ProjectRoot,
    const FString& AssetMetadataRequest,
    const FString& Query,
    const FString& SourceFilter,
    FGPAAWorkflowStatusChanged OnStatusChanged,
    FGPAAIndexUpdated OnIndexUpdated,
    FGPAASearchCompleted OnCompleted)
{
    if (!HttpClient.IsValid())
    {
        OnCompleted.ExecuteIfBound(false, TEXT("The local backend client is unavailable."), TArray<FGPAASourceReference>());
        return;
    }

    HttpClient->SyncUnrealAssetMetadata(
        BackendUrl,
        AssetMetadataRequest,
        FGPAARequestCompleted::CreateLambda([HttpClient, BackendUrl, ProjectRoot, Query, SourceFilter, OnStatusChanged, OnIndexUpdated, OnCompleted](bool bSucceeded, const FString& Message)
        {
            if (!bSucceeded)
            {
                OnCompleted.ExecuteIfBound(false, Message, TArray<FGPAASourceReference>());
                return;
            }

            HttpClient->IndexProject(
                BackendUrl,
                ProjectRoot,
                FGPAARequestCompleted::CreateLambda([HttpClient, BackendUrl, ProjectRoot, Query, SourceFilter, OnStatusChanged, OnIndexUpdated, OnCompleted](bool bIndexSucceeded, const FString& IndexMessage)
                {
                    if (!bIndexSucceeded)
                    {
                        OnCompleted.ExecuteIfBound(false, IndexMessage, TArray<FGPAASourceReference>());
                        return;
                    }

                    FGPAAIndexStatistics Statistics;
                    if (!TryParseIndexResponse(IndexMessage, Statistics, FGPAARequestCompleted::CreateLambda([OnCompleted](bool, const FString& ErrorMessage)
                    {
                        OnCompleted.ExecuteIfBound(false, ErrorMessage, TArray<FGPAASourceReference>());
                    })))
                    {
                        return;
                    }

                    OnIndexUpdated.ExecuteIfBound(Statistics);
                    OnStatusChanged.ExecuteIfBound(FormatSearchStatus(Statistics));
                    HttpClient->SearchProject(BackendUrl, ProjectRoot, Query, SourceFilter, OnCompleted);
                }));
        }));
}

void FGPAAProjectWorkflow::Ask(
    TSharedPtr<FGPAAHttpClient> HttpClient,
    const FString& BackendUrl,
    const FString& ProjectRoot,
    const FString& AssetMetadataRequest,
    const FString& Question,
    const FString& SourceFilter,
    const FString& ChatProviderName,
    FGPAAWorkflowStatusChanged OnStatusChanged,
    FGPAAIndexUpdated OnIndexUpdated,
    FGPAAChatCompleted OnCompleted)
{
    if (!HttpClient.IsValid())
    {
        OnCompleted.ExecuteIfBound(false, TEXT("The local backend client is unavailable."), FString(), TArray<FGPAASourceReference>());
        return;
    }

    HttpClient->SyncUnrealAssetMetadata(
        BackendUrl,
        AssetMetadataRequest,
        FGPAARequestCompleted::CreateLambda([HttpClient, BackendUrl, ProjectRoot, Question, SourceFilter, ChatProviderName, OnStatusChanged, OnIndexUpdated, OnCompleted](bool bSucceeded, const FString& Message)
        {
            if (!bSucceeded)
            {
                OnCompleted.ExecuteIfBound(false, Message, FString(), TArray<FGPAASourceReference>());
                return;
            }

            HttpClient->IndexProject(
                BackendUrl,
                ProjectRoot,
                FGPAARequestCompleted::CreateLambda([HttpClient, BackendUrl, ProjectRoot, Question, SourceFilter, ChatProviderName, OnStatusChanged, OnIndexUpdated, OnCompleted](bool bIndexSucceeded, const FString& IndexMessage)
                {
                    if (!bIndexSucceeded)
                    {
                        OnCompleted.ExecuteIfBound(false, IndexMessage, FString(), TArray<FGPAASourceReference>());
                        return;
                    }

                    FGPAAIndexStatistics Statistics;
                    if (!TryParseIndexResponse(IndexMessage, Statistics, FGPAARequestCompleted::CreateLambda([OnCompleted](bool, const FString& ErrorMessage)
                    {
                        OnCompleted.ExecuteIfBound(false, ErrorMessage, FString(), TArray<FGPAASourceReference>());
                    })))
                    {
                        return;
                    }

                    OnIndexUpdated.ExecuteIfBound(Statistics);
                    OnStatusChanged.ExecuteIfBound(FormatAskStatus(Statistics, ChatProviderName));
                    HttpClient->AskQuestion(BackendUrl, ProjectRoot, Question, SourceFilter, OnCompleted);
                }));
        }));
}

void FGPAAProjectWorkflow::RefreshKnowledge(
    TSharedPtr<FGPAAHttpClient> HttpClient,
    const FString& BackendUrl,
    const FString& ProjectRoot,
    const FString& AssetMetadataRequest,
    FGPAAWorkflowStatusChanged OnStatusChanged,
    FGPAAIndexUpdated OnIndexUpdated,
    FGPAARequestCompleted OnCompleted)
{
    if (!HttpClient.IsValid())
    {
        OnCompleted.ExecuteIfBound(false, TEXT("The local backend client is unavailable."));
        return;
    }

    HttpClient->SyncUnrealAssetMetadata(
        BackendUrl,
        AssetMetadataRequest,
        FGPAARequestCompleted::CreateLambda([HttpClient, BackendUrl, ProjectRoot, OnStatusChanged, OnIndexUpdated, OnCompleted](bool bSucceeded, const FString& Message)
        {
            if (!bSucceeded)
            {
                OnCompleted.ExecuteIfBound(false, Message);
                return;
            }

            OnStatusChanged.ExecuteIfBound(TEXT("Indexing project knowledge..."));
            HttpClient->IndexProject(
                BackendUrl,
                ProjectRoot,
                FGPAARequestCompleted::CreateLambda([OnIndexUpdated, OnCompleted](bool bIndexSucceeded, const FString& IndexMessage)
                {
                    if (!bIndexSucceeded)
                    {
                        OnCompleted.ExecuteIfBound(false, IndexMessage);
                        return;
                    }

                    FGPAAIndexStatistics Statistics;
                    if (!TryParseIndexResponse(IndexMessage, Statistics, OnCompleted))
                    {
                        return;
                    }

                    OnIndexUpdated.ExecuteIfBound(Statistics);
                    OnCompleted.ExecuteIfBound(true, FGPAAKnowledgePresenter::DidIndexDoWork(Statistics)
                        ? FGPAAKnowledgePresenter::FormatIndexActivity(Statistics)
                        : TEXT("Project knowledge already up to date."));
                }));
        }));
}

bool FGPAAProjectWorkflow::TryParseIndexResponse(const FString& Message, FGPAAIndexStatistics& OutStatistics, FGPAARequestCompleted OnFailure)
{
    if (FGPAAApiModels::ParseIndexStatisticsResponse(Message, OutStatistics))
    {
        return true;
    }

    OnFailure.ExecuteIfBound(false, TEXT("The backend returned an unexpected index response."));
    return false;
}
