using System.Security.Cryptography;
using System.Text;
using GPAA.Application.Ports;
using GPAA.Application.Projects;
using GPAA.Core.Indexing;

namespace GPAA.Infrastructure.Files;

public sealed class LocalProjectFileScanner : IProjectFileScanner
{
    private readonly UnrealAssetMetadataExtractor _assetMetadataExtractor;

    private static readonly HashSet<string> IgnoredDirectories = new(StringComparer.OrdinalIgnoreCase)
    {
        ".git",
        ".vs",
        "Binaries",
        "DerivedDataCache",
        "Intermediate",
        "Saved"
    };

    public LocalProjectFileScanner()
        : this(new UnrealAssetMetadataExtractor())
    {
    }

    public LocalProjectFileScanner(UnrealAssetMetadataExtractor assetMetadataExtractor)
    {
        _assetMetadataExtractor = assetMetadataExtractor;
    }

    public async Task<IReadOnlyList<ProjectFile>> ScanAsync(string projectRoot, CancellationToken cancellationToken)
    {
        if (!Directory.Exists(projectRoot))
        {
            throw new DirectoryNotFoundException($"Project root does not exist: {projectRoot}");
        }

        var root = Path.GetFullPath(projectRoot);
        var files = new List<ProjectFile>();

        foreach (var path in Directory.EnumerateFiles(root, "*", SearchOption.AllDirectories))
        {
            cancellationToken.ThrowIfCancellationRequested();

            if (ShouldIgnore(root, path) || !SupportedSourceFile.IsSupported(path))
            {
                continue;
            }

            var relativePath = Path.GetRelativePath(root, path).Replace('\\', '/');
            var indexableFile = await ReadIndexableContentAsync(path, relativePath, cancellationToken);
            var lastWriteTime = File.GetLastWriteTimeUtc(path);

            files.Add(new ProjectFile(
                relativePath,
                Path.GetFullPath(path),
                indexableFile.Content,
                indexableFile.ContentHash,
                new DateTimeOffset(lastWriteTime, TimeSpan.Zero)));
        }

        return files.OrderBy(file => file.RelativePath, StringComparer.OrdinalIgnoreCase).ToArray();
    }

    private async Task<IndexableFileContent> ReadIndexableContentAsync(string path, string relativePath, CancellationToken cancellationToken)
    {
        if (SupportedSourceFile.IsUnrealAssetMetadataSupported(path))
        {
            var bytes = await File.ReadAllBytesAsync(path, cancellationToken);
            return new IndexableFileContent(
                _assetMetadataExtractor.Extract(relativePath),
                ComputeSha256(bytes));
        }

        var content = await File.ReadAllTextAsync(path, Encoding.UTF8, cancellationToken);
        return new IndexableFileContent(content, ComputeSha256(content));
    }

    private static bool ShouldIgnore(string root, string path)
    {
        var relative = Path.GetRelativePath(root, path);
        var segments = relative.Split(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        return segments.Any(segment => IgnoredDirectories.Contains(segment));
    }

    private static string ComputeSha256(string content)
    {
        var bytes = SHA256.HashData(Encoding.UTF8.GetBytes(content));
        return Convert.ToHexString(bytes).ToLowerInvariant();
    }

    private static string ComputeSha256(byte[] content)
    {
        var bytes = SHA256.HashData(content);
        return Convert.ToHexString(bytes).ToLowerInvariant();
    }

    private sealed record IndexableFileContent(string Content, string ContentHash);
}
