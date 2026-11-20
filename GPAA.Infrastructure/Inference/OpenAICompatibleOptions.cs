namespace GPAA.Infrastructure.Inference;

public sealed class OpenAICompatibleOptions
{
    public string BaseUrl { get; init; } = "https://api.openai.com/v1";
    public string Model { get; init; } = "gpt-4.1-mini";
    public string ApiKey { get; init; } = string.Empty;
    public string ApiKeyEnvironmentVariable { get; init; } = "OPENAI_API_KEY";
    public int TimeoutSeconds { get; init; } = 120;
    public int MaxOutputTokens { get; init; } = 512;
    public float Temperature { get; init; } = 0.2f;
}
