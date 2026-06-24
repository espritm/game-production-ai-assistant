namespace GPAA.Application.Ports;

public interface IEmbeddingProvider
{
    string ModelId { get; }

    Task<IReadOnlyList<float>> EmbedAsync(string input, CancellationToken cancellationToken);
}
