#include "Services/GPAAHttpClient.h"

#include "GenericPlatform/GenericPlatformHttp.h"
#include "Logging/LogMacros.h"
#include "Models/GPAAApiModels.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPAAHttpClient, Log, All);

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

void FGPAAHttpClient::SyncUnrealAssetMetadata(const FString& BackendUrl, const FString& RequestBody, FGPAARequestCompleted OnCompleted)
{
    PostJson(BackendUrl, TEXT("/projects/assets/metadata"), RequestBody, OnCompleted);
}

void FGPAAHttpClient::GetKnowledgeStatus(const FString& BackendUrl, const FString& ProjectRoot, FGPAARequestCompleted OnCompleted)
{
    const FString Route = FString::Printf(
        TEXT("/projects/knowledge/status?projectRoot=%s"),
        *FGenericPlatformHttp::UrlEncode(ProjectRoot));

    GetJson(BackendUrl, Route, OnCompleted);
}

void FGPAAHttpClient::GetChatSettings(const FString& BackendUrl, FGPAARequestCompleted OnCompleted)
{
    GetJson(BackendUrl, TEXT("/runtime/chat-settings"), OnCompleted);
}

void FGPAAHttpClient::SaveChatSettings(const FString& BackendUrl, const FString& RequestBody, FGPAARequestCompleted OnCompleted)
{
    PostJson(BackendUrl, TEXT("/runtime/chat-settings"), RequestBody, OnCompleted);
}

void FGPAAHttpClient::TestChatProvider(const FString& BackendUrl, FGPAARequestCompleted OnCompleted)
{
    PostJson(BackendUrl, TEXT("/runtime/chat-provider/test"), TEXT("{}"), FGPAARequestCompleted::CreateLambda([OnCompleted](bool bSucceeded, const FString& Message)
    {
        if (!bSucceeded)
        {
            OnCompleted.ExecuteIfBound(false, Message);
            return;
        }

        FGPAAChatProviderTestResult Result;
        if (!FGPAAApiModels::ParseChatProviderTestResult(Message, Result))
        {
            OnCompleted.ExecuteIfBound(false, TEXT("The backend returned an unexpected provider test response."));
            return;
        }

        const FString ModelSuffix = Result.Model.IsEmpty() ? FString() : FString::Printf(TEXT(" (%s)"), *Result.Model);
        const FString Confirmation = Result.Message.IsEmpty() ? TEXT("Provider responded.") : Result.Message;
        OnCompleted.ExecuteIfBound(true, FString::Printf(TEXT("%s%s is ready. %s"), *Result.Provider, *ModelSuffix, *Confirmation));
    }));
}

void FGPAAHttpClient::GetChatHistory(const FString& BackendUrl, const FString& ProjectRoot, FGPAARequestCompleted OnCompleted)
{
    const FString Route = FString::Printf(
        TEXT("/chat/history?projectRoot=%s&limit=10"),
        *FGenericPlatformHttp::UrlEncode(ProjectRoot));

    GetJson(BackendUrl, Route, FGPAARequestCompleted::CreateLambda([OnCompleted](bool bSucceeded, const FString& Message)
    {
        if (!bSucceeded)
        {
            OnCompleted.ExecuteIfBound(false, Message);
            return;
        }

        TArray<FGPAAChatHistoryEntry> Entries;
        if (FGPAAApiModels::ParseChatHistoryResponse(Message, Entries))
        {
            OnCompleted.ExecuteIfBound(true, FormatChatHistory(Entries));
            return;
        }

        OnCompleted.ExecuteIfBound(false, TEXT("The backend returned an unexpected history response."));
    }));
}

