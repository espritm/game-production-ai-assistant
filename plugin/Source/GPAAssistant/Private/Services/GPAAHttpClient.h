#pragma once

#include "CoreMinimal.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Models/GPAAApiModels.h"

DECLARE_DELEGATE_TwoParams(FGPAARequestCompleted, bool /*bSucceeded*/, const FString& /*Message*/);
DECLARE_DELEGATE_ThreeParams(FGPAASearchCompleted, bool /*bSucceeded*/, const FString& /*Message*/, const TArray<FGPAASourceReference>& /*Results*/);
DECLARE_DELEGATE_FourParams(FGPAAChatCompleted, bool /*bSucceeded*/, const FString& /*Message*/, const FString& /*Answer*/, const TArray<FGPAASourceReference>& /*Sources*/);

class FGPAAHttpClient : public TSharedFromThis<FGPAAHttpClient>
{
public:
    void CheckBackend(const FString& BackendUrl, FGPAARequestCompleted OnCompleted);
    void IndexProject(const FString& BackendUrl, const FString& ProjectRoot, FGPAARequestCompleted OnCompleted);
    void SyncUnrealAssetMetadata(const FString& BackendUrl, const FString& RequestBody, FGPAARequestCompleted OnCompleted);
    void GetKnowledgeStatus(const FString& BackendUrl, const FString& ProjectRoot, FGPAARequestCompleted OnCompleted);
    void GetChatSettings(const FString& BackendUrl, FGPAARequestCompleted OnCompleted);
    void SaveChatSettings(const FString& BackendUrl, const FString& RequestBody, FGPAARequestCompleted OnCompleted);
    void TestChatProvider(const FString& BackendUrl, FGPAARequestCompleted OnCompleted);
    void GetChatHistory(const FString& BackendUrl, const FString& ProjectRoot, FGPAARequestCompleted OnCompleted);
    void SearchProject(const FString& BackendUrl, const FString& ProjectRoot, const FString& Query, const FString& SourceFilter, FGPAASearchCompleted OnCompleted);
    void AskQuestion(const FString& BackendUrl, const FString& ProjectRoot, const FString& Question, const FString& SourceFilter, FGPAAChatCompleted OnCompleted);

private:
    void GetJson(const FString& BackendUrl, const FString& Route, FGPAARequestCompleted OnCompleted);
    void PostJson(const FString& BackendUrl, const FString& Route, const FString& Body, FGPAARequestCompleted OnCompleted);
    void SendJsonRequest(const FString& BackendUrl, const FString& Route, const FString& Verb, const FString& Body, FGPAARequestCompleted OnCompleted);
    static FString NormalizeBackendUrl(const FString& BackendUrl);
    static FString FormatChatHistory(const TArray<FGPAAChatHistoryEntry>& Entries);
};
