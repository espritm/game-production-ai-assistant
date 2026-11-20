using GPAA.Api;
using GPAA.Application.Chat;
using GPAA.Application.Assets;
using GPAA.Application.Indexing;
using GPAA.Application.Ports;
using GPAA.Application.Search;
using GPAA.Core.Indexing;
using GPAA.Core.Projects;
using GPAA.Infrastructure.Embeddings;
using GPAA.Infrastructure.Files;
using GPAA.Infrastructure.Inference;
using GPAA.Infrastructure.Storage;
using GPAA.Core.Chat;

var builder = WebApplication.CreateBuilder(args);

var ollamaOptions = builder.Configuration.GetSection("Ollama").Get<OllamaOptions>() ?? new OllamaOptions();
var chatProviderOptions = builder.Configuration.GetSection("Chat").Get<ChatProviderOptions>() ?? new ChatProviderOptions();
var openAICompatibleOptions = builder.Configuration.GetSection("OpenAICompatible").Get<OpenAICompatibleOptions>() ?? new OpenAICompatibleOptions();
var parentProcessMonitorOptions = new ParentProcessMonitorOptions
{
    GpaaParentProcessId = builder.Configuration.GetValue<int>("GpaaParentProcessId")
};
var storageRoot = builder.Configuration.GetValue<string>("Storage:Root")
    ?? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "GPAA", "indexes");

builder.Services.AddSingleton(ollamaOptions);
builder.Services.AddSingleton(chatProviderOptions);
builder.Services.AddSingleton(openAICompatibleOptions);
builder.Services.AddSingleton(parentProcessMonitorOptions);
builder.Services.AddHostedService<ParentProcessMonitorService>();
builder.Services.AddSingleton(new SourceChunker());
builder.Services.AddSingleton(new SourceContextExtractor());
builder.Services.AddSingleton(new UnrealAssetRegistryDocumentRenderer());
builder.Services.AddSingleton<IProjectFileScanner, LocalProjectFileScanner>();
builder.Services.AddSingleton<IKnowledgeRepository>(_ => new LocalKnowledgeRepository(storageRoot));
builder.Services.AddSingleton<IUnrealAssetMetadataRepository>(_ => new LocalUnrealAssetMetadataRepository(storageRoot));
builder.Services.AddSingleton<IChatHistoryRepository>(_ => new LocalChatHistoryRepository(storageRoot));
builder.Services.AddSingleton(_ => new RuntimeChatSettingsService(storageRoot, chatProviderOptions, ollamaOptions, openAICompatibleOptions));
builder.Services.AddSingleton(_ =>
{
    var httpClient = new HttpClient { BaseAddress = new Uri(ollamaOptions.BaseUrl) };
    return new OllamaRuntimeStatusService(httpClient, ollamaOptions);
});

builder.Services.AddSingleton<IEmbeddingProvider>(services =>
{
    if (!ollamaOptions.UseOllamaEmbeddings)
    {
        return new DeterministicEmbeddingProvider();
    }

    var httpClient = new HttpClient { BaseAddress = new Uri(ollamaOptions.BaseUrl) };
    return new OllamaEmbeddingProvider(httpClient, ollamaOptions);
});

builder.Services.AddSingleton<ConfigurableChatCompletionProvider>();
builder.Services.AddSingleton<IChatCompletionProvider>(services =>
    services.GetRequiredService<ConfigurableChatCompletionProvider>());

builder.Services.AddSingleton<IStreamingChatCompletionProvider>(services =>
    services.GetRequiredService<ConfigurableChatCompletionProvider>());

builder.Services.AddSingleton<ProjectIndexingService>();
builder.Services.AddSingleton<SemanticSearchService>();
builder.Services.AddSingleton<ProjectChatService>();

var app = builder.Build();

app.MapGet("/health", () => Results.Ok(new HealthResponse("ok", DateTimeOffset.UtcNow)));

app.MapGet("/runtime/status", async (
    OllamaRuntimeStatusService ollamaStatusService,
    CancellationToken cancellationToken) =>
{
    Directory.CreateDirectory(storageRoot);
    var ollama = await ollamaStatusService.GetStatusAsync(cancellationToken);
    return Results.Ok(new RuntimeStatusResponse(
        Backend: "ok",
        CheckedAt: DateTimeOffset.UtcNow,
        StorageRoot: Path.GetFullPath(storageRoot),
        Ollama: ollama));
});

app.MapGet("/runtime/chat-settings", async (
    RuntimeChatSettingsService settingsService,
    CancellationToken cancellationToken) =>
{
    var settings = await settingsService.GetAsync(cancellationToken);
    return Results.Ok(ToChatSettingsResponse(settings));
});

