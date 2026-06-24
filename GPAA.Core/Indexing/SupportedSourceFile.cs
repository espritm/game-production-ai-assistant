namespace GPAA.Core.Indexing;

public static class SupportedSourceFile
{
    private static readonly HashSet<string> Extensions = new(StringComparer.OrdinalIgnoreCase)
    {
        ".h",
        ".hpp",
        ".cpp",
        ".cs",
        ".ini",
        ".uplugin",
        ".uproject"
    };

    public static bool IsSupported(string path) => Extensions.Contains(Path.GetExtension(path));
}
