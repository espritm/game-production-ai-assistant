// Copyright Game Production AI Assistant contributors 2026.
#include "Services/GPAAChatSettingsValidator.h"

bool FGPAAChatSettingsValidator::Validate(const FGPAAChatSettings& Settings, FString& OutError)
{
    if (!Settings.Provider.Equals(TEXT("OpenAICompatible"), ESearchCase::IgnoreCase))
    {
        return true;
    }

    const FString ApiKey = Settings.OpenAIApiKey.TrimStartAndEnd();
    const FString BaseUrl = Settings.OpenAIBaseUrl.TrimStartAndEnd();

    if (ApiKey.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase)
        || ApiKey.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
    {
        OutError = TEXT("The OpenAI API key field contains a URL. Paste the API key itself, usually starting with sk- or sk-proj-, not the platform page URL.");
        return false;
    }

    if (BaseUrl.Contains(TEXT("api.openai.com"), ESearchCase::IgnoreCase)
        && !ApiKey.IsEmpty()
        && ApiKey != TEXT("********")
        && !ApiKey.StartsWith(TEXT("sk-"), ESearchCase::CaseSensitive))
    {
        OutError = TEXT("The OpenAI API key does not look like an OpenAI secret key. Paste a key from the OpenAI API keys page, usually starting with sk- or sk-proj-.");
        return false;
    }

    return true;
}
