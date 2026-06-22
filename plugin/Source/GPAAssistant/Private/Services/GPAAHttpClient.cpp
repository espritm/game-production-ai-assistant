#include "Services/GPAAHttpClient.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void FGPAAHttpClient::CheckBackend(const FString& BackendUrl, FGPAARequestCompleted OnCompleted)
{
    GetJson(BackendUrl, TEXT("/runtime/status"), OnCompleted);
}

void FGPAAHttpClient::IndexProject(const FString& BackendUrl, const FString& ProjectRoot, FGPAARequestCompleted OnCompleted)
{
    const FString Body = FString::Printf(TEXT("{\"projectRoot\":\"%s\"}"), *ProjectRoot.ReplaceCharWithEscapedChar());
    PostJson(BackendUrl, TEXT("/projects/index"), Body, FGPAARequestCompleted::CreateLambda([OnCompleted](bool bSucceeded, const FString& Message)
    {
        if (!bSucceeded)
        {
            OnCompleted.ExecuteIfBound(false, Message);
            return;
        }

        OnCompleted.ExecuteIfBound(true, FString::Printf(TEXT("Indexing completed: %s"), *Message));
    }));
}

void FGPAAHttpClient::AskQuestion(const FString& BackendUrl, const FString& ProjectRoot, const FString& Question, FGPAARequestCompleted OnCompleted)
{
    const FString Body = FString::Printf(
        TEXT("{\"projectRoot\":\"%s\",\"question\":\"%s\",\"topK\":4}"),
        *ProjectRoot.ReplaceCharWithEscapedChar(),
        *Question.ReplaceCharWithEscapedChar());

    PostJson(BackendUrl, TEXT("/chat"), Body, FGPAARequestCompleted::CreateLambda([OnCompleted](bool bSucceeded, const FString& Message)
    {
        if (!bSucceeded)
        {
            OnCompleted.ExecuteIfBound(false, Message);
            return;
        }

        FString Answer;
        if (ExtractMessage(Message, TEXT("answer"), Answer))
        {
            OnCompleted.ExecuteIfBound(true, Answer);
            return;
        }

        OnCompleted.ExecuteIfBound(true, Message);
    }));
}

void FGPAAHttpClient::PostJson(const FString& BackendUrl, const FString& Route, const FString& Body, FGPAARequestCompleted OnCompleted)
{
    SendJsonRequest(BackendUrl, Route, TEXT("POST"), Body, OnCompleted);
}

void FGPAAHttpClient::GetJson(const FString& BackendUrl, const FString& Route, FGPAARequestCompleted OnCompleted)
{
    SendJsonRequest(BackendUrl, Route, TEXT("GET"), FString(), OnCompleted);
}

void FGPAAHttpClient::SendJsonRequest(const FString& BackendUrl, const FString& Route, const FString& Verb, const FString& Body, FGPAARequestCompleted OnCompleted)
{
    const FString Url = NormalizeBackendUrl(BackendUrl) + Route;
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();

    Request->SetURL(Url);
    Request->SetVerb(Verb);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetTimeout(300.0f);
    if (!Body.IsEmpty())
    {
        Request->SetContentAsString(Body);
    }

    Request->OnProcessRequestComplete().BindLambda([OnCompleted](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
    {
        if (!bWasSuccessful || !Response.IsValid())
        {
            OnCompleted.ExecuteIfBound(false, TEXT("The local backend did not respond."));
            return;
        }

        const int32 StatusCode = Response->GetResponseCode();
        const FString ResponseBody = Response->GetContentAsString();

        if (StatusCode < 200 || StatusCode >= 300)
        {
            OnCompleted.ExecuteIfBound(false, FString::Printf(TEXT("HTTP %d: %s"), StatusCode, *ResponseBody));
            return;
        }

        OnCompleted.ExecuteIfBound(true, ResponseBody);
    });

    if (!Request->ProcessRequest())
    {
        OnCompleted.ExecuteIfBound(false, FString::Printf(TEXT("Could not send request to %s."), *Url));
    }
}

FString FGPAAHttpClient::NormalizeBackendUrl(const FString& BackendUrl)
{
    FString Normalized = BackendUrl.TrimStartAndEnd();
    while (Normalized.EndsWith(TEXT("/")))
    {
        Normalized.LeftChopInline(1);
    }

    return Normalized.IsEmpty() ? TEXT("http://localhost:5000") : Normalized;
}

bool FGPAAHttpClient::ExtractMessage(const FString& ResponseBody, const FString& FieldName, FString& OutMessage)
{
    TSharedPtr<FJsonObject> JsonObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        return false;
    }

    return JsonObject->TryGetStringField(FieldName, OutMessage);
}
