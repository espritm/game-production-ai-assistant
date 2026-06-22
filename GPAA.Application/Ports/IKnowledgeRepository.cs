using GPAA.Core.Indexing;
using GPAA.Core.Projects;
using GPAA.Application.Projects;

namespace GPAA.Application.Ports;

public interface IKnowledgeRepository
{
    Task ReplaceProjectIndexAsync(
        ProjectId projectId,
        IReadOnlyList<IndexedFile> files,
        IReadOnlyList<SourceChunk> chunks,
        IndexStatistics statistics,
        CancellationToken cancellationToken);

    Task<ProjectIndexSnapshot?> GetProjectIndexAsync(ProjectId projectId, CancellationToken cancellationToken);
}
