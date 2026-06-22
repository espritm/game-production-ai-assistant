namespace GPAA.Core.Projects;

public sealed record SourceChunk(
    string Id,
    string RelativePath,
    int StartLine,
    int EndLine,
    string Content,
    IReadOnlyList<float> Embedding);
