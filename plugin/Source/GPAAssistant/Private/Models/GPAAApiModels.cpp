#include "Models/GPAAApiModels.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    bool ParseJsonObject(const FString& ResponseBody, TSharedPtr<FJsonObject>& OutObject)
    {
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    }

    int32 ReadInt(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName, int32 DefaultValue = 0)
    {
        double NumberValue = 0.0;
        return JsonObject->TryGetNumberField(FieldName, NumberValue)
            ? static_cast<int32>(NumberValue)
            : DefaultValue;
    }

    float ReadFloat(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName, float DefaultValue = 0.0f)
    {
        double NumberValue = 0.0;
        return JsonObject->TryGetNumberField(FieldName, NumberValue)
            ? static_cast<float>(NumberValue)
            : DefaultValue;
    }

    FString CollapseWhitespace(const FString& Text)
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

    bool ReadSourceReference(const TSharedPtr<FJsonObject>& JsonObject, FGPAASourceReference& OutSource)
    {
        if (!JsonObject.IsValid())
        {
            return false;
        }

        JsonObject->TryGetStringField(TEXT("relativePath"), OutSource.RelativePath);
        JsonObject->TryGetStringField(TEXT("content"), OutSource.Content);
        JsonObject->TryGetStringField(TEXT("sourceType"), OutSource.SourceType);
        JsonObject->TryGetNumberField(TEXT("startLine"), OutSource.StartLine);
        JsonObject->TryGetNumberField(TEXT("endLine"), OutSource.EndLine);
        JsonObject->TryGetNumberField(TEXT("score"), OutSource.Score);

        if (OutSource.SourceType.IsEmpty())
        {
            OutSource.SourceType = OutSource.RelativePath.StartsWith(TEXT(".gpaa/unreal-assets/")) ? TEXT("Asset") : TEXT("Code");
        }

        return !OutSource.RelativePath.IsEmpty();
    }

    void ReadSourceArray(const TArray<TSharedPtr<FJsonValue>>& Values, TArray<FGPAASourceReference>& OutSources, bool bCollapseContent)
    {
        for (const TSharedPtr<FJsonValue>& Value : Values)
        {
            FGPAASourceReference Source;
            if (!ReadSourceReference(Value.IsValid() ? Value->AsObject() : nullptr, Source))
            {
                continue;
            }

            if (bCollapseContent)
            {
                Source.Content = CollapseWhitespace(Source.Content);
                if (Source.Content.Len() > 420)
                {
                    Source.Content = Source.Content.Left(420) + TEXT("...");
                }
            }

            OutSources.Add(Source);
        }
    }

    void ReadIndexStatisticsObject(const TSharedPtr<FJsonObject>& JsonObject, FGPAAIndexStatistics& OutStatistics)
    {
        OutStatistics.FilesScanned = ReadInt(JsonObject, TEXT("filesScanned"));
        OutStatistics.FilesIndexed = ReadInt(JsonObject, TEXT("filesIndexed"));
        OutStatistics.ChunksIndexed = ReadInt(JsonObject, TEXT("chunksIndexed"));
        OutStatistics.FilesReused = ReadInt(JsonObject, TEXT("filesReused"));
        OutStatistics.FilesChanged = ReadInt(JsonObject, TEXT("filesChanged"));
        OutStatistics.FilesRemoved = ReadInt(JsonObject, TEXT("filesRemoved"));
        OutStatistics.ChunksReused = ReadInt(JsonObject, TEXT("chunksReused"));
        OutStatistics.ChunksEmbedded = ReadInt(JsonObject, TEXT("chunksEmbedded"));
        JsonObject->TryGetBoolField(TEXT("fullRebuild"), OutStatistics.bFullRebuild);
        JsonObject->TryGetStringField(TEXT("indexedAt"), OutStatistics.IndexedAt);
        JsonObject->TryGetStringField(TEXT("embeddingModel"), OutStatistics.EmbeddingModel);
    }
}

