using System.Text.Json;
using GPAA.Application.Assets;
using GPAA.Application.Ports;
using GPAA.Core.Projects;

namespace GPAA.Infrastructure.Storage;

public sealed class LocalUnrealAssetMetadataRepository : IUnrealAssetMetadataRepository
{
    private static readonly JsonSerializerOptions JsonOptions = new(JsonSerializerDefaults.Web)
    {
        WriteIndented = true
    };

    private readonly string _storageRoot;

    public LocalUnrealAssetMetadataRepository(string storageRoot)
    {
        _storageRoot = storageRoot;
    }

    public async Task ReplaceAsync(ProjectId projectId, IReadOnlyList<UnrealAssetMetadata> assets, CancellationToken cancellationToken)
    {
        var path = GetAssetMetadataPath(projectId);
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);

        var tempPath = path + ".tmp";
        await using (var stream = File.Create(tempPath))
        {
            await JsonSerializer.SerializeAsync(stream, new UnrealAssetMetadataDocument(assets, DateTimeOffset.UtcNow), JsonOptions, cancellationToken);
        }

        File.Move(tempPath, path, overwrite: true);
    }

    public async Task<IReadOnlyList<UnrealAssetMetadata>> ListAsync(ProjectId projectId, CancellationToken cancellationToken)
    {
        var path = GetAssetMetadataPath(projectId);
        if (!File.Exists(path))
        {
            return Array.Empty<UnrealAssetMetadata>();
        }

        await using var stream = File.OpenRead(path);
        var document = await JsonSerializer.DeserializeAsync<UnrealAssetMetadataDocument>(stream, JsonOptions, cancellationToken);
        return document?.Assets ?? Array.Empty<UnrealAssetMetadata>();
    }

    private string GetAssetMetadataPath(ProjectId projectId)
    {
        var safeName = Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(projectId.Value))).ToLowerInvariant();
        return Path.Combine(_storageRoot, safeName, "asset-registry.json");
    }

    private sealed record UnrealAssetMetadataDocument(
        IReadOnlyList<UnrealAssetMetadata> Assets,
        DateTimeOffset SyncedAt);
}
