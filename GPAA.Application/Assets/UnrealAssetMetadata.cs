namespace GPAA.Application.Assets;

public sealed record UnrealAssetMetadata(
    string ObjectPath,
    string PackageName,
    string PackagePath,
    string AssetName,
    string AssetClass,
    IReadOnlyDictionary<string, string> Tags,
    IReadOnlyList<string>? Dependencies = null,
    IReadOnlyList<string>? Referencers = null);
