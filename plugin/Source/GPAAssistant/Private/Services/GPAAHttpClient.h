#pragma once

#include "CoreMinimal.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

DECLARE_DELEGATE_TwoParams(FGPAARequestCompleted, bool /*bSucceeded*/, const FString& /*Message*/);

class FGPAAHttpClient : public TSharedFromThis<FGPAAHttpClient>
{
public:
    void CheckBackend(const FString& BackendUrl, FGPAARequestCompleted OnCompleted);
    void IndexProject(const FString& BackendUrl, const FString& ProjectRoot, FGPAARequestCompleted OnCompleted);
    void AskQuestion(const FString& BackendUrl, const FString& ProjectRoot, const FString& Question, FGPAARequestCompleted OnCompleted);

private:
    void GetJson(const FString& BackendUrl, const FString& Route, FGPAARequestCompleted OnCompleted);
    void PostJson(const FString& BackendUrl, const FString& Route, const FString& Body, FGPAARequestCompleted OnCompleted);
    void SendJsonRequest(const FString& BackendUrl, const FString& Route, const FString& Verb, const FString& Body, FGPAARequestCompleted OnCompleted);
    static FString NormalizeBackendUrl(const FString& BackendUrl);
    static bool ExtractMessage(const FString& ResponseBody, const FString& FieldName, FString& OutMessage);
};
