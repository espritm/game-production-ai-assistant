using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using GPAA.Application.Ports;

namespace GPAA.Infrastructure.Embeddings;

public sealed partial class DeterministicEmbeddingProvider : IEmbeddingProvider
{
    public const int DefaultDimensions = 384;
    private readonly int _dimensions;

    public DeterministicEmbeddingProvider(int dimensions = DefaultDimensions)
    {
        if (dimensions <= 0)
        {
            throw new ArgumentOutOfRangeException(nameof(dimensions));
        }

        _dimensions = dimensions;
    }

    public string ModelId => $"deterministic-hash-v1:{_dimensions}";

    public Task<IReadOnlyList<float>> EmbedAsync(string input, CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();

        var vector = new float[_dimensions];
        foreach (var token in Tokenize(input))
        {
            var hash = SHA256.HashData(Encoding.UTF8.GetBytes(token));
            var index = BitConverter.ToUInt32(hash, 0) % _dimensions;
            var sign = (hash[4] & 1) == 0 ? 1f : -1f;
            vector[index] += sign;
        }

        Normalize(vector);
        return Task.FromResult<IReadOnlyList<float>>(vector);
    }

    private static IEnumerable<string> Tokenize(string input)
    {
        foreach (Match match in TokenRegex().Matches(input.ToLowerInvariant()))
        {
            yield return match.Value;
        }
    }

    private static void Normalize(float[] vector)
    {
        var magnitude = Math.Sqrt(vector.Sum(value => value * value));
        if (magnitude == 0)
        {
            return;
        }

        for (var i = 0; i < vector.Length; i++)
        {
            vector[i] = (float)(vector[i] / magnitude);
        }
    }

    [GeneratedRegex("[a-z0-9_]+", RegexOptions.Compiled)]
    private static partial Regex TokenRegex();
}