FString FGPAAApiModels::SerializeChatSettings(const FGPAAChatSettings& Settings)
{
    TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetStringField(TEXT("provider"), Settings.Provider);
    JsonObject->SetStringField(TEXT("openAIBaseUrl"), Settings.OpenAIBaseUrl);
    JsonObject->SetStringField(TEXT("openAIModel"), Settings.OpenAIModel);
    JsonObject->SetStringField(TEXT("openAIApiKey"), Settings.OpenAIApiKey);
    JsonObject->SetStringField(TEXT("openAIApiKeyEnvironmentVariable"), Settings.OpenAIApiKeyEnvironmentVariable);
    JsonObject->SetNumberField(TEXT("timeoutSeconds"), Settings.TimeoutSeconds);
    JsonObject->SetNumberField(TEXT("maxOutputTokens"), Settings.MaxOutputTokens);
    JsonObject->SetNumberField(TEXT("temperature"), Settings.Temperature);

    FString Output;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(JsonObject, Writer);
    return Output;
}

bool FGPAAApiModels::ParseErrorResponse(const FString& ResponseBody, FGPAAErrorResponse& OutError)
{
    TSharedPtr<FJsonObject> JsonObject;
    if (!ParseJsonObject(ResponseBody, JsonObject))
    {
        return false;
    }

    JsonObject->TryGetStringField(TEXT("title"), OutError.Title);
    JsonObject->TryGetStringField(TEXT("detail"), OutError.Detail);
    return !OutError.Title.IsEmpty() || !OutError.Detail.IsEmpty();
}

FString FGPAAApiModels::FormatHttpError(int32 StatusCode, const FString& ResponseBody)
{
    FGPAAErrorResponse Error;
    if (ParseErrorResponse(ResponseBody, Error))
    {
        if (!Error.Detail.IsEmpty())
        {
            return FString::Printf(TEXT("HTTP %d: %s"), StatusCode, *Error.Detail);
        }

        if (!Error.Title.IsEmpty())
        {
            return FString::Printf(TEXT("HTTP %d: %s"), StatusCode, *Error.Title);
        }
    }

    return ResponseBody.IsEmpty()
        ? FString::Printf(TEXT("HTTP %d."), StatusCode)
        : FString::Printf(TEXT("HTTP %d: %s"), StatusCode, *ResponseBody);
}

bool FGPAAApiModels::ParseSearchResponse(const FString& ResponseBody, FGPAASearchResponse& OutResponse)
{
    TArray<TSharedPtr<FJsonValue>> Results;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
    if (!FJsonSerializer::Deserialize(Reader, Results))
    {
        return false;
    }

    ReadSourceArray(Results, OutResponse.Results, true);
    return true;
}

bool FGPAAApiModels::ParseChatResponse(const FString& ResponseBody, FGPAAChatResponse& OutResponse)
{
    TSharedPtr<FJsonObject> JsonObject;
    if (!ParseJsonObject(ResponseBody, JsonObject))
    {
        return false;
    }

    if (!JsonObject->TryGetStringField(TEXT("answer"), OutResponse.Answer))
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Sources = nullptr;
    if (JsonObject->TryGetArrayField(TEXT("sources"), Sources))
    {
        ReadSourceArray(*Sources, OutResponse.Sources, false);
    }

    return true;
}

bool FGPAAApiModels::ParseChatHistoryResponse(const FString& ResponseBody, TArray<FGPAAChatHistoryEntry>& OutEntries)
{
    TArray<TSharedPtr<FJsonValue>> Entries;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
    if (!FJsonSerializer::Deserialize(Reader, Entries))
    {
        return false;
    }

    for (const TSharedPtr<FJsonValue>& EntryValue : Entries)
    {
        const TSharedPtr<FJsonObject> EntryObject = EntryValue.IsValid() ? EntryValue->AsObject() : nullptr;
        if (!EntryObject.IsValid())
        {
            continue;
        }

        FGPAAChatHistoryEntry Entry;
        EntryObject->TryGetStringField(TEXT("question"), Entry.Question);
        EntryObject->TryGetStringField(TEXT("answer"), Entry.Answer);
        EntryObject->TryGetStringField(TEXT("createdAt"), Entry.CreatedAt);
        Entry.Answer = CollapseWhitespace(Entry.Answer);
        if (Entry.Answer.Len() > 360)
        {
            Entry.Answer = Entry.Answer.Left(360) + TEXT("...");
        }

        OutEntries.Add(Entry);
    }

    return true;
}