void FGPAAHttpClient::SearchProject(const FString& BackendUrl, const FString& ProjectRoot, const FString& Query, const FString& SourceFilter, FGPAASearchCompleted OnCompleted)
{
    const FString Body = FString::Printf(
        TEXT("{\"projectRoot\":\"%s\",\"query\":\"%s\",\"topK\":8,\"sourceFilter\":\"%s\"}"),
        *ProjectRoot.ReplaceCharWithEscapedChar(),
        *Query.ReplaceCharWithEscapedChar(),
        *SourceFilter.ReplaceCharWithEscapedChar());

    PostJson(BackendUrl, TEXT("/search"), Body, FGPAARequestCompleted::CreateLambda([OnCompleted](bool bSucceeded, const FString& Message)
    {
        if (!bSucceeded)
        {
            OnCompleted.ExecuteIfBound(false, Message, TArray<FGPAASourceReference>());
            return;
        }

        FGPAASearchResponse Response;
        if (FGPAAApiModels::ParseSearchResponse(Message, Response))
        {
            const FString DisplayMessage = Response.Results.IsEmpty()
                ? TEXT("No matching source chunks were found.")
                : FString::Printf(TEXT("%d matching source chunk(s) found."), Response.Results.Num());

            OnCompleted.ExecuteIfBound(true, DisplayMessage, Response.Results);
            return;
        }

        OnCompleted.ExecuteIfBound(false, TEXT("The backend returned an unexpected search response."), TArray<FGPAASourceReference>());
    }));
}

void FGPAAHttpClient::AskQuestion(const FString& BackendUrl, const FString& ProjectRoot, const FString& Question, const FString& SourceFilter, FGPAAChatCompleted OnCompleted)
{
    const FString Body = FString::Printf(
        TEXT("{\"projectRoot\":\"%s\",\"question\":\"%s\",\"topK\":4,\"sourceFilter\":\"%s\"}"),
        *ProjectRoot.ReplaceCharWithEscapedChar(),
        *Question.ReplaceCharWithEscapedChar(),
        *SourceFilter.ReplaceCharWithEscapedChar());

    PostJson(BackendUrl, TEXT("/chat"), Body, FGPAARequestCompleted::CreateLambda([OnCompleted](bool bSucceeded, const FString& Message)
    {
        if (!bSucceeded)
        {
            OnCompleted.ExecuteIfBound(false, Message, FString(), TArray<FGPAASourceReference>());
            return;
        }

        FGPAAChatResponse Response;
        if (FGPAAApiModels::ParseChatResponse(Message, Response))
        {
            OnCompleted.ExecuteIfBound(true, TEXT("Answer generated."), Response.Answer, Response.Sources);
            return;
        }

        OnCompleted.ExecuteIfBound(false, TEXT("The backend returned an unexpected chat response."), FString(), TArray<FGPAASourceReference>());
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
    Request->SetActivityTimeout(300.0f);
    if (!Body.IsEmpty())
    {
        Request->SetContentAsString(Body);
    }

    UE_LOG(LogGPAAHttpClient, Log, TEXT("%s %s body=%d chars"), *Verb, *Url, Body.Len());

    Request->OnProcessRequestComplete().BindLambda([OnCompleted, Url, Verb](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
    {
        if (!bWasSuccessful || !Response.IsValid())
        {
            UE_LOG(LogGPAAHttpClient, Warning, TEXT("%s %s failed before a valid HTTP response."), *Verb, *Url);
            OnCompleted.ExecuteIfBound(false, TEXT("The local backend did not respond."));
            return;
        }

        const int32 StatusCode = Response->GetResponseCode();
        const FString ResponseBody = Response->GetContentAsString();
        UE_LOG(LogGPAAHttpClient, Log, TEXT("%s %s completed with HTTP %d, response=%d chars"), *Verb, *Url, StatusCode, ResponseBody.Len());

        if (StatusCode < 200 || StatusCode >= 300)
        {
            UE_LOG(LogGPAAHttpClient, Warning, TEXT("%s %s returned HTTP %d: %s"), *Verb, *Url, StatusCode, *ResponseBody.Left(1000));
            OnCompleted.ExecuteIfBound(false, FGPAAApiModels::FormatHttpError(StatusCode, ResponseBody));
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

FString FGPAAHttpClient::FormatChatHistory(const TArray<FGPAAChatHistoryEntry>& Entries)
{
    if (Entries.IsEmpty())
    {
        return TEXT("No local chat history yet.");
    }

    TArray<FString> Lines;
    Lines.Add(TEXT("Recent local chat history:"));

    for (const FGPAAChatHistoryEntry& Entry : Entries)
    {
        Lines.Add(FString::Printf(TEXT("\n%s\nQ: %s\nA: %s"), *Entry.CreatedAt, *Entry.Question, *Entry.Answer));
    }

    return FString::Join(Lines, TEXT("\n"));
}
