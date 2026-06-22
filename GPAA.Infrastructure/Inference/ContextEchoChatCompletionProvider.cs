using GPAA.Application.Ports;
using GPAA.Core.Chat;

namespace GPAA.Infrastructure.Inference;

public sealed class ContextEchoChatCompletionProvider : IChatCompletionProvider
{
    public Task<string> CompleteAsync(IReadOnlyList<ChatMessage> messages, CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();

        var prompt = messages.LastOrDefault(message => message.Role.Equals("user", StringComparison.OrdinalIgnoreCase))?.Content
            ?? "No prompt was provided.";

        var answer = "Local chat provider is running in echo mode. Configure Ollama to enable model-backed answers."
            + Environment.NewLine
            + Environment.NewLine
            + prompt;

        return Task.FromResult(answer);
    }
}
