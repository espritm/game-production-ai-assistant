#pragma once

#include "CoreMinimal.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

struct FGPAASourceReference
{
    FString RelativePath;
    FString Content;
    int32 StartLine = 0;
    int32 EndLine = 0;
    double Score = 0.0;
};

DECLARE_DELEGATE_TwoParams(FGPAARequestCompleted, bool /*bSucceeded*/, const FString& /*Message*/);
DECLARE_DELEGATE_ThreeParams(FGPAASearchCompleted, bool /*bSucceeded*/, const FString& /*Message*/, const TArray<FGPAASourceReference>& /*Results*/);
DECLARE_DELEGATE_FourParams(FGPAAChatCompleted, bool /*bSucceeded*/, const FString& /*Message*/, const FString& /*Answer*/, const TArray<FGPAASourceReference>& /*Sources*/);

class FGPAAHttpClient : public TSharedFromThis<FGPAAHttpClient>
{
public:
    void CheckBackend(const FString& BackendUrl, FGPAARequestCompleted OnCompleted);
    void IndexProject(const FString& BackendUrl, const FString& ProjectRoot, FGPAARequestCompleted OnCompleted);
    void GetChatHistory(const FString& BackendUrl, const FString& ProjectRoot, FGPAARequestCompleted OnCompleted);
    void SearchProject(const FString& BackendUrl, const FString& ProjectRoot, const FString& Query, FGPAASearchCompleted OnCompleted);
    void AskQuestion(const FString& BackendUrl, const FString& ProjectRoot, const FString& Question, FGPAAChatCompleted OnCompleted);

private:
    void GetJson(const FString& BackendUrl, const FString& Route, FGPAARequestCompleted OnCompleted);
    void PostJson(const FString& BackendUrl, const FString& Route, const FString& Body, FGPAARequestCompleted OnCompleted);
    void SendJsonRequest(const FString& BackendUrl, const FString& Route, const FString& Verb, const FString& Body, FGPAARequestCompleted OnCompleted);
    static FString NormalizeBackendUrl(const FString& BackendUrl);
    static bool ExtractMessage(const FString& ResponseBody, const FString& FieldName, FString& OutMessage);
    static bool ExtractSources(const FString& ResponseBody, TArray<FGPAASourceReference>& OutSources);
    static bool FormatChatHistory(const FString& ResponseBody, FString& OutMessage);
    static bool ParseSearchResults(const FString& ResponseBody, TArray<FGPAASourceReference>& OutResults);
    static FString CollapseWhitespace(const FString& Text);
};
