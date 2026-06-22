using GPAA.Application.Indexing;
using GPAA.Application.Ports;
using GPAA.Application.Projects;
using GPAA.Application.Search;
using GPAA.Core.Indexing;
using GPAA.Core.Projects;
using GPAA.Core.Search;
using GPAA.Infrastructure.Embeddings;
using GPAA.Infrastructure.Files;
using GPAA.Infrastructure.Inference;
using GPAA.Infrastructure.Storage;

var tests = new (string Name, Func<Task> Run)[]
{
    ("SourceChunker creates overlapping chunks", SourceChunkerCreatesOverlappingChunks),
    ("SourceChunker respects max character limit", SourceChunkerRespectsMaxCharacterLimit),
    ("VectorMath ranks identical vectors higher", VectorMathRanksIdenticalVectorsHigher),
    ("SemanticSearchService skips incompatible embedding dimensions", SemanticSearchServiceSkipsIncompatibleEmbeddingDimensions),
    ("LocalProjectFileScanner ignores Unreal generated directories", LocalProjectFileScannerIgnoresGeneratedDirectories),
    ("ProjectIndexingService stores searchable chunks", ProjectIndexingServiceStoresSearchableChunks),
    ("OllamaRuntimeStatusService reports unreachable runtime", OllamaRuntimeStatusServiceReportsUnreachableRuntime)
};

var failed = 0;

foreach (var test in tests)
{
    try
    {
        await test.Run();
        Console.WriteLine($"PASS {test.Name}");
    }
    catch (Exception ex)
    {
        failed++;
        Console.WriteLine($"FAIL {test.Name}");
        Console.WriteLine(ex);
    }
}

if (failed > 0)
{
    Environment.ExitCode = 1;
}

static Task SourceChunkerCreatesOverlappingChunks()
{
    var content = string.Join(Environment.NewLine, Enumerable.Range(1, 12).Select(i => $"line {i}"));
    var chunker = new SourceChunker(maxLinesPerChunk: 5, overlapLines: 2);

    var chunks = chunker.Chunk("Source/Test.cpp", content);

    Assert.Equal(4, chunks.Count);
    Assert.Equal(1, chunks[0].StartLine);
    Assert.Equal(5, chunks[0].EndLine);
    Assert.Equal(4, chunks[1].StartLine);
    Assert.Equal(8, chunks[1].EndLine);
    return Task.CompletedTask;
}

static Task SourceChunkerRespectsMaxCharacterLimit()
{
    var content = string.Join(Environment.NewLine, Enumerable.Range(1, 10).Select(i => new string('x', 30)));
    var chunker = new SourceChunker(maxLinesPerChunk: 10, overlapLines: 1, maxCharactersPerChunk: 80);

    var chunks = chunker.Chunk("Source/Large.cpp", content);

    Assert.True(chunks.Count > 1, "Content should be split into multiple chunks.");
    Assert.True(chunks.All(chunk => chunk.Content.Length <= 100), "Chunks should stay near the configured character budget.");
    return Task.CompletedTask;
}

static Task VectorMathRanksIdenticalVectorsHigher()
{
    var left = new[] { 1f, 0f, 1f };
    var same = new[] { 1f, 0f, 1f };
    var different = new[] { 0f, 1f, 0f };

    var sameScore = VectorMath.CosineSimilarity(left, same);
    var differentScore = VectorMath.CosineSimilarity(left, different);

    Assert.True(sameScore > differentScore, "Identical vectors should rank higher than unrelated vectors.");
    return Task.CompletedTask;
}

static async Task SemanticSearchServiceSkipsIncompatibleEmbeddingDimensions()
{
    var repository = new InMemoryKnowledgeRepository(new ProjectIndexSnapshot(
        Array.Empty<IndexedFile>(),
        new[]
        {
            new SourceChunk("chunk-1", "Source/Test.cpp", 1, 1, "test", new[] { 1f, 0f })
        },
        new IndexStatistics(1, 1, 1, DateTimeOffset.UtcNow)));

    var searchService = new SemanticSearchService(
        new FixedEmbeddingProvider(new[] { 1f, 0f, 0f }),
        repository);

    var results = await searchService.SearchAsync(
        new SearchProjectRequest("C:\\Project", "test", 5),
        CancellationToken.None);

    Assert.Equal(0, results.Count);
}

