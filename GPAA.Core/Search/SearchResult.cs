namespace GPAA.Core.Search;

public sealed record SearchResult(
    string ChunkId,
    string RelativePath,
    int StartLine,
    int EndLine,
    string Content,
    double Score);
