#include "Application/GPAAChatSettingsWorkflow.h"

#include "Services/GPAAChatSettingsValidator.h"

void FGPAAChatSettingsWorkflow::Save(
    TSharedPtr<FGPAAHttpClient> HttpClient,
    const FString& BackendUrl,
    const FGPAAChatSettings& Settings,
    FGPAARequestCompleted OnCompleted)
{
    if (!HttpClient.IsValid())
    {
        OnCompleted.ExecuteIfBound(false, TEXT("The local backend client is unavailable."));
        return;
    }

    FString ValidationError;
    if (!FGPAAChatSettingsValidator::Validate(Settings, ValidationError))
    {
        OnCompleted.ExecuteIfBound(false, ValidationError);
        return;
    }

    HttpClient->SaveChatSettings(BackendUrl, FGPAAApiModels::SerializeChatSettings(Settings), OnCompleted);
}

void FGPAAChatSettingsWorkflow::SaveThenTest(
    TSharedPtr<FGPAAHttpClient> HttpClient,
    const FString& BackendUrl,
    const FGPAAChatSettings& Settings,
    FGPAARequestCompleted OnStatusChanged,
    FGPAARequestCompleted OnCompleted)
{
    Save(
        HttpClient,
        BackendUrl,
        Settings,
        FGPAARequestCompleted::CreateLambda([HttpClient, BackendUrl, Settings, OnStatusChanged, OnCompleted](bool bSucceeded, const FString& Message)
        {
            if (!bSucceeded)
            {
                OnCompleted.ExecuteIfBound(false, Message);
                return;
            }

            OnStatusChanged.ExecuteIfBound(true, FString::Printf(TEXT("Testing %s..."), *Settings.Provider));
            HttpClient->TestChatProvider(BackendUrl, OnCompleted);
        }));
}
