using GPAA.Application.Assets;
using GPAA.Core.Projects;

namespace GPAA.Application.Ports;

public interface IUnrealAssetMetadataRepository
{
    Task ReplaceAsync(ProjectId projectId, IReadOnlyList<UnrealAssetMetadata> assets, CancellationToken cancellationToken);

    Task<IReadOnlyList<UnrealAssetMetadata>> ListAsync(ProjectId projectId, CancellationToken cancellationToken);
}
