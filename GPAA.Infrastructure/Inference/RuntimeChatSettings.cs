namespace GPAA.Infrastructure.Inference;

public sealed record RuntimeChatSettings(
    string Provider,
    string OpenAIBaseUrl,
    string OpenAIModel,
    string OpenAIApiKey,
    string OpenAIApiKeyEnvironmentVariable,
    int TimeoutSeconds,
    int MaxOutputTokens,
    float Temperature);
