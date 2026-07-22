using GPAA.Application.Indexing;
using GPAA.Application.Ports;
using GPAA.Application.Projects;
using GPAA.Application.Assets;
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
    ("SemanticSearchService filters asset results", SemanticSearchServiceFiltersAssetResults),
    ("ProjectChatService returns retrieved sources", ProjectChatServiceReturnsRetrievedSources),
    ("ProjectChatService stores chat history", ProjectChatServiceStoresChatHistory),
    ("LocalProjectFileScanner ignores Unreal generated directories", LocalProjectFileScannerIgnoresGeneratedDirectories),
    ("LocalProjectFileScanner indexes Unreal asset metadata", LocalProjectFileScannerIndexesUnrealAssetMetadata),
    ("ProjectIndexingService stores searchable chunks", ProjectIndexingServiceStoresSearchableChunks),
    ("ProjectIndexingService stores searchable Unreal asset metadata", ProjectIndexingServiceStoresSearchableUnrealAssetMetadata),
    ("ProjectIndexingService stores searchable Asset Registry metadata", ProjectIndexingServiceStoresSearchableAssetRegistryMetadata),
    ("ProjectIndexingService limits embedding input size", ProjectIndexingServiceLimitsEmbeddingInputSize),
    ("ProjectIndexingService reuses unchanged chunks", ProjectIndexingServiceReusesUnchangedChunks),
    ("ProjectIndexingService rebuilds when embedding model changes", ProjectIndexingServiceRebuildsWhenEmbeddingModelChanges),
    ("SemanticSearchService refreshes index when files change", SemanticSearchServiceRefreshesIndexWhenFilesChange),
    ("OpenAICompatibleChatCompletionProvider sends authenticated request", OpenAICompatibleChatCompletionProviderSendsAuthenticatedRequest),
    ("OpenAICompatibleChatCompletionProvider formats quota errors", OpenAICompatibleChatCompletionProviderFormatsQuotaErrors),
    ("OpenAICompatibleChatCompletionProvider rejects URL API keys", OpenAICompatibleChatCompletionProviderRejectsUrlApiKeys),
    ("OpenAICompatibleChatCompletionProvider rejects non OpenAI keys for OpenAI base URL", OpenAICompatibleChatCompletionProviderRejectsNonOpenAIKeysForOpenAIBaseUrl),
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

static async Task SemanticSearchServiceFiltersAssetResults()
{
    var repository = new InMemoryKnowledgeRepository(new ProjectIndexSnapshot(
        Array.Empty<IndexedFile>(),
        new[]
        {
            new SourceChunk("code-1", "Source/Test.cpp", 1, 1, "test", new[] { 1f, 0f, 0f }),
            new SourceChunk("asset-1", ".gpaa/unreal-assets/_Game_BP_Player.asset-metadata", 1, 1, "test", new[] { 1f, 0f, 0f })
        },
        new IndexStatistics(2, 2, 2, DateTimeOffset.UtcNow),
        new ProjectIndexMetadata(1, "fixed:test", DateTimeOffset.UtcNow)));

    var embeddingProvider = new FixedEmbeddingProvider(new[] { 1f, 0f, 0f });
    var searchService = new SemanticSearchService(
        embeddingProvider,
        repository,
        CreateNoOpIndexingService(repository, embeddingProvider));

    var results = await searchService.SearchAsync(
        new SearchProjectRequest("C:\\Project", "test", 5, "Assets"),
        CancellationToken.None);

    Assert.Equal(1, results.Count);
    Assert.Equal("Asset", results[0].SourceType);
    Assert.Equal(".gpaa/unreal-assets/_Game_BP_Player.asset-metadata", results[0].RelativePath);
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
            new EmptyUnrealAssetMetadataRepository(),
            new SourceChunker(),
            new SourceContextExtractor(),
            new UnrealAssetRegistryDocumentRenderer());
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

