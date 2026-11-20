using GPAA.Core.Chat;

namespace GPAA.Application.Ports;

public interface IChatCompletionProvider
{
    Task<string> CompleteAsync(IReadOnlyList<ChatMessage> messages, CancellationToken cancellationToken);
}
