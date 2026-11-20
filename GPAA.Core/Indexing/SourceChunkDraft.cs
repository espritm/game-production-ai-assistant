namespace GPAA.Core.Indexing;

public sealed record SourceChunkDraft(
    string RelativePath,
    int StartLine,
    int EndLine,
    string Content);