app.MapPut("/runtime/chat-settings", async (
    RuntimeChatSettings settings,
    RuntimeChatSettingsService settingsService,
    CancellationToken cancellationToken) =>
{
    var saved = await settingsService.SaveAsync(settings, cancellationToken);
    return Results.Ok(ToChatSettingsResponse(saved));
});

app.MapPost("/runtime/chat-settings", async (
    RuntimeChatSettings settings,
    RuntimeChatSettingsService settingsService,
    CancellationToken cancellationToken) =>
{
    var saved = await settingsService.SaveAsync(settings, cancellationToken);
    return Results.Ok(ToChatSettingsResponse(saved));
});

app.MapPost("/runtime/chat-provider/test", async (
    RuntimeChatSettingsService settingsService,
    IChatCompletionProvider chatCompletionProvider,
    CancellationToken cancellationToken) =>
{
    try
    {
        var settings = await settingsService.GetAsync(cancellationToken);
        var answer = await chatCompletionProvider.CompleteAsync(
            new[]
            {
                new ChatMessage("system", "You are a chat provider health check. Reply with a short confirmation."),
                new ChatMessage("user", "Reply with: GPAA chat provider ready.")
            },
            cancellationToken);

        return Results.Ok(new RuntimeChatProviderTestResponse(
            settings.Provider,
            GetConfiguredChatModel(settings),
            string.IsNullOrWhiteSpace(answer) ? "Provider responded." : answer.Trim(),
            DateTimeOffset.UtcNow));
    }
    catch (TimeoutException ex)
    {
        return Results.Problem(ex.Message, statusCode: StatusCodes.Status504GatewayTimeout);
    }
    catch (TaskCanceledException ex) when (!cancellationToken.IsCancellationRequested)
    {
        return Results.Problem($"The chat provider did not answer before the configured timeout. {ex.Message}", statusCode: StatusCodes.Status504GatewayTimeout);
    }
    catch (InvalidOperationException ex)
    {
        return Results.Problem(ex.Message, statusCode: StatusCodes.Status502BadGateway);
    }
});

app.MapPost("/projects/index", async (
    IndexProjectRequest request,
    ProjectIndexingService indexingService,
    CancellationToken cancellationToken) =>
{
    var statistics = await indexingService.IndexAsync(request, cancellationToken);
    return Results.Ok(statistics);
});

app.MapGet("/projects/index/status", async (
    string projectRoot,
    IKnowledgeRepository repository,
    CancellationToken cancellationToken) =>
{
    var snapshot = await repository.GetProjectIndexAsync(ProjectId.FromPath(projectRoot), cancellationToken);
    return snapshot is null
        ? Results.NotFound(new { message = "Project has not been indexed yet." })
        : Results.Ok(snapshot.Statistics);
});

app.MapGet("/projects/knowledge/status", async (
    string projectRoot,
    IKnowledgeRepository knowledgeRepository,
    IUnrealAssetMetadataRepository assetMetadataRepository,
    CancellationToken cancellationToken) =>
{
    ArgumentException.ThrowIfNullOrWhiteSpace(projectRoot);

    var projectId = ProjectId.FromPath(projectRoot);
    var snapshot = await knowledgeRepository.GetProjectIndexAsync(projectId, cancellationToken);
    var assets = await assetMetadataRepository.ListAsync(projectId, cancellationToken);

    var codeFiles = snapshot?.Files.Count(file => !IsAssetMetadataPath(file.RelativePath)) ?? 0;
    var assetDocuments = snapshot?.Files.Count(file => IsAssetMetadataPath(file.RelativePath)) ?? 0;

    return Results.Ok(new ProjectKnowledgeStatusResponse(
        snapshot?.Statistics,
        snapshot?.Metadata.EmbeddingModel ?? string.Empty,
        codeFiles,
        assets.Count,
        assetDocuments,
        snapshot is not null));
});

app.MapPost("/projects/assets/metadata", async (
    SyncUnrealAssetMetadataRequest request,
    IUnrealAssetMetadataRepository assetMetadataRepository,
    CancellationToken cancellationToken) =>
{
    ArgumentException.ThrowIfNullOrWhiteSpace(request.ProjectRoot);
    await assetMetadataRepository.ReplaceAsync(
        ProjectId.FromPath(request.ProjectRoot),
        request.Assets,
        cancellationToken);

    return Results.Ok(new
    {
        assetsSynced = request.Assets.Count,
        syncedAt = DateTimeOffset.UtcNow
    });
});

app.MapPost("/search", async (
    SearchProjectRequest request,
    SemanticSearchService searchService,
    CancellationToken cancellationToken) =>
{
    var results = await searchService.SearchAsync(request, cancellationToken);
    return Results.Ok(results);
});

