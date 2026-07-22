namespace GPAA.Core.Indexing;

public static class SupportedSourceFile
{
    private static readonly HashSet<string> TextExtensions = new(StringComparer.OrdinalIgnoreCase)
    {
        ".h",
        ".hpp",
        ".cpp",
        ".cs",
        ".ini",
        ".uplugin",
        ".uproject"
    };

    private static readonly HashSet<string> UnrealAssetMetadataExtensions = new(StringComparer.OrdinalIgnoreCase)
    {
        ".uasset",
        ".umap"
    };

    public static bool IsSupported(string path)
        => IsTextSupported(path) || IsUnrealAssetMetadataSupported(path);

    public static bool IsTextSupported(string path)
        => TextExtensions.Contains(Path.GetExtension(path));

    public static bool IsUnrealAssetMetadataSupported(string path)
        => UnrealAssetMetadataExtensions.Contains(Path.GetExtension(path));
}
