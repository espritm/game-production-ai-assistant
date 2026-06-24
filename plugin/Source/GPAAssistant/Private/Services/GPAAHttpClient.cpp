#include "Services/GPAAHttpClient.h"

#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformHttp.h"
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

        FString FormattedHistory;
        if (FormatChatHistory(Message, FormattedHistory))
        {
            OnCompleted.ExecuteIfBound(true, FormattedHistory);
            return;
        }

        OnCompleted.ExecuteIfBound(false, TEXT("The backend returned an unexpected history response."));
    }));
}

void FGPAAHttpClient::SearchProject(const FString& BackendUrl, const FString& ProjectRoot, const FString& Query, FGPAASearchCompleted OnCompleted)
{
    const FString Body = FString::Printf(
        TEXT("{\"projectRoot\":\"%s\",\"query\":\"%s\",\"topK\":8}"),
        *ProjectRoot.ReplaceCharWithEscapedChar(),
        *Query.ReplaceCharWithEscapedChar());

    PostJson(BackendUrl, TEXT("/search"), Body, FGPAARequestCompleted::CreateLambda([OnCompleted](bool bSucceeded, const FString& Message)
    {
        if (!bSucceeded)
        {
            OnCompleted.ExecuteIfBound(false, Message, TArray<FGPAASourceReference>());
            return;
        }

        TArray<FGPAASourceReference> Results;
        if (ParseSearchResults(Message, Results))
        {
            const FString DisplayMessage = Results.IsEmpty()
                ? TEXT("No matching source chunks were found.")
                : FString::Printf(TEXT("%d matching source chunk(s) found."), Results.Num());

            OnCompleted.ExecuteIfBound(true, DisplayMessage, Results);
            return;
        }

        OnCompleted.ExecuteIfBound(false, TEXT("The backend returned an unexpected search response."), TArray<FGPAASourceReference>());
    }));
}

void FGPAAHttpClient::AskQuestion(const FString& BackendUrl, const FString& ProjectRoot, const FString& Question, FGPAAChatCompleted OnCompleted)
{
    const FString Body = FString::Printf(
        TEXT("{\"projectRoot\":\"%s\",\"question\":\"%s\",\"topK\":4}"),
        *ProjectRoot.ReplaceCharWithEscapedChar(),
        *Question.ReplaceCharWithEscapedChar());

    PostJson(BackendUrl, TEXT("/chat"), Body, FGPAARequestCompleted::CreateLambda([OnCompleted](bool bSucceeded, const FString& Message)
    {
        if (!bSucceeded)
        {
            OnCompleted.ExecuteIfBound(false, Message, FString(), TArray<FGPAASourceReference>());
            return;
        }

        FString Answer;
        if (ExtractMessage(Message, TEXT("answer"), Answer))
        {
            TArray<FGPAASourceReference> Sources;
            ExtractSources(Message, Sources);

            OnCompleted.ExecuteIfBound(true, TEXT("Answer generated."), Answer, Sources);
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

bool FGPAAHttpClient::ExtractSources(const FString& ResponseBody, TArray<FGPAASourceReference>& OutSources)
{
    TSharedPtr<FJsonObject> JsonObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Sources = nullptr;
    if (!JsonObject->TryGetArrayField(TEXT("sources"), Sources))
    {
        return false;
    }

    for (const TSharedPtr<FJsonValue>& SourceValue : *Sources)
    {
        const TSharedPtr<FJsonObject> SourceObject = SourceValue->AsObject();
        if (!SourceObject.IsValid())
        {
            continue;
        }

        FGPAASourceReference Source;

        SourceObject->TryGetStringField(TEXT("relativePath"), Source.RelativePath);
        SourceObject->TryGetStringField(TEXT("content"), Source.Content);
        SourceObject->TryGetNumberField(TEXT("startLine"), Source.StartLine);
        SourceObject->TryGetNumberField(TEXT("endLine"), Source.EndLine);
        SourceObject->TryGetNumberField(TEXT("score"), Source.Score);

        if (!Source.RelativePath.IsEmpty())
        {
            OutSources.Add(Source);
        }
    }

    return !OutSources.IsEmpty();
}

bool FGPAAHttpClient::FormatChatHistory(const FString& ResponseBody, FString& OutMessage)
{
    TArray<TSharedPtr<FJsonValue>> Entries;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);

    if (!FJsonSerializer::Deserialize(Reader, Entries))
    {
        return false;
    }

    if (Entries.IsEmpty())
    {
        OutMessage = TEXT("No local chat history yet.");
        return true;
    }

    TArray<FString> Lines;
    Lines.Add(TEXT("Recent local chat history:"));

    for (const TSharedPtr<FJsonValue>& EntryValue : Entries)
    {
        const TSharedPtr<FJsonObject> EntryObject = EntryValue->AsObject();
        if (!EntryObject.IsValid())
        {
            continue;
        }

        FString Question;
        FString Answer;
        FString CreatedAt;
        EntryObject->TryGetStringField(TEXT("question"), Question);
        EntryObject->TryGetStringField(TEXT("answer"), Answer);
        EntryObject->TryGetStringField(TEXT("createdAt"), CreatedAt);

        Answer = CollapseWhitespace(Answer);
        if (Answer.Len() > 360)
        {
            Answer = Answer.Left(360) + TEXT("...");
        }

        Lines.Add(FString::Printf(TEXT("\n%s\nQ: %s\nA: %s"), *CreatedAt, *Question, *Answer));
    }

    OutMessage = FString::Join(Lines, TEXT("\n"));
    return true;
}

bool FGPAAHttpClient::ParseSearchResults(const FString& ResponseBody, TArray<FGPAASourceReference>& OutResults)
{
    TArray<TSharedPtr<FJsonValue>> Results;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);

    if (!FJsonSerializer::Deserialize(Reader, Results))
    {
        return false;
    }

    for (const TSharedPtr<FJsonValue>& ResultValue : Results)
    {
        const TSharedPtr<FJsonObject> ResultObject = ResultValue->AsObject();
        if (!ResultObject.IsValid())
        {
            continue;
        }

        FGPAASourceReference Result;

        ResultObject->TryGetStringField(TEXT("relativePath"), Result.RelativePath);
        ResultObject->TryGetStringField(TEXT("content"), Result.Content);
        ResultObject->TryGetNumberField(TEXT("startLine"), Result.StartLine);
        ResultObject->TryGetNumberField(TEXT("endLine"), Result.EndLine);
        ResultObject->TryGetNumberField(TEXT("score"), Result.Score);

        if (Result.RelativePath.IsEmpty())
        {
            continue;
        }

        Result.Content = CollapseWhitespace(Result.Content);
        if (Result.Content.Len() > 420)
        {
            Result.Content = Result.Content.Left(420) + TEXT("...");
        }

        OutResults.Add(Result);
    }

    return true;
}

FString FGPAAHttpClient::CollapseWhitespace(const FString& Text)
{
    FString Result;
    Result.Reserve(Text.Len());

    bool bPreviousWasWhitespace = false;
    for (TCHAR Character : Text)
    {
        const bool bIsWhitespace = FChar::IsWhitespace(Character);
        if (bIsWhitespace)
        {
            if (!bPreviousWasWhitespace)
            {
                Result.AppendChar(TEXT(' '));
            }
        }
        else
        {
            Result.AppendChar(Character);
        }

        bPreviousWasWhitespace = bIsWhitespace;
    }

    return Result.TrimStartAndEnd();
}
