using System.Collections.Concurrent;
using System.Security.Cryptography;
using System.Text;
using GPAA.Application.Ports;
using GPAA.Application.Projects;
using GPAA.Core.Indexing;
using GPAA.Core.Projects;

namespace GPAA.Application.Indexing;

public sealed class ProjectIndexingService
{
    private const int MaxEmbeddingInputCharacters = 1800;

    private readonly IProjectFileScanner _scanner;
    private readonly IEmbeddingProvider _embeddingProvider;
    private readonly IKnowledgeRepository _repository;
    private readonly IUnrealAssetMetadataRepository _assetMetadataRepository;
    private readonly SourceChunker _chunker;
    private readonly SourceContextExtractor _contextExtractor;
    private readonly UnrealAssetRegistryDocumentRenderer _assetRegistryDocumentRenderer;
    private readonly ConcurrentDictionary<string, SemaphoreSlim> _projectLocks = new(StringComparer.OrdinalIgnoreCase);

    public ProjectIndexingService(
        IProjectFileScanner scanner,
        IEmbeddingProvider embeddingProvider,
        IKnowledgeRepository repository,
        IUnrealAssetMetadataRepository assetMetadataRepository,
        SourceChunker chunker,
        SourceContextExtractor contextExtractor,
        UnrealAssetRegistryDocumentRenderer assetRegistryDocumentRenderer)
    {
        _scanner = scanner;
        _embeddingProvider = embeddingProvider;
        _repository = repository;
        _assetMetadataRepository = assetMetadataRepository;
        _chunker = chunker;
        _contextExtractor = contextExtractor;
        _assetRegistryDocumentRenderer = assetRegistryDocumentRenderer;
    }

    public async Task<IndexStatistics> IndexAsync(IndexProjectRequest request, CancellationToken cancellationToken)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(request.ProjectRoot);

        var projectId = ProjectId.FromPath(request.ProjectRoot);
        var projectLock = _projectLocks.GetOrAdd(projectId.Value, _ => new SemaphoreSlim(1, 1));
        await projectLock.WaitAsync(cancellationToken);

