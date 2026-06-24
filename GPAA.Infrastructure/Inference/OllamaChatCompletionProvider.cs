using System.Net.Http.Json;
using System.Text.Json;
using GPAA.Application.Ports;
using GPAA.Core.Chat;

namespace GPAA.Infrastructure.Inference;

public sealed class OllamaChatCompletionProvider : IChatCompletionProvider, IStreamingChatCompletionProvider
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

    public async IAsyncEnumerable<string> CompleteStreamAsync(
        IReadOnlyList<ChatMessage> messages,
        [System.Runtime.CompilerServices.EnumeratorCancellation] CancellationToken cancellationToken)
    {
        var response = await _httpClient.PostAsJsonAsync(
            "/api/chat",
            new OllamaChatRequest(
                _options.ChatModel,
                messages.Select(message => new OllamaChatMessage(message.Role, message.Content)).ToArray(),
                Stream: true),
            cancellationToken);

        response.EnsureSuccessStatusCode();

        await using var stream = await response.Content.ReadAsStreamAsync(cancellationToken);
        using var reader = new StreamReader(stream);

        while (!reader.EndOfStream)
        {
            cancellationToken.ThrowIfCancellationRequested();
            var line = await reader.ReadLineAsync(cancellationToken);
            if (string.IsNullOrWhiteSpace(line))
            {
                continue;
            }

            var payload = JsonSerializer.Deserialize<OllamaChatResponse>(line, new JsonSerializerOptions(JsonSerializerDefaults.Web));
            var content = payload?.Message?.Content;
            if (!string.IsNullOrEmpty(content))
            {
                yield return content;
            }
        }
    }

    private sealed record OllamaChatRequest(string Model, OllamaChatMessage[] Messages, bool Stream);
    private sealed record OllamaChatMessage(string Role, string Content);
    private sealed record OllamaChatResponse(OllamaChatMessage? Message);
}
