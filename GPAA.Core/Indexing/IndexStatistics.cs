namespace GPAA.Core.Indexing;

public sealed record IndexStatistics(
    int FilesScanned,
    int FilesIndexed,
    int ChunksIndexed,
    DateTimeOffset IndexedAt);
