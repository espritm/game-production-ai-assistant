using GPAA.Application.Indexing;
using GPAA.Application.Ports;
using GPAA.Core.Projects;
using GPAA.Core.Search;

namespace GPAA.Application.Search;

public sealed class SemanticSearchService
{
    private readonly IEmbeddingProvider _embeddingProvider;
    private readonly IKnowledgeRepository _repository;
    private readonly ProjectIndexingService _indexingService;

    public SemanticSearchService(
        IEmbeddingProvider embeddingProvider,
        IKnowledgeRepository repository,
        ProjectIndexingService indexingService)
    {
        _embeddingProvider = embeddingProvider;
        _repository = repository;
        _indexingService = indexingService;
    }

    public async Task<IReadOnlyList<SearchResult>> SearchAsync(SearchProjectRequest request, CancellationToken cancellationToken)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(request.ProjectRoot);
        ArgumentException.ThrowIfNullOrWhiteSpace(request.Query);

        var topK = Math.Clamp(request.TopK, 1, 50);
        var projectId = ProjectId.FromPath(request.ProjectRoot);
        await _indexingService.IndexAsync(new IndexProjectRequest(projectId.Value), cancellationToken);
        var snapshot = await _repository.GetProjectIndexAsync(projectId, cancellationToken);

        if (snapshot is null || snapshot.Chunks.Count == 0)
        {
            return Array.Empty<SearchResult>();
        }

        var queryEmbedding = await _embeddingProvider.EmbedAsync(request.Query, cancellationToken);

        var compatibleChunks = snapshot.Chunks
            .Where(chunk => MatchesSourceFilter(chunk.RelativePath, request.SourceFilter))
            .Where(chunk => chunk.Embedding.Count == queryEmbedding.Count)
            .ToArray();

        if (compatibleChunks.Length == 0)
        {
            return Array.Empty<SearchResult>();
        }

        return compatibleChunks
            .Select(chunk => new SearchResult(
                chunk.Id,
                chunk.RelativePath,
                chunk.StartLine,
                chunk.EndLine,
                chunk.Content,
                VectorMath.CosineSimilarity(queryEmbedding, chunk.Embedding),
                GetSourceType(chunk.RelativePath)))
            .OrderByDescending(result => result.Score)
            .Take(topK)
            .ToArray();
    }

    private static bool MatchesSourceFilter(string relativePath, string sourceFilter)
    {
        if (sourceFilter.Equals("Assets", StringComparison.OrdinalIgnoreCase))
        {
            return IsAssetMetadataPath(relativePath);
        }

        if (sourceFilter.Equals("Code", StringComparison.OrdinalIgnoreCase))
        {
            return !IsAssetMetadataPath(relativePath);
        }

        return true;
    }

    private static string GetSourceType(string relativePath)
        => IsAssetMetadataPath(relativePath) ? "Asset" : "Code";

    private static bool IsAssetMetadataPath(string relativePath)
        => relativePath.StartsWith(".gpaa/unreal-assets/", StringComparison.OrdinalIgnoreCase);
}
