namespace GPAA.Core.Indexing;

public sealed record IndexStatistics(
    int FilesScanned,
    int FilesIndexed,
    int ChunksIndexed,
    DateTimeOffset IndexedAt,
    int FilesReused = 0,
    int FilesChanged = 0,
    int FilesRemoved = 0,
    int ChunksReused = 0,
    int ChunksEmbedded = 0,
    bool FullRebuild = true,
    string EmbeddingModel = "");
