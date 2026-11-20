using System.Text.Json;

namespace GPAA.Infrastructure.Inference;

public sealed class RuntimeChatSettingsService
{
    private static readonly JsonSerializerOptions JsonOptions = new(JsonSerializerDefaults.Web)
    {
        WriteIndented = true
    };

    private readonly string _settingsPath;
    private readonly RuntimeChatSettings _defaults;
    private readonly SemaphoreSlim _lock = new(1, 1);

    public RuntimeChatSettingsService(
        string storageRoot,
        ChatProviderOptions chatProviderOptions,
        OllamaOptions ollamaOptions,
        OpenAICompatibleOptions openAIOptions)
    {
        _settingsPath = Path.Combine(storageRoot, "runtime-chat-settings.json");
        _defaults = new RuntimeChatSettings(
            string.IsNullOrWhiteSpace(chatProviderOptions.Provider)
                ? (ollamaOptions.UseOllamaChat ? "Ollama" : "Echo")
                : chatProviderOptions.Provider,
            openAIOptions.BaseUrl,
            openAIOptions.Model,
            openAIOptions.ApiKey,
            openAIOptions.ApiKeyEnvironmentVariable,
            openAIOptions.TimeoutSeconds,
            openAIOptions.MaxOutputTokens,
            openAIOptions.Temperature);
    }

    public async Task<RuntimeChatSettings> GetAsync(CancellationToken cancellationToken)
    {
        await _lock.WaitAsync(cancellationToken);
        try
        {
            return await ReadUnsafeAsync(cancellationToken);
        }
        finally
        {
            _lock.Release();
        }
    }

    public async Task<RuntimeChatSettings> SaveAsync(RuntimeChatSettings settings, CancellationToken cancellationToken)
    {
        await _lock.WaitAsync(cancellationToken);
        try
        {
            var current = await ReadUnsafeAsync(cancellationToken);
            if (settings.OpenAIApiKey == "********")
            {
                settings = settings with { OpenAIApiKey = current.OpenAIApiKey };
            }

            var normalized = Normalize(settings);
            Directory.CreateDirectory(Path.GetDirectoryName(_settingsPath)!);
            await using var stream = File.Create(_settingsPath);
            await JsonSerializer.SerializeAsync(stream, normalized, JsonOptions, cancellationToken);
            return normalized;
        }
        finally
        {
            _lock.Release();
        }
    }

    private async Task<RuntimeChatSettings> ReadUnsafeAsync(CancellationToken cancellationToken)
    {
        if (!File.Exists(_settingsPath))
        {
            return _defaults;
        }

        await using var stream = File.OpenRead(_settingsPath);
        var settings = await JsonSerializer.DeserializeAsync<RuntimeChatSettings>(stream, JsonOptions, cancellationToken);
        return Normalize(settings ?? _defaults);
    }

    private RuntimeChatSettings Normalize(RuntimeChatSettings settings)
    {
        return new RuntimeChatSettings(
            string.IsNullOrWhiteSpace(settings.Provider) ? _defaults.Provider : settings.Provider.Trim(),
            string.IsNullOrWhiteSpace(settings.OpenAIBaseUrl) ? _defaults.OpenAIBaseUrl : settings.OpenAIBaseUrl.Trim(),
            string.IsNullOrWhiteSpace(settings.OpenAIModel) ? _defaults.OpenAIModel : settings.OpenAIModel.Trim(),
            settings.OpenAIApiKey.Trim(),
            string.IsNullOrWhiteSpace(settings.OpenAIApiKeyEnvironmentVariable)
                ? _defaults.OpenAIApiKeyEnvironmentVariable
                : settings.OpenAIApiKeyEnvironmentVariable.Trim(),
            Math.Clamp(settings.TimeoutSeconds, 30, 600),
            Math.Clamp(settings.MaxOutputTokens, 64, 4096),
            Math.Clamp(settings.Temperature, 0.0f, 2.0f));
    }
}
