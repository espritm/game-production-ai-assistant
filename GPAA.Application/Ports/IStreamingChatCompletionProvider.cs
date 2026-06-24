using GPAA.Core.Chat;

namespace GPAA.Application.Ports;

public interface IStreamingChatCompletionProvider
{
    IAsyncEnumerable<string> CompleteStreamAsync(IReadOnlyList<ChatMessage> messages, CancellationToken cancellationToken);
}
