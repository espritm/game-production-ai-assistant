using GPAA.Application.Indexing;
using GPAA.Application.Ports;
using GPAA.Application.Projects;
using GPAA.Application.Chat;
using GPAA.Application.Search;
using GPAA.Core.Chat;
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
    ("ProjectChatService returns retrieved sources", ProjectChatServiceReturnsRetrievedSources),
    ("ProjectChatService stores chat history", ProjectChatServiceStoresChatHistory),
    ("LocalProjectFileScanner ignores Unreal generated directories", LocalProjectFileScannerIgnoresGeneratedDirectories),
    ("ProjectIndexingService stores searchable chunks", ProjectIndexingServiceStoresSearchableChunks),
    ("ProjectIndexingService reuses unchanged chunks", ProjectIndexingServiceReusesUnchangedChunks),
    ("ProjectIndexingService rebuilds when embedding model changes", ProjectIndexingServiceRebuildsWhenEmbeddingModelChanges),
    ("SemanticSearchService refreshes index when files change", SemanticSearchServiceRefreshesIndexWhenFilesChange),
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
        new IndexStatistics(1, 1, 1, DateTimeOffset.UtcNow),
        new ProjectIndexMetadata(1, "fixed:test", DateTimeOffset.UtcNow)));

    var embeddingProvider = new FixedEmbeddingProvider(new[] { 1f, 0f, 0f });
    var searchService = new SemanticSearchService(
        embeddingProvider,
        repository,
        CreateNoOpIndexingService(repository, embeddingProvider));

    var results = await searchService.SearchAsync(
        new SearchProjectRequest("C:\\Project", "test", 5),
        CancellationToken.None);

    Assert.Equal(0, results.Count);
}

static async Task ProjectChatServiceReturnsRetrievedSources()
{
    var repository = new InMemoryKnowledgeRepository(new ProjectIndexSnapshot(
        Array.Empty<IndexedFile>(),
        new[]
        {
            new SourceChunk("chunk-1", "Source/Test.cpp", 1, 3, "void Test() {}", new[] { 1f, 0f, 0f })
        },
        new IndexStatistics(1, 1, 1, DateTimeOffset.UtcNow),
        new ProjectIndexMetadata(1, "fixed:test", DateTimeOffset.UtcNow)));

    var embeddingProvider = new FixedEmbeddingProvider(new[] { 1f, 0f, 0f });
    var searchService = new SemanticSearchService(
        embeddingProvider,
        repository,
        CreateNoOpIndexingService(repository, embeddingProvider));
    var chatService = new ProjectChatService(
        searchService,
        new FixedChatCompletionProvider("answer"),
        new FixedChatCompletionProvider("answer"),
        new InMemoryChatHistoryRepository());

    var response = await chatService.AskAsync(
        new ChatProjectRequest("C:\\Project", "Where is Test?", 3),
        CancellationToken.None);

    Assert.Equal("answer", response.Answer);
    Assert.Equal(1, response.Sources.Count);
    Assert.Equal("Source/Test.cpp", response.Sources[0].RelativePath);
}

static async Task ProjectChatServiceStoresChatHistory()
{
    var root = CreateTempDirectory();
    var storage = CreateTempDirectory();

    try
    {
        Directory.CreateDirectory(Path.Combine(root, "Source"));
        await File.WriteAllTextAsync(Path.Combine(root, "Source", "System.cpp"), "void TestSystem() {}");

        var repository = new LocalKnowledgeRepository(storage);
        var historyRepository = new LocalChatHistoryRepository(storage);
        var embeddingProvider = new DeterministicEmbeddingProvider();
        var indexingService = new ProjectIndexingService(
            new LocalProjectFileScanner(),
            embeddingProvider,
            repository,
            new SourceChunker(),
            new SourceContextExtractor());
        var searchService = new SemanticSearchService(embeddingProvider, repository, indexingService);
        var chatService = new ProjectChatService(
            searchService,
            new FixedChatCompletionProvider("stored answer"),
            new FixedChatCompletionProvider("stored answer"),
            historyRepository);

        await chatService.AskAsync(new ChatProjectRequest(root, "What is TestSystem?", 3), CancellationToken.None);
        var history = await historyRepository.ListAsync(ProjectId.FromPath(root), 10, CancellationToken.None);

        Assert.Equal(1, history.Count);
        Assert.Equal("What is TestSystem?", history[0].Question);
        Assert.Equal("stored answer", history[0].Answer);
    }
    finally
    {
        Directory.Delete(root, recursive: true);
        Directory.Delete(storage, recursive: true);
    }
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
            new SourceChunker(),
            new SourceContextExtractor());

        var statistics = await indexingService.IndexAsync(new IndexProjectRequest(root), CancellationToken.None);
        var searchService = new SemanticSearchService(embeddingProvider, repository, indexingService);
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