static async Task LocalProjectFileScannerIndexesUnrealAssetMetadata()
{
    var root = CreateTempDirectory();
    try
    {
        Directory.CreateDirectory(Path.Combine(root, "Content", "Blueprints"));
        await File.WriteAllBytesAsync(Path.Combine(root, "Content", "Blueprints", "BP_PlayerCharacter.uasset"), new byte[] { 1, 2, 3, 4 });

        var scanner = new LocalProjectFileScanner();
        var files = await scanner.ScanAsync(root, CancellationToken.None);

        Assert.Equal(1, files.Count);
        Assert.Equal("Content/Blueprints/BP_PlayerCharacter.uasset", files[0].RelativePath);
        Assert.True(files[0].Content.Contains("AssetType: Blueprint", StringComparison.Ordinal), "Asset metadata should infer Blueprint type.");
        Assert.True(files[0].Content.Contains("AssetName: BP_PlayerCharacter", StringComparison.Ordinal), "Asset metadata should include the asset name.");
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
            new EmptyUnrealAssetMetadataRepository(),
            new SourceChunker(),
            new SourceContextExtractor(),
            new UnrealAssetRegistryDocumentRenderer());

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

static async Task ProjectIndexingServiceStoresSearchableUnrealAssetMetadata()
{
    var root = CreateTempDirectory();
    var storage = CreateTempDirectory();

    try
    {
        Directory.CreateDirectory(Path.Combine(root, "Content", "DataAssets"));
        await File.WriteAllBytesAsync(Path.Combine(root, "Content", "DataAssets", "DA_WeaponBalance.uasset"), new byte[] { 9, 8, 7, 6 });

        var repository = new LocalKnowledgeRepository(storage);
        var embeddingProvider = new DeterministicEmbeddingProvider();
        var indexingService = new ProjectIndexingService(
            new LocalProjectFileScanner(),
            embeddingProvider,
            repository,
            new EmptyUnrealAssetMetadataRepository(),
            new SourceChunker(),
            new SourceContextExtractor(),
            new UnrealAssetRegistryDocumentRenderer());

        var statistics = await indexingService.IndexAsync(new IndexProjectRequest(root), CancellationToken.None);
        var searchService = new SemanticSearchService(embeddingProvider, repository, indexingService);
        var results = await searchService.SearchAsync(new SearchProjectRequest(root, "weapon balance data asset", 3), CancellationToken.None);

        Assert.Equal(1, statistics.FilesIndexed);
        Assert.Equal(1, statistics.ChunksIndexed);
        Assert.True(results.Count > 0, "Asset metadata should be searchable.");
        Assert.Equal("Content/DataAssets/DA_WeaponBalance.uasset", results[0].RelativePath);
    }
    finally
    {
        Directory.Delete(root, recursive: true);
        Directory.Delete(storage, recursive: true);
    }
}

static async Task ProjectIndexingServiceStoresSearchableAssetRegistryMetadata()
{
    var root = CreateTempDirectory();
    var storage = CreateTempDirectory();

    try
    {
        var repository = new LocalKnowledgeRepository(storage);
        var assetRepository = new InMemoryUnrealAssetMetadataRepository(new[]
        {
            new UnrealAssetMetadata(
                "/Game/UI/WBP_Inventory.WBP_Inventory",
                "/Game/UI/WBP_Inventory",
                "/Game/UI",
                "WBP_Inventory",
                "WidgetBlueprint",
                new Dictionary<string, string>
                {
                    ["ParentClass"] = "UserWidget",
                    ["GeneratedClass"] = "/Game/UI/WBP_Inventory.WBP_Inventory_C"
                },
                new[] { "/Game/UI/WBP_InventoryStyle" },
                new[] { "/Game/Maps/L_DebugInventory" })
        });
        var embeddingProvider = new DeterministicEmbeddingProvider();
        var indexingService = new ProjectIndexingService(
            new LocalProjectFileScanner(),
            embeddingProvider,
            repository,
            assetRepository,
            new SourceChunker(),
            new SourceContextExtractor(),
            new UnrealAssetRegistryDocumentRenderer());

        var statistics = await indexingService.IndexAsync(new IndexProjectRequest(root), CancellationToken.None);
        var searchService = new SemanticSearchService(embeddingProvider, repository, indexingService);
        var results = await searchService.SearchAsync(new SearchProjectRequest(root, "inventory style debug map referencer", 3), CancellationToken.None);

        Assert.Equal(1, statistics.FilesIndexed);
        Assert.True(results.Count > 0, "Asset Registry metadata should be searchable.");
        Assert.True(results[0].Content.Contains("AssetClass: WidgetBlueprint", StringComparison.Ordinal), "Registry content should include the asset class.");
        Assert.True(results[0].Content.Contains("Dependency: /Game/UI/WBP_InventoryStyle", StringComparison.Ordinal), "Registry content should include package dependencies.");
        Assert.True(results[0].Content.Contains("Referencer: /Game/Maps/L_DebugInventory", StringComparison.Ordinal), "Registry content should include package referencers.");
    }
    finally
    {
        Directory.Delete(root, recursive: true);
        Directory.Delete(storage, recursive: true);
    }
}

static async Task ProjectIndexingServiceLimitsEmbeddingInputSize()
{
    var root = CreateTempDirectory();
    var storage = CreateTempDirectory();

    try
    {
        Directory.CreateDirectory(Path.Combine(root, "Source"));
        var longLine = new string('A', 10_000);
        await File.WriteAllTextAsync(Path.Combine(root, "Source", "HugeLine.cpp"), $"void HugeLine() {{}}{Environment.NewLine}{longLine}");

        var repository = new LocalKnowledgeRepository(storage);
        var embeddingProvider = new CountingEmbeddingProvider("fixed:model-a", new[] { 1f, 0f, 0f });
        var indexingService = new ProjectIndexingService(
            new LocalProjectFileScanner(),
            embeddingProvider,
            repository,
            new EmptyUnrealAssetMetadataRepository(),
            new SourceChunker(),
            new SourceContextExtractor(),
            new UnrealAssetRegistryDocumentRenderer());

        await indexingService.IndexAsync(new IndexProjectRequest(root), CancellationToken.None);

        Assert.True(embeddingProvider.MaxObservedInputLength <= 1900, "Embedding input should stay below the local model context budget.");
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
            new EmptyUnrealAssetMetadataRepository(),
            new SourceChunker(),
            new SourceContextExtractor(),
            new UnrealAssetRegistryDocumentRenderer());

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
            new EmptyUnrealAssetMetadataRepository(),
            new SourceChunker(),
            new SourceContextExtractor(),
            new UnrealAssetRegistryDocumentRenderer());
        var secondProvider = new CountingEmbeddingProvider("fixed:model-b", new[] { 0f, 1f, 0f });
        var secondService = new ProjectIndexingService(
            new LocalProjectFileScanner(),
            secondProvider,
            repository,
            new EmptyUnrealAssetMetadataRepository(),
            new SourceChunker(),
            new SourceContextExtractor(),
            new UnrealAssetRegistryDocumentRenderer());

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
            new EmptyUnrealAssetMetadataRepository(),
            new SourceChunker(),
            new SourceContextExtractor(),
            new UnrealAssetRegistryDocumentRenderer());
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

