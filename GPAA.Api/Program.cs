using GPAA.Application.Chat;
using GPAA.Application.Indexing;
using GPAA.Application.Ports;
using GPAA.Application.Search;
using GPAA.Core.Indexing;
using GPAA.Core.Projects;
using GPAA.Infrastructure.Embeddings;
using GPAA.Infrastructure.Files;
using GPAA.Infrastructure.Inference;
using GPAA.Infrastructure.Storage;

var builder = WebApplication.CreateBuilder(args);

var ollamaOptions = builder.Configuration.GetSection("Ollama").Get<OllamaOptions>() ?? new OllamaOptions();
var storageRoot = builder.Configuration.GetValue<string>("Storage:Root")
    ?? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "GPAA", "indexes");

builder.Services.AddSingleton(ollamaOptions);
builder.Services.AddSingleton(new SourceChunker());
builder.Services.AddSingleton<IProjectFileScanner, LocalProjectFileScanner>();
builder.Services.AddSingleton<IKnowledgeRepository>(_ => new LocalKnowledgeRepository(storageRoot));
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

builder.Services.AddSingleton<IChatCompletionProvider>(services =>
{
    if (!ollamaOptions.UseOllamaChat)
    {
        return new ContextEchoChatCompletionProvider();
    }

    var httpClient = new HttpClient { BaseAddress = new Uri(ollamaOptions.BaseUrl) };
    return new OllamaChatCompletionProvider(httpClient, ollamaOptions);
});

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
    var answer = await chatService.AskAsync(request, cancellationToken);
    return Results.Ok(new ChatResponse(answer));
});

app.Run();

public sealed record HealthResponse(string Status, DateTimeOffset CheckedAt);
public sealed record ChatResponse(string Answer);
public sealed record RuntimeStatusResponse(
    string Backend,
    DateTimeOffset CheckedAt,
    string StorageRoot,
    OllamaRuntimeStatus Ollama);
