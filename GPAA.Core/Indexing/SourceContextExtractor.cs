using System.Text.RegularExpressions;

namespace GPAA.Core.Indexing;

public sealed partial class SourceContextExtractor
{
    public string Extract(string relativePath, string content)
    {
        var declarations = new List<string>();

        foreach (Match match in UnrealMacroRegex().Matches(content))
        {
            declarations.Add(match.Value);
        }

        foreach (Match match in TypeDeclarationRegex().Matches(content))
        {
            declarations.Add(match.Value.Trim());
        }

        foreach (Match match in FunctionDeclarationRegex().Matches(content))
        {
            var declaration = match.Value.Trim();
            if (!declaration.StartsWith("if", StringComparison.Ordinal)
                && !declaration.StartsWith("for", StringComparison.Ordinal)
                && !declaration.StartsWith("while", StringComparison.Ordinal)
                && !declaration.StartsWith("switch", StringComparison.Ordinal))
            {
                declarations.Add(declaration);
            }
        }

        if (relativePath.EndsWith(".Build.cs", StringComparison.OrdinalIgnoreCase))
        {
            foreach (Match match in ModuleRuleRegex().Matches(content))
            {
                declarations.Add(match.Value.Trim());
            }
        }

        if (declarations.Count == 0)
        {
            return string.Empty;
        }

        return string.Join(Environment.NewLine, declarations.Distinct().Take(16));
    }

    [GeneratedRegex(@"\bU(CLASS|STRUCT|ENUM|INTERFACE|FUNCTION|PROPERTY)\s*\([^)]*\)", RegexOptions.Compiled)]
    private static partial Regex UnrealMacroRegex();

    [GeneratedRegex(@"\b(class|struct|enum)\s+[A-Za-z_][A-Za-z0-9_]*", RegexOptions.Compiled)]
    private static partial Regex TypeDeclarationRegex();

    [GeneratedRegex(@"\b[A-Za-z_][A-Za-z0-9_:<>,\s*&]*\s+[A-Za-z_][A-Za-z0-9_:]*\s*\([^;{}]*\)\s*(const)?\s*(override|final)?", RegexOptions.Compiled)]
    private static partial Regex FunctionDeclarationRegex();

    [GeneratedRegex(@"\bpublic\s+class\s+[A-Za-z_][A-Za-z0-9_]*\s*:\s*ModuleRules\b", RegexOptions.Compiled)]
    private static partial Regex ModuleRuleRegex();
}
