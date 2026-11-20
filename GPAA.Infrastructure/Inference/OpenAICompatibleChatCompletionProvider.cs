using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Text.Json;
using System.Text.Json.Serialization;
using GPAA.Application.Ports;
using GPAA.Core.Chat;

namespace GPAA.Infrastructure.Inference;

public sealed class OpenAICompatibleChatCompletionProvider : IChatCompletionProvider, IStreamingChatCompletionProvider
{
    private readonly HttpClient _httpClient;
    private readonly OpenAICompatibleOptions _options;

    public OpenAICompatibleChatCompletionProvider(HttpClient httpClient, OpenAICompatibleOptions options)
    {
        _httpClient = httpClient;
        _options = options;
        EnsureDirectoryBaseAddress(_httpClient);

        var apiKey = ResolveApiKey(options);
        if (string.IsNullOrWhiteSpace(apiKey))
        {
            throw new InvalidOperationException(
                $"Cloud chat is configured but no API key was found. Set OpenAICompatible:ApiKey or the {options.ApiKeyEnvironmentVariable} environment variable.");
        }

        if (LooksLikeUrl(apiKey))
        {
            throw new InvalidOperationException(
                "Cloud chat API key is invalid: it looks like a URL. Replace the masked API key in the Unreal plugin settings with the actual secret key, usually starting with sk- or sk-proj-.");
        }

        if (IsOpenAIBaseUrl(options.BaseUrl) && !LooksLikeOpenAISecretKey(apiKey))
        {
            throw new InvalidOperationException(
                "OpenAI chat is configured, but the stored API key does not look like an OpenAI secret key. Paste a key from the OpenAI API keys page, usually starting with sk- or sk-proj-.");
        }

        _httpClient.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", apiKey);
    }

    public async Task<string> CompleteAsync(IReadOnlyList<ChatMessage> messages, CancellationToken cancellationToken)
    {
        var response = await _httpClient.PostAsJsonAsync(
            "chat/completions",
            new OpenAICompatibleChatRequest(
                _options.Model,
                messages.Select(message => new OpenAICompatibleChatMessage(message.Role, message.Content)).ToArray(),
                Math.Max(64, _options.MaxOutputTokens),
                _options.Temperature),
            cancellationToken);

        if (!response.IsSuccessStatusCode)
        {
            var error = await response.Content.ReadAsStringAsync(cancellationToken);
            throw new InvalidOperationException(
                FormatFailureMessage((int)response.StatusCode, error));
        }

        var payload = await response.Content.ReadFromJsonAsync<OpenAICompatibleChatResponse>(cancellationToken);
        return payload?.Choices.FirstOrDefault()?.Message?.Content
            ?? throw new InvalidOperationException("Cloud chat provider returned an empty response.");
    }

    public async IAsyncEnumerable<string> CompleteStreamAsync(
        IReadOnlyList<ChatMessage> messages,
        [System.Runtime.CompilerServices.EnumeratorCancellation] CancellationToken cancellationToken)
    {
        yield return await CompleteAsync(messages, cancellationToken);
    }

    private static string ResolveApiKey(OpenAICompatibleOptions options)
    {
        if (!string.IsNullOrWhiteSpace(options.ApiKey))
        {
            return options.ApiKey;
        }

        return string.IsNullOrWhiteSpace(options.ApiKeyEnvironmentVariable)
            ? string.Empty
            : Environment.GetEnvironmentVariable(options.ApiKeyEnvironmentVariable) ?? string.Empty;
    }

    private static bool LooksLikeUrl(string value)
        => value.StartsWith("http://", StringComparison.OrdinalIgnoreCase)
            || value.StartsWith("https://", StringComparison.OrdinalIgnoreCase);

    private static bool IsOpenAIBaseUrl(string baseUrl)
        => Uri.TryCreate(baseUrl, UriKind.Absolute, out var uri)
            && uri.Host.Equals("api.openai.com", StringComparison.OrdinalIgnoreCase);

    private static bool LooksLikeOpenAISecretKey(string value)
        => value.StartsWith("sk-", StringComparison.Ordinal);

    private static string FormatFailureMessage(int statusCode, string responseBody)
    {
        var message = TryReadProviderError(responseBody, "message");
        var code = TryReadProviderError(responseBody, "code");

        if (statusCode == 401 || code.Equals("invalid_api_key", StringComparison.OrdinalIgnoreCase))
        {
            return "Cloud chat authentication failed. Check that the API key is a valid secret key for the selected provider.";
        }

        if (statusCode == 429 && code.Equals("insufficient_quota", StringComparison.OrdinalIgnoreCase))
        {
            return "Cloud chat quota exceeded. Check the provider billing, project budget, or usage limits, then try again.";
        }

        if (statusCode == 429)
        {
            return string.IsNullOrWhiteSpace(message)
                ? "Cloud chat rate limit reached. Wait a moment or reduce request frequency, then try again."
                : $"Cloud chat rate limit reached: {message}";
        }

        return string.IsNullOrWhiteSpace(message)
            ? $"Cloud chat request failed with HTTP {statusCode}."
            : $"Cloud chat request failed with HTTP {statusCode}: {message}";
    }

    private static string TryReadProviderError(string responseBody, string fieldName)
    {
        if (string.IsNullOrWhiteSpace(responseBody))
        {
            return string.Empty;
        }

        try
        {
            using var document = JsonDocument.Parse(responseBody);
            if (document.RootElement.TryGetProperty("error", out var error)
                && error.TryGetProperty(fieldName, out var field)
                && field.ValueKind == JsonValueKind.String)
            {
                return field.GetString() ?? string.Empty;
            }
        }
        catch (JsonException)
        {
            return string.Empty;
        }

        return string.Empty;
    }

    private static void EnsureDirectoryBaseAddress(HttpClient httpClient)
    {
        var baseAddress = httpClient.BaseAddress;
        if (baseAddress is null || baseAddress.AbsoluteUri.EndsWith("/", StringComparison.Ordinal))
        {
            return;
        }

        httpClient.BaseAddress = new Uri(baseAddress.AbsoluteUri + "/");
    }

    private sealed record OpenAICompatibleChatRequest(
        [property: JsonPropertyName("model")] string Model,
        [property: JsonPropertyName("messages")] OpenAICompatibleChatMessage[] Messages,
        [property: JsonPropertyName("max_tokens")] int MaxTokens,
        [property: JsonPropertyName("temperature")] float Temperature);

    private sealed record OpenAICompatibleChatMessage(
        [property: JsonPropertyName("role")] string Role,
        [property: JsonPropertyName("content")] string Content);

    private sealed record OpenAICompatibleChatResponse(
        [property: JsonPropertyName("choices")] OpenAICompatibleChatChoice[] Choices);

    private sealed record OpenAICompatibleChatChoice(
        [property: JsonPropertyName("message")] OpenAICompatibleChatMessage? Message);
}
