namespace GPAA.Application.Projects;

public sealed record ProjectIndexMetadata(
    int SchemaVersion,
    string EmbeddingModel,
    DateTimeOffset CreatedAt)
{
    public static ProjectIndexMetadata Legacy(DateTimeOffset createdAt)
        => new(0, "legacy-unknown", createdAt);
}
