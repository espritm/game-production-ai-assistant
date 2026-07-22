using System.Text;
using GPAA.Application.Ports;
using GPAA.Application.Search;
using GPAA.Core.Chat;
using GPAA.Core.Projects;

namespace GPAA.Application.Chat;

public sealed class ProjectChatService
{
    private const int MaxChatSources = 2;
    private const int MaxSourceCharacters = 700;
    private const int MaxTotalContextCharacters = 1600;

    private readonly SemanticSearchService _searchService;
    private readonly IChatCompletionProvider _chatCompletionProvider;
    private readonly IStreamingChatCompletionProvider _streamingChatCompletionProvider;
    private readonly IChatHistoryRepository _historyRepository;

    public ProjectChatService(
        SemanticSearchService searchService,
        IChatCompletionProvider chatCompletionProvider,
        IStreamingChatCompletionProvider streamingChatCompletionProvider,
        IChatHistoryRepository historyRepository)
    {
        _searchService = searchService;
        _chatCompletionProvider = chatCompletionProvider;
        _streamingChatCompletionProvider = streamingChatCompletionProvider;
        _historyRepository = historyRepository;
    }

    public async Task<ChatProjectResponse> AskAsync(ChatProjectRequest request, CancellationToken cancellationToken)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(request.Question);

        var results = await _searchService.SearchAsync(
            new SearchProjectRequest(request.ProjectRoot, request.Question, request.TopK, request.SourceFilter),
            cancellationToken);

        var prompt = BuildPrompt(request.Question, results.Take(MaxChatSources).ToArray());
        var messages = new[]
        {
            new ChatMessage("system", "You are a local-first AI assistant for Unreal Engine production projects. Answer with concise technical guidance and cite file references when context is available."),
            new ChatMessage("user", prompt)
        };

        var answer = await _chatCompletionProvider.CompleteAsync(messages, cancellationToken);
        await _historyRepository.AddAsync(
            ProjectId.FromPath(request.ProjectRoot),
            new ChatHistoryEntry(
                Guid.NewGuid().ToString("N"),
                ProjectId.FromPath(request.ProjectRoot).Value,
                request.Question,
                answer,
                results,
                DateTimeOffset.UtcNow),
            cancellationToken);

        return new ChatProjectResponse(answer, results);
    }

    public async IAsyncEnumerable<string> AskStreamAsync(
        ChatProjectRequest request,
        [System.Runtime.CompilerServices.EnumeratorCancellation] CancellationToken cancellationToken)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(request.Question);

        var results = await _searchService.SearchAsync(
            new SearchProjectRequest(request.ProjectRoot, request.Question, request.TopK, request.SourceFilter),
            cancellationToken);

        var prompt = BuildPrompt(request.Question, results.Take(MaxChatSources).ToArray());
        var messages = new[]
        {
            new ChatMessage("system", "You are a local-first AI assistant for Unreal Engine production projects. Answer with concise technical guidance and cite file references when context is available."),
            new ChatMessage("user", prompt)
        };

        var answerBuilder = new StringBuilder();
        await foreach (var fragment in _streamingChatCompletionProvider.CompleteStreamAsync(messages, cancellationToken))
        {
            answerBuilder.Append(fragment);
            yield return fragment;
        }

        await _historyRepository.AddAsync(
            ProjectId.FromPath(request.ProjectRoot),
            new ChatHistoryEntry(
                Guid.NewGuid().ToString("N"),
                ProjectId.FromPath(request.ProjectRoot).Value,
                request.Question,
                answerBuilder.ToString(),
                results,
                DateTimeOffset.UtcNow),
            cancellationToken);
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
            var totalContextCharacters = 0;
            foreach (var result in results.Take(MaxChatSources))
            {
                if (totalContextCharacters >= MaxTotalContextCharacters)
                {
                    break;
                }

                var content = LimitSourceContent(result.Content, Math.Min(MaxSourceCharacters, MaxTotalContextCharacters - totalContextCharacters));
                totalContextCharacters += content.Length;

                builder.AppendLine($"Source: {result.RelativePath}:{result.StartLine}-{result.EndLine}");
                builder.AppendLine(content);
                builder.AppendLine();
            }
        }

        builder.AppendLine("Answer in at most 8 concise bullet points.");
        builder.AppendLine("Answer using only the provided context.");
        builder.AppendLine("If no source directly supports the answer, say that no matching implementation was found.");
        builder.AppendLine("Do not invent systems, files, classes, or engine behavior that is not present in the sources.");
        builder.AppendLine("When a source supports a claim, cite it as Source: relative/path:start-end.");
        return builder.ToString();
    }

    private static string LimitSourceContent(string content, int maxCharacters)
    {
        if (content.Length <= maxCharacters)
        {
            return content;
        }

        return content[..maxCharacters] + Environment.NewLine + "[Source truncated for local chat performance.]";
    }
}