bool FGPAAApiModels::ParseChatSettings(const FString& ResponseBody, FGPAAChatSettings& OutSettings)
{
    TSharedPtr<FJsonObject> JsonObject;
    if (!ParseJsonObject(ResponseBody, JsonObject))
    {
        return false;
    }

    JsonObject->TryGetStringField(TEXT("provider"), OutSettings.Provider);
    JsonObject->TryGetStringField(TEXT("openAIBaseUrl"), OutSettings.OpenAIBaseUrl);
    JsonObject->TryGetStringField(TEXT("openAIModel"), OutSettings.OpenAIModel);
    JsonObject->TryGetStringField(TEXT("openAIApiKey"), OutSettings.OpenAIApiKey);
    JsonObject->TryGetStringField(TEXT("openAIApiKeyEnvironmentVariable"), OutSettings.OpenAIApiKeyEnvironmentVariable);
    OutSettings.TimeoutSeconds = ReadInt(JsonObject, TEXT("timeoutSeconds"), OutSettings.TimeoutSeconds);
    OutSettings.MaxOutputTokens = ReadInt(JsonObject, TEXT("maxOutputTokens"), OutSettings.MaxOutputTokens);
    OutSettings.Temperature = ReadFloat(JsonObject, TEXT("temperature"), OutSettings.Temperature);
    return !OutSettings.Provider.IsEmpty();
}

bool FGPAAApiModels::ParseKnowledgeStatus(const FString& ResponseBody, FGPAAKnowledgeStatus& OutStatus)
{
    TSharedPtr<FJsonObject> JsonObject;
    if (!ParseJsonObject(ResponseBody, JsonObject))
    {
        return false;
    }

    JsonObject->TryGetBoolField(TEXT("hasIndex"), OutStatus.bHasIndex);
    JsonObject->TryGetStringField(TEXT("embeddingModel"), OutStatus.EmbeddingModel);
    OutStatus.CodeFilesIndexed = ReadInt(JsonObject, TEXT("codeFilesIndexed"));
    OutStatus.AssetsSynced = ReadInt(JsonObject, TEXT("assetsSynced"));
    OutStatus.AssetDocumentsIndexed = ReadInt(JsonObject, TEXT("assetDocumentsIndexed"));

    const TSharedPtr<FJsonObject>* Statistics = nullptr;
    if (JsonObject->TryGetObjectField(TEXT("statistics"), Statistics) && Statistics && Statistics->IsValid())
    {
        ReadIndexStatisticsObject(*Statistics, OutStatus.Statistics);
    }

    return true;
}

bool FGPAAApiModels::ParseChatProviderTestResult(const FString& ResponseBody, FGPAAChatProviderTestResult& OutResult)
{
    TSharedPtr<FJsonObject> JsonObject;
    if (!ParseJsonObject(ResponseBody, JsonObject))
    {
        return false;
    }

    JsonObject->TryGetStringField(TEXT("provider"), OutResult.Provider);
    JsonObject->TryGetStringField(TEXT("model"), OutResult.Model);
    JsonObject->TryGetStringField(TEXT("message"), OutResult.Message);
    return !OutResult.Provider.IsEmpty();
}

bool FGPAAApiModels::ParseIndexStatisticsResponse(const FString& ResponseBody, FGPAAIndexStatistics& OutStatistics)
{
    FString Json = ResponseBody;
    Json.RemoveFromStart(TEXT("Indexing completed: "));

    TSharedPtr<FJsonObject> JsonObject;
    if (!ParseJsonObject(Json, JsonObject))
    {
        return false;
    }

    ReadIndexStatisticsObject(JsonObject, OutStatistics);
    return true;
}
