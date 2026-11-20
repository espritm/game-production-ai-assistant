using System.Net.Http.Json;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
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
        var answer = new StringBuilder();
        var budget = TimeSpan.FromSeconds(Math.Max(15, _options.ChatResponseBudgetSeconds));
        using var budgetTokenSource = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        budgetTokenSource.CancelAfter(budget);

        try
        {
            await foreach (var fragment in CompleteStreamAsync(messages, budgetTokenSource.Token))
            {
                answer.Append(fragment);
            }
        }
        catch (OperationCanceledException) when (!cancellationToken.IsCancellationRequested)
        {
            if (answer.Length > 0)
            {
                answer.AppendLine();
                answer.AppendLine();
                answer.AppendLine("[Partial answer: the local model reached the configured response budget before finishing.]");
                return answer.ToString();
            }

            return "The local model did not produce an answer before the configured response budget. Try a smaller/faster Ollama chat model or ask a narrower question.";
        }

        return answer.Length == 0
            ? "The local model returned an empty answer."
            : answer.ToString();
    }

    public async IAsyncEnumerable<string> CompleteStreamAsync(
        IReadOnlyList<ChatMessage> messages,
        [System.Runtime.CompilerServices.EnumeratorCancellation] CancellationToken cancellationToken)
    {
        using var request = new HttpRequestMessage(HttpMethod.Post, "/api/chat")
        {
            Content = JsonContent.Create(new OllamaChatRequest(
                    _options.ChatModel,
                    messages.Select(message => new OllamaChatMessage(message.Role, message.Content)).ToArray(),
                    Stream: true,
                    Options: new OllamaChatOptions(Math.Max(64, _options.ChatMaxOutputTokens), 0.2f)))
        };

        using var response = await _httpClient.SendAsync(
            request,
            HttpCompletionOption.ResponseHeadersRead,
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

            if (payload?.Done == true)
            {
                yield break;
            }
        }
    }

    private sealed record OllamaChatRequest(string Model, OllamaChatMessage[] Messages, bool Stream, OllamaChatOptions Options);
    private sealed record OllamaChatOptions(
        [property: JsonPropertyName("num_predict")] int NumPredict,
        [property: JsonPropertyName("temperature")] float Temperature);
    private sealed record OllamaChatMessage(string Role, string Content);
    private sealed record OllamaChatResponse(OllamaChatMessage? Message, bool Done);
}
