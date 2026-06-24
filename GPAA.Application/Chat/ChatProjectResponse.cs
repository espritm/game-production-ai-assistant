using GPAA.Core.Search;

namespace GPAA.Application.Chat;

public sealed record ChatProjectResponse(
    string Answer,
    IReadOnlyList<SearchResult> Sources);
