using System.Text.Json;
using GPAA.Application.Chat;
using GPAA.Application.Ports;
using GPAA.Core.Projects;

namespace GPAA.Infrastructure.Storage;

public sealed class LocalChatHistoryRepository : IChatHistoryRepository
{
    private static readonly JsonSerializerOptions JsonOptions = new(JsonSerializerDefaults.Web)
    {
        WriteIndented = true
    };

    private readonly string _storageRoot;

    public LocalChatHistoryRepository(string storageRoot)
    {
        _storageRoot = storageRoot;
    }

    public async Task AddAsync(ProjectId projectId, ChatHistoryEntry entry, CancellationToken cancellationToken)
    {
        var history = (await ListAllAsync(projectId, cancellationToken)).ToList();
        history.Insert(0, entry);

        var path = GetHistoryPath(projectId);
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        var tempPath = path + ".tmp";
        await using (var stream = File.Create(tempPath))
        {
            await JsonSerializer.SerializeAsync(stream, new ChatHistoryDocument(history.Take(100).ToArray()), JsonOptions, cancellationToken);
        }

        File.Move(tempPath, path, overwrite: true);
    }

    public async Task<IReadOnlyList<ChatHistoryEntry>> ListAsync(ProjectId projectId, int limit, CancellationToken cancellationToken)
    {
        var history = await ListAllAsync(projectId, cancellationToken);
        return history.Take(Math.Clamp(limit, 1, 100)).ToArray();
    }

    private async Task<IReadOnlyList<ChatHistoryEntry>> ListAllAsync(ProjectId projectId, CancellationToken cancellationToken)
    {
        var path = GetHistoryPath(projectId);
        if (!File.Exists(path))
        {
            return Array.Empty<ChatHistoryEntry>();
        }

        await using var stream = File.OpenRead(path);
        var document = await JsonSerializer.DeserializeAsync<ChatHistoryDocument>(stream, JsonOptions, cancellationToken);
        return document?.Entries ?? Array.Empty<ChatHistoryEntry>();
    }

    private string GetHistoryPath(ProjectId projectId)
    {
        var safeName = Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(projectId.Value))).ToLowerInvariant();
        return Path.Combine(_storageRoot, safeName, "chat-history.json");
    }

    private sealed record ChatHistoryDocument(IReadOnlyList<ChatHistoryEntry> Entries);
}
