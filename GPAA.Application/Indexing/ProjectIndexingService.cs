using GPAA.Application.Ports;
using GPAA.Core.Indexing;
using GPAA.Core.Projects;

namespace GPAA.Application.Indexing;

public sealed class ProjectIndexingService
{
    private readonly IProjectFileScanner _scanner;
    private readonly IEmbeddingProvider _embeddingProvider;
    private readonly IKnowledgeRepository _repository;
    private readonly SourceChunker _chunker;

    public ProjectIndexingService(
        IProjectFileScanner scanner,
        IEmbeddingProvider embeddingProvider,
        IKnowledgeRepository repository,
        SourceChunker chunker)
    {
        _scanner = scanner;
        _embeddingProvider = embeddingProvider;
        _repository = repository;
        _chunker = chunker;
    }

    public async Task<IndexStatistics> IndexAsync(IndexProjectRequest request, CancellationToken cancellationToken)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(request.ProjectRoot);

        var projectId = ProjectId.FromPath(request.ProjectRoot);
        var scannedFiles = await _scanner.ScanAsync(projectId.Value, cancellationToken);
        var indexedFiles = new List<IndexedFile>();
        var chunks = new List<SourceChunk>();

        foreach (var file in scannedFiles)
        {
            cancellationToken.ThrowIfCancellationRequested();

            indexedFiles.Add(new IndexedFile(
                file.RelativePath,
                file.AbsolutePath,
                file.ContentHash,
                DateTimeOffset.UtcNow));

            foreach (var draft in _chunker.Chunk(file.RelativePath, file.Content))
            {
                var embedding = await _embeddingProvider.EmbedAsync(draft.Content, cancellationToken);
                chunks.Add(new SourceChunk(
                    CreateChunkId(file.ContentHash, draft.StartLine, draft.EndLine),
                    draft.RelativePath,
                    draft.StartLine,
                    draft.EndLine,
                    draft.Content,
                    embedding));
            }
        }

        var statistics = new IndexStatistics(
            scannedFiles.Count,
            indexedFiles.Count,
            chunks.Count,
            DateTimeOffset.UtcNow);

        await _repository.ReplaceProjectIndexAsync(projectId, indexedFiles, chunks, statistics, cancellationToken);
        return statistics;
    }

    private static string CreateChunkId(string contentHash, int startLine, int endLine)
        => $"{contentHash}:{startLine}:{endLine}";
}
