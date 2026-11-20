using System.Net.Http.Json;
using GPAA.Application.Ports;

namespace GPAA.Infrastructure.Inference;

public sealed class OllamaEmbeddingProvider : IEmbeddingProvider
{
    private readonly HttpClient _httpClient;
    private readonly OllamaOptions _options;

    public OllamaEmbeddingProvider(HttpClient httpClient, OllamaOptions options)
    {
        _httpClient = httpClient;
        _options = options;
    }

    public string ModelId => $"ollama:{_options.EmbeddingModel}";

    public async Task<IReadOnlyList<float>> EmbedAsync(string input, CancellationToken cancellationToken)
    {
        var response = await _httpClient.PostAsJsonAsync(
            "/api/embeddings",
            new OllamaEmbeddingRequest(_options.EmbeddingModel, input),
            cancellationToken);

        if (!response.IsSuccessStatusCode)
        {
            var error = await response.Content.ReadAsStringAsync(cancellationToken);
            throw new InvalidOperationException(
                $"Ollama embedding request failed with HTTP {(int)response.StatusCode}: {error}");
        }

        var payload = await response.Content.ReadFromJsonAsync<OllamaEmbeddingResponse>(cancellationToken);
        return payload?.Embedding ?? throw new InvalidOperationException("Ollama returned an empty embedding response.");
    }

    private sealed record OllamaEmbeddingRequest(string Model, string Prompt);
    private sealed record OllamaEmbeddingResponse(float[] Embedding);
}