static async Task LocalProjectFileScannerIgnoresGeneratedDirectories()
{
    var root = CreateTempDirectory();
    try
    {
        Directory.CreateDirectory(Path.Combine(root, "Source"));
        Directory.CreateDirectory(Path.Combine(root, "Intermediate"));
        await File.WriteAllTextAsync(Path.Combine(root, "Source", "Player.cpp"), "void Player() {}");
        await File.WriteAllTextAsync(Path.Combine(root, "Intermediate", "Generated.cpp"), "void Generated() {}");

        var scanner = new LocalProjectFileScanner();
        var files = await scanner.ScanAsync(root, CancellationToken.None);

        Assert.Equal(1, files.Count);
        Assert.Equal("Source/Player.cpp", files[0].RelativePath);
    }
    finally
    {
        Directory.Delete(root, recursive: true);
    }
}

static async Task ProjectIndexingServiceStoresSearchableChunks()
{
    var root = CreateTempDirectory();
    var storage = CreateTempDirectory();

    try
    {
        Directory.CreateDirectory(Path.Combine(root, "Source"));
        await File.WriteAllTextAsync(
            Path.Combine(root, "Source", "StaminaComponent.cpp"),
            "void UStaminaComponent::RegenerateStamina() { CurrentStamina += Rate; }");

        var repository = new LocalKnowledgeRepository(storage);
        var embeddingProvider = new DeterministicEmbeddingProvider();
        var indexingService = new ProjectIndexingService(
            new LocalProjectFileScanner(),
            embeddingProvider,
            repository,
            new SourceChunker());

        var statistics = await indexingService.IndexAsync(new IndexProjectRequest(root), CancellationToken.None);
        var searchService = new SemanticSearchService(embeddingProvider, repository);
        var results = await searchService.SearchAsync(new SearchProjectRequest(root, "stamina regeneration", 3), CancellationToken.None);

        Assert.Equal(1, statistics.FilesIndexed);
        Assert.Equal(1, statistics.ChunksIndexed);
        Assert.Equal(1, results.Count);
        Assert.Equal("Source/StaminaComponent.cpp", results[0].RelativePath);
    }
    finally
    {
        Directory.Delete(root, recursive: true);
        Directory.Delete(storage, recursive: true);
    }
}

static async Task OllamaRuntimeStatusServiceReportsUnreachableRuntime()
{
    using var httpClient = new HttpClient
    {
        BaseAddress = new Uri("http://127.0.0.1:9"),
        Timeout = TimeSpan.FromMilliseconds(250)
    };

    var options = new OllamaOptions
    {
        BaseUrl = "http://127.0.0.1:9",
        ChatModel = "llama3.1",
        EmbeddingModel = "nomic-embed-text",
        UseOllamaChat = true,
        UseOllamaEmbeddings = true
    };

    var service = new OllamaRuntimeStatusService(httpClient, options);
    var status = await service.GetStatusAsync(CancellationToken.None);

    Assert.True(!status.IsReachable, "Ollama should be reported unreachable on a closed local port.");
    Assert.Equal(true, status.ChatEnabled);
    Assert.Equal(true, status.EmbeddingsEnabled);
}

static string CreateTempDirectory()
{
    var path = Path.Combine(Path.GetTempPath(), "gpaa-tests", Guid.NewGuid().ToString("N"));
    Directory.CreateDirectory(path);
    return path;
}

internal static class Assert
{
    public static void Equal<T>(T expected, T actual)
    {
        if (!EqualityComparer<T>.Default.Equals(expected, actual))
        {
            throw new InvalidOperationException($"Expected '{expected}', got '{actual}'.");
        }
    }

    public static void True(bool condition, string message)
    {
        if (!condition)
        {
            throw new InvalidOperationException(message);
        }
    }
}

internal sealed class FixedEmbeddingProvider : IEmbeddingProvider
{
    private readonly IReadOnlyList<float> _embedding;

    public FixedEmbeddingProvider(IReadOnlyList<float> embedding)
    {
        _embedding = embedding;
    }

    public Task<IReadOnlyList<float>> EmbedAsync(string input, CancellationToken cancellationToken)
        => Task.FromResult(_embedding);
}

internal sealed class InMemoryKnowledgeRepository : IKnowledgeRepository
{
    private readonly ProjectIndexSnapshot _snapshot;

    public InMemoryKnowledgeRepository(ProjectIndexSnapshot snapshot)
    {
        _snapshot = snapshot;
    }

    public Task ReplaceProjectIndexAsync(
        ProjectId projectId,
        IReadOnlyList<IndexedFile> files,
        IReadOnlyList<SourceChunk> chunks,
        IndexStatistics statistics,
        CancellationToken cancellationToken)
        => Task.CompletedTask;

    public Task<ProjectIndexSnapshot?> GetProjectIndexAsync(ProjectId projectId, CancellationToken cancellationToken)
        => Task.FromResult<ProjectIndexSnapshot?>(_snapshot);
}
