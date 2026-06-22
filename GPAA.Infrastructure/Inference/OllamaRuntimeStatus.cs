namespace GPAA.Infrastructure.Inference;

public sealed record OllamaRuntimeStatus(
    string BaseUrl,
    string ChatModel,
    string EmbeddingModel,
    bool ChatEnabled,
    bool EmbeddingsEnabled,
    bool IsReachable,
    IReadOnlyList<string> InstalledModels,
    string? Error);
