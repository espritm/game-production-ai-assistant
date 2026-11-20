using GPAA.Application.Chat;
using GPAA.Core.Projects;

namespace GPAA.Application.Ports;

public interface IChatHistoryRepository
{
    Task AddAsync(ProjectId projectId, ChatHistoryEntry entry, CancellationToken cancellationToken);

    Task<IReadOnlyList<ChatHistoryEntry>> ListAsync(ProjectId projectId, int limit, CancellationToken cancellationToken);
}