static async Task ProjectIndexingServiceReusesUnchangedChunks()
{
    var root = CreateTempDirectory();
    var storage = CreateTempDirectory();

    try
    {
        Directory.CreateDirectory(Path.Combine(root, "Source"));
        await File.WriteAllTextAsync(Path.Combine(root, "Source", "Player.cpp"), "void MovePlayer() {}");

        var repository = new LocalKnowledgeRepository(storage);
        var embeddingProvider = new CountingEmbeddingProvider("fixed:model-a", new[] { 1f, 0f, 0f });
        var indexingService = new ProjectIndexingService(
            new LocalProjectFileScanner(),
            embeddingProvider,
            repository,
            new SourceChunker(),
            new SourceContextExtractor());

        var first = await indexingService.IndexAsync(new IndexProjectRequest(root), CancellationToken.None);
        var second = await indexingService.IndexAsync(new IndexProjectRequest(root), CancellationToken.None);

        Assert.Equal(1, first.ChunksEmbedded);
        Assert.Equal(1, second.FilesReused);
        Assert.Equal(1, second.ChunksReused);
        Assert.Equal(0, second.ChunksEmbedded);
        Assert.Equal(false, second.FullRebuild);
        Assert.Equal(1, embeddingProvider.CallCount);
    }
    finally
    {
        Directory.Delete(root, recursive: true);
        Directory.Delete(storage, recursive: true);
    }
}

static async Task ProjectIndexingServiceRebuildsWhenEmbeddingModelChanges()
{
    var root = CreateTempDirectory();
    var storage = CreateTempDirectory();

    try
    {
        Directory.CreateDirectory(Path.Combine(root, "Source"));
        await File.WriteAllTextAsync(Path.Combine(root, "Source", "Player.cpp"), "void MovePlayer() {}");

        var repository = new LocalKnowledgeRepository(storage);
        var firstService = new ProjectIndexingService(
            new LocalProjectFileScanner(),
            new CountingEmbeddingProvider("fixed:model-a", new[] { 1f, 0f, 0f }),
            repository,
            new SourceChunker(),
            new SourceContextExtractor());
        var secondProvider = new CountingEmbeddingProvider("fixed:model-b", new[] { 0f, 1f, 0f });
        var secondService = new ProjectIndexingService(
            new LocalProjectFileScanner(),
            secondProvider,
            repository,
            new SourceChunker(),
            new SourceContextExtractor());

        await firstService.IndexAsync(new IndexProjectRequest(root), CancellationToken.None);
        var second = await secondService.IndexAsync(new IndexProjectRequest(root), CancellationToken.None);

        Assert.Equal(true, second.FullRebuild);
        Assert.Equal(0, second.FilesReused);
        Assert.Equal(1, second.ChunksEmbedded);
        Assert.Equal("fixed:model-b", second.EmbeddingModel);
        Assert.Equal(1, secondProvider.CallCount);
    }
    finally
    {
        Directory.Delete(root, recursive: true);
        Directory.Delete(storage, recursive: true);
    }
}