        try
        {
            return await RebuildIncrementallyAsync(projectId, cancellationToken);
        }
        finally
        {
            projectLock.Release();
        }
    }

    private async Task<IndexStatistics> RebuildIncrementallyAsync(ProjectId projectId, CancellationToken cancellationToken)
    {
        var scannedFiles = await GetIndexableFilesAsync(projectId, cancellationToken);
        var existingIndex = await _repository.GetProjectIndexAsync(projectId, cancellationToken);
        var canReuseExistingIndex = existingIndex?.Metadata.EmbeddingModel == _embeddingProvider.ModelId;
        var existingFilesByPath = canReuseExistingIndex
            ? existingIndex!.Files.ToDictionary(file => file.RelativePath, StringComparer.OrdinalIgnoreCase)
            : new Dictionary<string, IndexedFile>(StringComparer.OrdinalIgnoreCase);
        var existingChunksByPath = canReuseExistingIndex
            ? existingIndex!.Chunks
                .GroupBy(chunk => chunk.RelativePath, StringComparer.OrdinalIgnoreCase)
                .ToDictionary(group => group.Key, group => group.ToArray(), StringComparer.OrdinalIgnoreCase)
            : new Dictionary<string, SourceChunk[]>(StringComparer.OrdinalIgnoreCase);

        var indexedFiles = new List<IndexedFile>();
        var chunks = new List<SourceChunk>();
        var filesReused = 0;
        var filesChanged = 0;
        var chunksReused = 0;
        var chunksEmbedded = 0;
        var scannedPaths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        foreach (var file in scannedFiles)
        {
            cancellationToken.ThrowIfCancellationRequested();
            scannedPaths.Add(file.RelativePath);

            if (existingFilesByPath.TryGetValue(file.RelativePath, out var existingFile)
                && existingFile.ContentHash == file.ContentHash
                && existingChunksByPath.TryGetValue(file.RelativePath, out var existingChunks))
            {
                indexedFiles.Add(existingFile with { AbsolutePath = file.AbsolutePath });
                chunks.AddRange(existingChunks);
                filesReused++;
                chunksReused += existingChunks.Length;
                continue;
            }

            indexedFiles.Add(new IndexedFile(
                file.RelativePath,
                file.AbsolutePath,
                file.ContentHash,
                DateTimeOffset.UtcNow));

            filesChanged++;
            var sourceContext = _contextExtractor.Extract(file.RelativePath, file.Content);
            foreach (var draft in _chunker.Chunk(file.RelativePath, file.Content))
            {
                var content = sourceContext.Length == 0
                    ? draft.Content
                    : $"Project source context:{Environment.NewLine}{sourceContext}{Environment.NewLine}{Environment.NewLine}{draft.Content}";
                content = LimitEmbeddingInput(content);
                var embedding = await _embeddingProvider.EmbedAsync(content, cancellationToken);
                chunks.Add(new SourceChunk(
                    CreateChunkId(file.ContentHash, draft.StartLine, draft.EndLine),
                    draft.RelativePath,
                    draft.StartLine,
                    draft.EndLine,
                    content,
                    embedding));
                chunksEmbedded++;
            }
        }

        var filesRemoved = canReuseExistingIndex
            ? existingIndex!.Files.Count(file => !scannedPaths.Contains(file.RelativePath))
            : 0;
        var indexedAt = DateTimeOffset.UtcNow;
        var statistics = new IndexStatistics(
            scannedFiles.Count,
            indexedFiles.Count,
            chunks.Count,
            indexedAt,
            filesReused,
            filesChanged,
            filesRemoved,
            chunksReused,
            chunksEmbedded,
            !canReuseExistingIndex,
            _embeddingProvider.ModelId);

        var metadata = new ProjectIndexMetadata(
            SchemaVersion: 1,
            EmbeddingModel: _embeddingProvider.ModelId,
            CreatedAt: indexedAt);

        await _repository.ReplaceProjectIndexAsync(projectId, indexedFiles, chunks, statistics, metadata, cancellationToken);
        return statistics;
    }

    private static string CreateChunkId(string contentHash, int startLine, int endLine)
        => $"{contentHash}:{startLine}:{endLine}";

    private static string LimitEmbeddingInput(string content)
    {
        if (content.Length <= MaxEmbeddingInputCharacters)
        {
            return content;
        }

        return string.Concat(
            content.AsSpan(0, MaxEmbeddingInputCharacters),
            Environment.NewLine,
            "[Content truncated before embedding because it exceeds the local embedding model context window.]");
    }

    private async Task<IReadOnlyList<ProjectFile>> GetIndexableFilesAsync(ProjectId projectId, CancellationToken cancellationToken)
    {
        var files = (await _scanner.ScanAsync(projectId.Value, cancellationToken)).ToList();
        var assets = await _assetMetadataRepository.ListAsync(projectId, cancellationToken);

        foreach (var asset in assets)
        {
            var content = _assetRegistryDocumentRenderer.Render(
                asset.ObjectPath,
                asset.PackageName,
                asset.PackagePath,
                asset.AssetName,
                asset.AssetClass,
                asset.Tags,
                asset.Dependencies,
                asset.Referencers);

            var relativePath = $".gpaa/unreal-assets/{SanitizeVirtualPath(asset.ObjectPath)}.asset-metadata";
            files.Add(new ProjectFile(
                relativePath,
                relativePath,
                content,
                ComputeSha256(content),
                DateTimeOffset.UtcNow));
        }

        return files.OrderBy(file => file.RelativePath, StringComparer.OrdinalIgnoreCase).ToArray();
    }

    private static string SanitizeVirtualPath(string value)
        => string.Join('_', value.Split(Path.GetInvalidFileNameChars(), StringSplitOptions.RemoveEmptyEntries))
            .Replace('/', '_')
            .Replace('\\', '_');

    private static string ComputeSha256(string content)
    {
        var bytes = SHA256.HashData(Encoding.UTF8.GetBytes(content));
        return Convert.ToHexString(bytes).ToLowerInvariant();
    }
}
