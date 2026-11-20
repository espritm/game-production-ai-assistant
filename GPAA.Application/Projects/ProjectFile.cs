namespace GPAA.Application.Projects;

public sealed record ProjectFile(
    string RelativePath,
    string AbsolutePath,
    string Content,
    string ContentHash,
    DateTimeOffset LastWriteTime);
