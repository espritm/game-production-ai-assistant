using GPAA.Application.Ports;
using GPAA.Core.Chat;

namespace GPAA.Infrastructure.Inference;

public sealed class ConfigurableChatCompletionProvider : IChatCompletionProvider, IStreamingChatCompletionProvider
{
    private readonly RuntimeChatSettingsService _settingsService;
    private readonly OllamaOptions _ollamaOptions;

    public ConfigurableChatCompletionProvider(RuntimeChatSettingsService settingsService, OllamaOptions ollamaOptions)
    {
        _settingsService = settingsService;
        _ollamaOptions = ollamaOptions;
    }

    public async Task<string> CompleteAsync(IReadOnlyList<ChatMessage> messages, CancellationToken cancellationToken)
    {
        var settings = await _settingsService.GetAsync(cancellationToken);

        if (settings.Provider.Equals("Echo", StringComparison.OrdinalIgnoreCase))
        {
            return await new ContextEchoChatCompletionProvider().CompleteAsync(messages, cancellationToken);
        }

        if (settings.Provider.Equals("OpenAICompatible", StringComparison.OrdinalIgnoreCase))
        {
            using var httpClient = new HttpClient
            {
                BaseAddress = new Uri(settings.OpenAIBaseUrl),
                Timeout = TimeSpan.FromSeconds(settings.TimeoutSeconds)
            };
            var options = new OpenAICompatibleOptions
            {
                BaseUrl = settings.OpenAIBaseUrl,
                Model = settings.OpenAIModel,
                ApiKey = settings.OpenAIApiKey,
                ApiKeyEnvironmentVariable = settings.OpenAIApiKeyEnvironmentVariable,
                TimeoutSeconds = settings.TimeoutSeconds,
                MaxOutputTokens = settings.MaxOutputTokens,
                Temperature = settings.Temperature
            };

            return await new OpenAICompatibleChatCompletionProvider(httpClient, options).CompleteAsync(messages, cancellationToken);
        }

        if (settings.Provider.Equals("Ollama", StringComparison.OrdinalIgnoreCase))
        {
            using var httpClient = new HttpClient
            {
                BaseAddress = new Uri(_ollamaOptions.BaseUrl),
                Timeout = TimeSpan.FromSeconds(Math.Max(30, _ollamaOptions.ChatTimeoutSeconds))
            };

            return await new OllamaChatCompletionProvider(httpClient, _ollamaOptions).CompleteAsync(messages, cancellationToken);
        }

        throw new InvalidOperationException($"Unknown chat provider '{settings.Provider}'. Supported values are Echo, Ollama, and OpenAICompatible.");
    }

    public async IAsyncEnumerable<string> CompleteStreamAsync(
        IReadOnlyList<ChatMessage> messages,
        [System.Runtime.CompilerServices.EnumeratorCancellation] CancellationToken cancellationToken)
    {
        yield return await CompleteAsync(messages, cancellationToken);
    }
}
