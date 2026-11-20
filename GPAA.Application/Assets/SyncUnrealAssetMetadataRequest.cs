namespace GPAA.Application.Assets;

public sealed record SyncUnrealAssetMetadataRequest(
    string ProjectRoot,
    IReadOnlyList<UnrealAssetMetadata> Assets);
