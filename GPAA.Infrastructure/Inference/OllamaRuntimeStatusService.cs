using System.Net.Http.Json;

namespace GPAA.Infrastructure.Inference;

public sealed class OllamaRuntimeStatusService
{
    private readonly HttpClient _httpClient;
    private readonly OllamaOptions _options;

    public OllamaRuntimeStatusService(HttpClient httpClient, OllamaOptions options)
    {
        _httpClient = httpClient;
        _options = options;
    }

    public async Task<OllamaRuntimeStatus> GetStatusAsync(CancellationToken cancellationToken)
    {
        try
        {
            var response = await _httpClient.GetAsync("/api/tags", cancellationToken);
            if (!response.IsSuccessStatusCode)
            {
                return CreateUnavailable($"Ollama returned HTTP {(int)response.StatusCode}.");
            }

            var payload = await response.Content.ReadFromJsonAsync<OllamaTagsResponse>(cancellationToken);
            var models = payload?.Models?
                .Select(model => model.Name)
                .Where(name => !string.IsNullOrWhiteSpace(name))
                .OrderBy(name => name, StringComparer.OrdinalIgnoreCase)
                .ToArray() ?? Array.Empty<string>();

            return new OllamaRuntimeStatus(
                _options.BaseUrl,
                _options.ChatModel,
                _options.EmbeddingModel,
                _options.UseOllamaChat,
                _options.UseOllamaEmbeddings,
                IsReachable: true,
                models,
                Error: null);
        }
        catch (Exception ex) when (ex is HttpRequestException or TaskCanceledException)
        {
            return CreateUnavailable(ex.Message);
        }
    }

    private OllamaRuntimeStatus CreateUnavailable(string error)
        => new(
            _options.BaseUrl,
            _options.ChatModel,
            _options.EmbeddingModel,
            _options.UseOllamaChat,
            _options.UseOllamaEmbeddings,
            IsReachable: false,
            Array.Empty<string>(),
            error);

    private sealed record OllamaTagsResponse(OllamaModel[]? Models);
    private sealed record OllamaModel(string Name);
}
