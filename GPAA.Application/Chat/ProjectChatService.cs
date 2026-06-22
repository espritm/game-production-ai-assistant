using System.Text;
using GPAA.Application.Ports;
using GPAA.Application.Search;
using GPAA.Core.Chat;

namespace GPAA.Application.Chat;

public sealed class ProjectChatService
{
    private readonly SemanticSearchService _searchService;
    private readonly IChatCompletionProvider _chatCompletionProvider;

    public ProjectChatService(SemanticSearchService searchService, IChatCompletionProvider chatCompletionProvider)
    {
        _searchService = searchService;
        _chatCompletionProvider = chatCompletionProvider;
    }

    public async Task<string> AskAsync(ChatProjectRequest request, CancellationToken cancellationToken)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(request.Question);

        var results = await _searchService.SearchAsync(
            new SearchProjectRequest(request.ProjectRoot, request.Question, request.TopK),
            cancellationToken);

        var prompt = BuildPrompt(request.Question, results);
        var messages = new[]
        {
            new ChatMessage("system", "You are a local-first AI assistant for Unreal Engine production projects. Answer with concise technical guidance and cite file references when context is available."),
            new ChatMessage("user", prompt)
        };

        return await _chatCompletionProvider.CompleteAsync(messages, cancellationToken);
    }

    private static string BuildPrompt(string question, IReadOnlyList<Core.Search.SearchResult> results)
    {
        var builder = new StringBuilder();
        builder.AppendLine("Question:");
        builder.AppendLine(question);
        builder.AppendLine();
        builder.AppendLine("Relevant project context:");

        if (results.Count == 0)
        {
            builder.AppendLine("No indexed context was found.");
        }
        else
        {
            foreach (var result in results)
            {
                builder.AppendLine($"Source: {result.RelativePath}:{result.StartLine}-{result.EndLine}");
                builder.AppendLine(result.Content);
                builder.AppendLine();
            }
        }

        builder.AppendLine("Answer using only the context when possible. If the context is insufficient, say what is missing.");
        return builder.ToString();
    }
}
