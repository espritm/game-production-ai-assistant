namespace GPAA.Infrastructure.Inference;

public sealed class OllamaOptions
{
    public string BaseUrl { get; init; } = "http://localhost:11434";
    public string ChatModel { get; init; } = "llama3.1";
    public string EmbeddingModel { get; init; } = "nomic-embed-text";
    public int ChatTimeoutSeconds { get; init; } = 240;
    public int ChatMaxOutputTokens { get; init; } = 192;
    public int ChatResponseBudgetSeconds { get; init; } = 90;
    public bool UseOllamaEmbeddings { get; init; }
    public bool UseOllamaChat { get; init; }
}
