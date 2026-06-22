using GPAA.Application.Projects;

namespace GPAA.Application.Ports;

public interface IProjectFileScanner
{
    Task<IReadOnlyList<ProjectFile>> ScanAsync(string projectRoot, CancellationToken cancellationToken);
}
