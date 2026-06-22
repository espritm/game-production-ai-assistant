using System.Text.Json;
using GPAA.Application.Ports;
using GPAA.Application.Projects;
using GPAA.Core.Indexing;
using GPAA.Core.Projects;

namespace GPAA.Infrastructure.Storage;

public sealed class LocalKnowledgeRepository : IKnowledgeRepository
{
    private static readonly JsonSerializerOptions JsonOptions = new(JsonSerializerDefaults.Web)
    {
        WriteIndented = true
    };

    private readonly string _storageRoot;

    public LocalKnowledgeRepository(string storageRoot)
    {
        _storageRoot = storageRoot;
    }

    public async Task ReplaceProjectIndexAsync(
        ProjectId projectId,
        IReadOnlyList<IndexedFile> files,
        IReadOnlyList<SourceChunk> chunks,
        IndexStatistics statistics,
        CancellationToken cancellationToken)
    {
        var path = GetProjectIndexPath(projectId);
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);

        var document = new ProjectIndexDocument(files, chunks, statistics);
        var tempPath = path + ".tmp";
        await using (var stream = File.Create(tempPath))
        {
            await JsonSerializer.SerializeAsync(stream, document, JsonOptions, cancellationToken);
        }

        File.Move(tempPath, path, overwrite: true);
    }

    public async Task<ProjectIndexSnapshot?> GetProjectIndexAsync(ProjectId projectId, CancellationToken cancellationToken)
    {
        var path = GetProjectIndexPath(projectId);
        if (!File.Exists(path))
        {
            return null;
        }

        await using var stream = File.OpenRead(path);
        var document = await JsonSerializer.DeserializeAsync<ProjectIndexDocument>(stream, JsonOptions, cancellationToken);
        return document is null
            ? null
            : new ProjectIndexSnapshot(document.Files, document.Chunks, document.Statistics);
    }

    private string GetProjectIndexPath(ProjectId projectId)
    {
        var safeName = Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(projectId.Value))).ToLowerInvariant();
        return Path.Combine(_storageRoot, safeName, "index.json");
    }

    private sealed record ProjectIndexDocument(
        IReadOnlyList<IndexedFile> Files,
        IReadOnlyList<SourceChunk> Chunks,
        IndexStatistics Statistics);
}
