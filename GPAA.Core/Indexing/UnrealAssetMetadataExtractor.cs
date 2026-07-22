namespace GPAA.Core.Indexing;

public sealed class UnrealAssetMetadataExtractor
{
    public string Extract(string relativePath)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(relativePath);

        var normalizedPath = relativePath.Replace('\\', '/');
        var fileName = Path.GetFileNameWithoutExtension(normalizedPath);
        var extension = Path.GetExtension(normalizedPath);
        var directory = Path.GetDirectoryName(normalizedPath)?.Replace('\\', '/') ?? string.Empty;
        var assetType = InferAssetType(fileName, extension, directory);

        return string.Join(Environment.NewLine, new[]
        {
            "Unreal asset metadata",
            $"AssetName: {fileName}",
            $"AssetType: {assetType}",
            $"RelativePath: {normalizedPath}",
            $"Folder: {directory}",
            $"Extension: {extension}",
            $"SearchTerms: {BuildSearchTerms(fileName, directory, assetType)}"
        });
    }

    private static string InferAssetType(string fileName, string extension, string directory)
    {
        if (extension.Equals(".umap", StringComparison.OrdinalIgnoreCase))
        {
            return "Map";
        }

        var name = fileName.ToLowerInvariant();
        var folder = directory.ToLowerInvariant();

        if (name.StartsWith("bp_", StringComparison.Ordinal) || folder.Contains("blueprint", StringComparison.Ordinal))
        {
            return "Blueprint";
        }

        if (name.StartsWith("w_", StringComparison.Ordinal) || name.StartsWith("ui_", StringComparison.Ordinal) || folder.Contains("widget", StringComparison.Ordinal))
        {
            return "WidgetBlueprint";
        }

        if (name.StartsWith("m_", StringComparison.Ordinal))
        {
            return "Material";
        }

        if (name.StartsWith("mi_", StringComparison.Ordinal) || name.StartsWith("mic_", StringComparison.Ordinal))
        {
            return "MaterialInstance";
        }

        if (name.StartsWith("t_", StringComparison.Ordinal) || folder.Contains("texture", StringComparison.Ordinal))
        {
            return "Texture";
        }

        if (name.StartsWith("sk_", StringComparison.Ordinal))
        {
            return "SkeletalMesh";
        }

        if (name.StartsWith("sm_", StringComparison.Ordinal))
        {
            return "StaticMesh";
        }

        if (name.StartsWith("da_", StringComparison.Ordinal) || folder.Contains("dataasset", StringComparison.Ordinal) || folder.Contains("data asset", StringComparison.Ordinal))
        {
            return "DataAsset";
        }

        if (name.StartsWith("dt_", StringComparison.Ordinal))
        {
            return "DataTable";
        }

        if (name.StartsWith("abp_", StringComparison.Ordinal))
        {
            return "AnimationBlueprint";
        }

        if (folder.Contains("animation", StringComparison.Ordinal))
        {
            return "AnimationAsset";
        }

        return "UnrealAsset";
    }

    private static string BuildSearchTerms(string fileName, string directory, string assetType)
    {
        var terms = new List<string> { assetType };
        terms.AddRange(SplitIdentifier(fileName));
        terms.AddRange(directory.Split('/', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries));
        return string.Join(' ', terms.Distinct(StringComparer.OrdinalIgnoreCase));
    }

    private static IEnumerable<string> SplitIdentifier(string value)
    {
        var current = new List<char>();
        foreach (var character in value.Replace('_', ' '))
        {
            if (char.IsWhiteSpace(character))
            {
                if (current.Count > 0)
                {
                    yield return new string(current.ToArray());
                    current.Clear();
                }

                continue;
            }

            if (current.Count > 0 && char.IsUpper(character) && !char.IsUpper(current[^1]))
            {
                yield return new string(current.ToArray());
                current.Clear();
            }

            current.Add(character);
        }

        if (current.Count > 0)
        {
            yield return new string(current.ToArray());
        }
    }
}