static async Task OpenAICompatibleChatCompletionProviderSendsAuthenticatedRequest()
{
    CapturingHttpMessageHandler.Reset();
    using var httpClient = new HttpClient(new CapturingHttpMessageHandler(
        System.Net.HttpStatusCode.OK,
        """{"choices":[{"message":{"role":"assistant","content":"Cloud answer"}}]}"""))
    {
        BaseAddress = new Uri("https://example.test/v1")
    };
    var provider = new OpenAICompatibleChatCompletionProvider(
        httpClient,
        new OpenAICompatibleOptions
        {
            ApiKey = "sk-test-key",
            Model = "test-model"
        });

    var answer = await provider.CompleteAsync(
        new[] { new ChatMessage("user", "Hello") },
        CancellationToken.None);

    Assert.Equal("Cloud answer", answer);
    Assert.Equal("Bearer", CapturingHttpMessageHandler.LastAuthorizationScheme);
    Assert.Equal("sk-test-key", CapturingHttpMessageHandler.LastAuthorizationParameter);
    Assert.Equal("/v1/chat/completions", CapturingHttpMessageHandler.LastRequestPath);
}

static async Task OpenAICompatibleChatCompletionProviderFormatsQuotaErrors()
{
    using var httpClient = new HttpClient(new CapturingHttpMessageHandler(
        System.Net.HttpStatusCode.TooManyRequests,
        """{"error":{"message":"You exceeded your current quota.","code":"insufficient_quota"}}"""))
    {
        BaseAddress = new Uri("https://example.test/v1")
    };
    var provider = new OpenAICompatibleChatCompletionProvider(
        httpClient,
        new OpenAICompatibleOptions
        {
            ApiKey = "sk-test-key",
            Model = "test-model"
        });

    try
    {
        await provider.CompleteAsync(new[] { new ChatMessage("user", "Hello") }, CancellationToken.None);
    }
    catch (InvalidOperationException ex)
    {
        Assert.True(ex.Message.Contains("quota exceeded", StringComparison.OrdinalIgnoreCase), "Quota errors should be readable.");
        return;
    }

    throw new InvalidOperationException("Quota errors should fail with a readable InvalidOperationException.");
}

static Task OpenAICompatibleChatCompletionProviderRejectsUrlApiKeys()
{
    using var httpClient = new HttpClient(new CapturingHttpMessageHandler(System.Net.HttpStatusCode.OK, "{}"))
    {
        BaseAddress = new Uri("https://example.test/v1")
    };

    try
    {
        _ = new OpenAICompatibleChatCompletionProvider(
            httpClient,
            new OpenAICompatibleOptions
            {
                ApiKey = "https://platform.openai.com/home",
                Model = "test-model"
            });
    }
    catch (InvalidOperationException ex)
    {
        Assert.True(ex.Message.Contains("looks like a URL", StringComparison.Ordinal), "The error should explain that the API key is a URL.");
        return Task.CompletedTask;
    }

    throw new InvalidOperationException("URL-like API keys should be rejected before calling the cloud provider.");
}

