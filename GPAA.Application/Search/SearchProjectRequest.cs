namespace GPAA.Application.Search;

public sealed record SearchProjectRequest(string ProjectRoot, string Query, int TopK = 8, string SourceFilter = "All");
