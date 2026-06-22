namespace GPAA.Application.Ports;

public interface IEmbeddingProvider
{
    Task<IReadOnlyList<float>> EmbedAsync(string input, CancellationToken cancellationToken);
}
