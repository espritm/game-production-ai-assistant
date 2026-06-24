using GPAA.Core.Search;

namespace GPAA.Application.Chat;

public sealed record ChatHistoryEntry(
    string Id,
    string ProjectRoot,
    string Question,
    string Answer,
    IReadOnlyList<SearchResult> Sources,
    DateTimeOffset CreatedAt);
