using GPAA.Core.Indexing;
using GPAA.Core.Projects;

namespace GPAA.Application.Projects;

public sealed record ProjectIndexSnapshot(
    IReadOnlyList<IndexedFile> Files,
    IReadOnlyList<SourceChunk> Chunks,
    IndexStatistics Statistics,
    ProjectIndexMetadata Metadata);
