namespace GPAA.Infrastructure.Inference;

public sealed class OllamaOptions
{
    public string BaseUrl { get; init; } = "http://localhost:11434";
    public string ChatModel { get; init; } = "llama3.1";
    public string EmbeddingModel { get; init; } = "nomic-embed-text";
    public bool UseOllamaEmbeddings { get; init; }
    public bool UseOllamaChat { get; init; }
}
