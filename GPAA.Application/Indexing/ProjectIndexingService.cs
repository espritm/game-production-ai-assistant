using System.Collections.Concurrent;
using GPAA.Application.Ports;
using GPAA.Application.Projects;
using GPAA.Core.Indexing;
using GPAA.Core.Projects;

namespace GPAA.Application.Indexing;

public sealed class ProjectIndexingService
{
    private readonly IProjectFileScanner _scanner;
    private readonly IEmbeddingProvider _embeddingProvider;
    private readonly IKnowledgeRepository _repository;
    private readonly SourceChunker _chunker;
    private readonly SourceContextExtractor _contextExtractor;
    private readonly ConcurrentDictionary<string, SemaphoreSlim> _projectLocks = new(StringComparer.OrdinalIgnoreCase);

    public ProjectIndexingService(
        IProjectFileScanner scanner,
        IEmbeddingProvider embeddingProvider,
        IKnowledgeRepository repository,
        SourceChunker chunker,
        SourceContextExtractor contextExtractor)
    {
        _scanner = scanner;
        _embeddingProvider = embeddingProvider;
        _repository = repository;
        _chunker = chunker;
        _contextExtractor = contextExtractor;
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
        var scannedFiles = await _scanner.ScanAsync(projectId.Value, cancellationToken);
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
}