static Task OpenAICompatibleChatCompletionProviderRejectsNonOpenAIKeysForOpenAIBaseUrl()
{
    using var httpClient = new HttpClient(new CapturingHttpMessageHandler(System.Net.HttpStatusCode.OK, "{}"))
    {
        BaseAddress = new Uri("https://api.openai.com/v1")
    };

    try
    {
        _ = new OpenAICompatibleChatCompletionProvider(
            httpClient,
            new OpenAICompatibleOptions
            {
                BaseUrl = "https://api.openai.com/v1",
                ApiKey = "not-an-openai-secret",
                Model = "gpt-4.1-mini"
            });
    }
    catch (InvalidOperationException ex)
    {
        Assert.True(ex.Message.Contains("does not look like an OpenAI secret key", StringComparison.Ordinal), "The error should explain the expected OpenAI key shape.");
        return Task.CompletedTask;
    }

    throw new InvalidOperationException("OpenAI official API keys should be validated before calling the provider.");
}

static string CreateTempDirectory()
{
    var path = Path.Combine(Path.GetTempPath(), "gpaa-tests", Guid.NewGuid().ToString("N"));
    Directory.CreateDirectory(path);
    return path;
}

static ProjectIndexingService CreateNoOpIndexingService(IKnowledgeRepository repository, IEmbeddingProvider embeddingProvider)
    => new(new EmptyProjectFileScanner(), embeddingProvider, repository, new EmptyUnrealAssetMetadataRepository(), new SourceChunker(), new SourceContextExtractor(), new UnrealAssetRegistryDocumentRenderer());

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
    public int MaxObservedInputLength { get; private set; }

    public Task<IReadOnlyList<float>> EmbedAsync(string input, CancellationToken cancellationToken)
    {
        CallCount++;
        MaxObservedInputLength = Math.Max(MaxObservedInputLength, input.Length);
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

internal sealed class EmptyUnrealAssetMetadataRepository : IUnrealAssetMetadataRepository
{
    public Task ReplaceAsync(ProjectId projectId, IReadOnlyList<UnrealAssetMetadata> assets, CancellationToken cancellationToken)
        => Task.CompletedTask;

    public Task<IReadOnlyList<UnrealAssetMetadata>> ListAsync(ProjectId projectId, CancellationToken cancellationToken)
        => Task.FromResult<IReadOnlyList<UnrealAssetMetadata>>(Array.Empty<UnrealAssetMetadata>());
}

internal sealed class InMemoryUnrealAssetMetadataRepository : IUnrealAssetMetadataRepository
{
    private readonly IReadOnlyList<UnrealAssetMetadata> _assets;

    public InMemoryUnrealAssetMetadataRepository(IReadOnlyList<UnrealAssetMetadata> assets)
    {
        _assets = assets;
    }

    public Task ReplaceAsync(ProjectId projectId, IReadOnlyList<UnrealAssetMetadata> assets, CancellationToken cancellationToken)
        => Task.CompletedTask;

    public Task<IReadOnlyList<UnrealAssetMetadata>> ListAsync(ProjectId projectId, CancellationToken cancellationToken)
        => Task.FromResult(_assets);
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

internal sealed class CapturingHttpMessageHandler : HttpMessageHandler
{
    private readonly System.Net.HttpStatusCode _statusCode;
    private readonly string _responseBody;

    public CapturingHttpMessageHandler(System.Net.HttpStatusCode statusCode, string responseBody)
    {
        _statusCode = statusCode;
        _responseBody = responseBody;
    }

    public static string? LastAuthorizationScheme { get; private set; }
    public static string? LastAuthorizationParameter { get; private set; }
    public static string? LastRequestPath { get; private set; }

    public static void Reset()
    {
        LastAuthorizationScheme = null;
        LastAuthorizationParameter = null;
        LastRequestPath = null;
    }

    protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
    {
        LastAuthorizationScheme = request.Headers.Authorization?.Scheme;
        LastAuthorizationParameter = request.Headers.Authorization?.Parameter;
        LastRequestPath = request.RequestUri?.AbsolutePath;

        return Task.FromResult(new HttpResponseMessage(_statusCode)
        {
            Content = new StringContent(_responseBody)
        });
    }
}
