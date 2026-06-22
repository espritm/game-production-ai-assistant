namespace GPAA.Application.Chat;

public sealed record ChatProjectRequest(string ProjectRoot, string Question, int TopK = 8);
