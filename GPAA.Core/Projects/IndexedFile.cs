namespace GPAA.Core.Projects;

public sealed record IndexedFile(
    string RelativePath,
    string AbsolutePath,
    string ContentHash,
    DateTimeOffset IndexedAt);
