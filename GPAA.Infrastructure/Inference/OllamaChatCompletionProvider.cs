using System.Net.Http.Json;
using GPAA.Application.Ports;
using GPAA.Core.Chat;

namespace GPAA.Infrastructure.Inference;

public sealed class OllamaChatCompletionProvider : IChatCompletionProvider
{
    private readonly HttpClient _httpClient;
    private readonly OllamaOptions _options;

    public OllamaChatCompletionProvider(HttpClient httpClient, OllamaOptions options)
    {
        _httpClient = httpClient;
        _options = options;
    }

    public async Task<string> CompleteAsync(IReadOnlyList<ChatMessage> messages, CancellationToken cancellationToken)
    {
        var response = await _httpClient.PostAsJsonAsync(
            "/api/chat",
            new OllamaChatRequest(
                _options.ChatModel,
                messages.Select(message => new OllamaChatMessage(message.Role, message.Content)).ToArray(),
                Stream: false),
            cancellationToken);

        response.EnsureSuccessStatusCode();

        var payload = await response.Content.ReadFromJsonAsync<OllamaChatResponse>(cancellationToken);
        return payload?.Message?.Content ?? throw new InvalidOperationException("Ollama returned an empty chat response.");
    }

    private sealed record OllamaChatRequest(string Model, OllamaChatMessage[] Messages, bool Stream);
    private sealed record OllamaChatMessage(string Role, string Content);
    private sealed record OllamaChatResponse(OllamaChatMessage? Message);
}