static async Task SemanticSearchServiceRefreshesIndexWhenFilesChange()
{
    var root = CreateTempDirectory();
    var storage = CreateTempDirectory();

    try
    {
        Directory.CreateDirectory(Path.Combine(root, "Source"));
        var sourcePath = Path.Combine(root, "Source", "System.cpp");
        await File.WriteAllTextAsync(sourcePath, "void OldSystem() {}");

        var repository = new LocalKnowledgeRepository(storage);
        var embeddingProvider = new DeterministicEmbeddingProvider();
        var indexingService = new ProjectIndexingService(
            new LocalProjectFileScanner(),
            embeddingProvider,
            repository,
            new SourceChunker(),
            new SourceContextExtractor());
        var searchService = new SemanticSearchService(embeddingProvider, repository, indexingService);

        await searchService.SearchAsync(new SearchProjectRequest(root, "old system", 3), CancellationToken.None);
        await File.WriteAllTextAsync(sourcePath, "void NewSystem() {}");

        var results = await searchService.SearchAsync(new SearchProjectRequest(root, "new system", 3), CancellationToken.None);

        Assert.Equal(1, results.Count);
        Assert.True(results[0].Content.Contains("NewSystem", StringComparison.Ordinal), "Search should use the refreshed file content.");
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

static ProjectIndexingService CreateNoOpIndexingService(IKnowledgeRepository repository, IEmbeddingProvider embeddingProvider)
    => new(new EmptyProjectFileScanner(), embeddingProvider, repository, new SourceChunker(), new SourceContextExtractor());

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

    public string ModelId => "fixed:test";

    public Task<IReadOnlyList<float>> EmbedAsync(string input, CancellationToken cancellationToken)
        => Task.FromResult(_embedding);
}

internal sealed class CountingEmbeddingProvider : IEmbeddingProvider
{
    private readonly IReadOnlyList<float> _embedding;

    public CountingEmbeddingProvider(string modelId, IReadOnlyList<float> embedding)
    {
        ModelId = modelId;
        _embedding = embedding;
    }

    public string ModelId { get; }
    public int CallCount { get; private set; }

    public Task<IReadOnlyList<float>> EmbedAsync(string input, CancellationToken cancellationToken)
    {
        CallCount++;
        return Task.FromResult(_embedding);
    }
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
        ProjectIndexMetadata metadata,
        CancellationToken cancellationToken)
        => Task.CompletedTask;

    public Task<ProjectIndexSnapshot?> GetProjectIndexAsync(ProjectId projectId, CancellationToken cancellationToken)
        => Task.FromResult<ProjectIndexSnapshot?>(_snapshot);
}

internal sealed class EmptyProjectFileScanner : IProjectFileScanner
{
    public Task<IReadOnlyList<ProjectFile>> ScanAsync(string projectRoot, CancellationToken cancellationToken)
        => Task.FromResult<IReadOnlyList<ProjectFile>>(Array.Empty<ProjectFile>());
}

internal sealed class FixedChatCompletionProvider : IChatCompletionProvider, IStreamingChatCompletionProvider
{
    private readonly string _answer;

    public FixedChatCompletionProvider(string answer)
    {
        _answer = answer;
    }

    public Task<string> CompleteAsync(IReadOnlyList<ChatMessage> messages, CancellationToken cancellationToken)
        => Task.FromResult(_answer);

    public async IAsyncEnumerable<string> CompleteStreamAsync(
        IReadOnlyList<ChatMessage> messages,
        [System.Runtime.CompilerServices.EnumeratorCancellation] CancellationToken cancellationToken)
    {
        await Task.CompletedTask;
        yield return _answer;
    }
}

internal sealed class InMemoryChatHistoryRepository : IChatHistoryRepository
{
    public Task AddAsync(ProjectId projectId, ChatHistoryEntry entry, CancellationToken cancellationToken)
        => Task.CompletedTask;

    public Task<IReadOnlyList<ChatHistoryEntry>> ListAsync(ProjectId projectId, int limit, CancellationToken cancellationToken)
        => Task.FromResult<IReadOnlyList<ChatHistoryEntry>>(Array.Empty<ChatHistoryEntry>());
}
