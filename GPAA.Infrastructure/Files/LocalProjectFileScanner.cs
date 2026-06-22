using System.Security.Cryptography;
using System.Text;
using GPAA.Application.Ports;
using GPAA.Application.Projects;
using GPAA.Core.Indexing;

namespace GPAA.Infrastructure.Files;

public sealed class LocalProjectFileScanner : IProjectFileScanner
{
    private static readonly HashSet<string> IgnoredDirectories = new(StringComparer.OrdinalIgnoreCase)
    {
        ".git",
        ".vs",
        "Binaries",
        "DerivedDataCache",
        "Intermediate",
        "Saved"
    };

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

            var content = await File.ReadAllTextAsync(path, Encoding.UTF8, cancellationToken);
            var relativePath = Path.GetRelativePath(root, path).Replace('\\', '/');
            var lastWriteTime = File.GetLastWriteTimeUtc(path);

            files.Add(new ProjectFile(
                relativePath,
                Path.GetFullPath(path),
                content,
                ComputeSha256(content),
                new DateTimeOffset(lastWriteTime, TimeSpan.Zero)));
        }

        return files.OrderBy(file => file.RelativePath, StringComparer.OrdinalIgnoreCase).ToArray();
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
}
