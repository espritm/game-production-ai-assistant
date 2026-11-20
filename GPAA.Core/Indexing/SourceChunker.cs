namespace GPAA.Core.Indexing;

public sealed class SourceChunker
{
    public const int DefaultMaxLinesPerChunk = 40;
    public const int DefaultOverlapLines = 4;
    public const int DefaultMaxCharactersPerChunk = 3000;

    private readonly int _maxLinesPerChunk;
    private readonly int _overlapLines;
    private readonly int _maxCharactersPerChunk;

    public SourceChunker(
        int maxLinesPerChunk = DefaultMaxLinesPerChunk,
        int overlapLines = DefaultOverlapLines,
        int maxCharactersPerChunk = DefaultMaxCharactersPerChunk)
    {
        if (maxLinesPerChunk <= 0)
        {
            throw new ArgumentOutOfRangeException(nameof(maxLinesPerChunk));
        }

        if (overlapLines < 0 || overlapLines >= maxLinesPerChunk)
        {
            throw new ArgumentOutOfRangeException(nameof(overlapLines));
        }

        if (maxCharactersPerChunk <= 0)
        {
            throw new ArgumentOutOfRangeException(nameof(maxCharactersPerChunk));
        }

        _maxLinesPerChunk = maxLinesPerChunk;
        _overlapLines = overlapLines;
        _maxCharactersPerChunk = maxCharactersPerChunk;
    }

    public IReadOnlyList<SourceChunkDraft> Chunk(string relativePath, string content)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(relativePath);

        if (string.IsNullOrWhiteSpace(content))
        {
            return Array.Empty<SourceChunkDraft>();
        }

        var normalized = content.Replace("\r\n", "\n", StringComparison.Ordinal).Replace('\r', '\n');
        var lines = normalized.Split('\n');
        var chunks = new List<SourceChunkDraft>();
        var startIndex = 0;

        while (startIndex < lines.Length)
        {
            var endExclusive = FindChunkEnd(lines, startIndex);
            var chunkContent = string.Join(Environment.NewLine, lines[startIndex..endExclusive]).Trim();

            if (!string.IsNullOrWhiteSpace(chunkContent))
            {
                chunks.Add(new SourceChunkDraft(
                    relativePath,
                    startIndex + 1,
                    endExclusive,
                    chunkContent));
            }

            if (endExclusive == lines.Length)
            {
                break;
            }

            startIndex = Math.Max(endExclusive - _overlapLines, startIndex + 1);
        }

        return chunks;
    }

    private int FindChunkEnd(string[] lines, int startIndex)
    {
        var endExclusive = startIndex;
        var characterCount = 0;

        while (endExclusive < lines.Length && endExclusive - startIndex < _maxLinesPerChunk)
        {
            var lineLength = lines[endExclusive].Length + Environment.NewLine.Length;
            if (endExclusive > startIndex && characterCount + lineLength > _maxCharactersPerChunk)
            {
                break;
            }

            characterCount += lineLength;
            endExclusive++;
        }

        return Math.Max(endExclusive, startIndex + 1);
    }
}