app.MapPost("/chat", async (
    ChatProjectRequest request,
    ProjectChatService chatService,
    CancellationToken cancellationToken) =>
{
    try
    {
        var response = await chatService.AskAsync(request, cancellationToken);
        return Results.Ok(response);
    }
    catch (TimeoutException ex)
    {
        return Results.Problem(ex.Message, statusCode: StatusCodes.Status504GatewayTimeout);
    }
    catch (TaskCanceledException ex) when (!cancellationToken.IsCancellationRequested)
    {
        return Results.Problem($"The local chat model did not answer before the configured timeout. {ex.Message}", statusCode: StatusCodes.Status504GatewayTimeout);
    }
    catch (InvalidOperationException ex)
    {
        return Results.Problem(ex.Message, statusCode: StatusCodes.Status502BadGateway);
    }
});

app.MapPost("/chat/stream", async (
    ChatProjectRequest request,
    ProjectChatService chatService,
    HttpContext httpContext,
    CancellationToken cancellationToken) =>
{
    httpContext.Response.Headers.ContentType = "text/event-stream";
    await foreach (var fragment in chatService.AskStreamAsync(request, cancellationToken))
    {
        await httpContext.Response.WriteAsync($"data: {fragment.Replace("\r", string.Empty).Replace("\n", "\\n")}\n\n", cancellationToken);
        await httpContext.Response.Body.FlushAsync(cancellationToken);
    }
});

app.MapGet("/chat/history", async (
    string projectRoot,
    int? limit,
    IChatHistoryRepository historyRepository,
    CancellationToken cancellationToken) =>
{
    var entries = await historyRepository.ListAsync(
        ProjectId.FromPath(projectRoot),
        limit.GetValueOrDefault(20),
        cancellationToken);

    return Results.Ok(entries);
});

app.Run();

static RuntimeChatSettingsResponse ToChatSettingsResponse(RuntimeChatSettings settings)
    => new(
        settings.Provider,
        settings.OpenAIBaseUrl,
        settings.OpenAIModel,
        MaskApiKey(settings.OpenAIBaseUrl, settings.OpenAIApiKey),
        settings.OpenAIApiKeyEnvironmentVariable,
        settings.TimeoutSeconds,
        settings.MaxOutputTokens,
        settings.Temperature);

static string MaskApiKey(string baseUrl, string apiKey)
{
    if (string.IsNullOrWhiteSpace(apiKey)
        || LooksLikeUrl(apiKey)
        || (IsOpenAIBaseUrl(baseUrl) && !LooksLikeOpenAISecretKey(apiKey)))
    {
        return string.Empty;
    }

    return "********";
}

static bool LooksLikeUrl(string value)
    => value.StartsWith("http://", StringComparison.OrdinalIgnoreCase)
        || value.StartsWith("https://", StringComparison.OrdinalIgnoreCase);

static bool IsOpenAIBaseUrl(string baseUrl)
    => Uri.TryCreate(baseUrl, UriKind.Absolute, out var uri)
        && uri.Host.Equals("api.openai.com", StringComparison.OrdinalIgnoreCase);

static bool LooksLikeOpenAISecretKey(string value)
    => value.StartsWith("sk-", StringComparison.Ordinal);

static string GetConfiguredChatModel(RuntimeChatSettings settings)
    => settings.Provider.Equals("OpenAICompatible", StringComparison.OrdinalIgnoreCase)
        ? settings.OpenAIModel
        : settings.Provider;

static bool IsAssetMetadataPath(string relativePath)
    => relativePath.StartsWith(".gpaa/unreal-assets/", StringComparison.OrdinalIgnoreCase);

public sealed record HealthResponse(string Status, DateTimeOffset CheckedAt);
public sealed record RuntimeStatusResponse(
    string Backend,
    DateTimeOffset CheckedAt,
    string StorageRoot,
    OllamaRuntimeStatus Ollama);
public sealed record RuntimeChatSettingsResponse(
    string Provider,
    string OpenAIBaseUrl,
    string OpenAIModel,
    string OpenAIApiKey,
    string OpenAIApiKeyEnvironmentVariable,
    int TimeoutSeconds,
    int MaxOutputTokens,
    float Temperature);
public sealed record RuntimeChatProviderTestResponse(
    string Provider,
    string Model,
    string Message,
    DateTimeOffset TestedAt);
public sealed record ProjectKnowledgeStatusResponse(
    IndexStatistics? Statistics,
    string EmbeddingModel,
    int CodeFilesIndexed,
    int AssetsSynced,
    int AssetDocumentsIndexed,
    bool HasIndex);
