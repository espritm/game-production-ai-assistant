namespace GPAA.Core.Indexing;

public sealed class UnrealAssetRegistryDocumentRenderer
{
    public string Render(
        string objectPath,
        string packageName,
        string packagePath,
        string assetName,
        string assetClass,
        IReadOnlyDictionary<string, string> tags,
        IReadOnlyList<string>? dependencies,
        IReadOnlyList<string>? referencers)
    {
        var normalizedDependencies = NormalizeRelations(dependencies);
        var normalizedReferencers = NormalizeRelations(referencers);
        var lines = new List<string>
        {
            "Unreal Asset Registry metadata",
            $"ObjectPath: {objectPath}",
            $"PackageName: {packageName}",
            $"PackagePath: {packagePath}",
            $"AssetName: {assetName}",
            $"AssetClass: {assetClass}"
        };

        foreach (var tag in tags.OrderBy(tag => tag.Key, StringComparer.OrdinalIgnoreCase).Take(64))
        {
            if (!string.IsNullOrWhiteSpace(tag.Key) && !string.IsNullOrWhiteSpace(tag.Value))
            {
                lines.Add($"Tag.{tag.Key}: {tag.Value}");
            }
        }

        foreach (var dependency in normalizedDependencies)
        {
            lines.Add($"Dependency: {dependency}");
        }

        foreach (var referencer in normalizedReferencers)
        {
            lines.Add($"Referencer: {referencer}");
        }

        lines.Add($"SearchTerms: {BuildSearchTerms(objectPath, packageName, packagePath, assetName, assetClass, tags, normalizedDependencies, normalizedReferencers)}");
        return string.Join(Environment.NewLine, lines);
    }

    private static string BuildSearchTerms(
        string objectPath,
        string packageName,
        string packagePath,
        string assetName,
        string assetClass,
        IReadOnlyDictionary<string, string> tags,
        IReadOnlyList<string> dependencies,
        IReadOnlyList<string> referencers)
    {
        var terms = new List<string>
        {
            assetClass,
            objectPath,
            packageName,
            packagePath
        };

        terms.AddRange(SplitIdentifier(assetName));
        foreach (var tag in tags.Take(64))
        {
            terms.Add(tag.Key);
            terms.Add(tag.Value);
        }

        foreach (var dependency in dependencies)
        {
            terms.Add(dependency);
            terms.AddRange(SplitIdentifier(Path.GetFileName(dependency)));
        }

        foreach (var referencer in referencers)
        {
            terms.Add(referencer);
            terms.AddRange(SplitIdentifier(Path.GetFileName(referencer)));
        }

        return string.Join(' ', terms.Where(term => !string.IsNullOrWhiteSpace(term)).Distinct(StringComparer.OrdinalIgnoreCase));
    }

    private static IReadOnlyList<string> NormalizeRelations(IReadOnlyList<string>? relations)
    {
        if (relations is null)
        {
            return Array.Empty<string>();
        }

        return relations
            .Where(relation => !string.IsNullOrWhiteSpace(relation))
            .Select(relation => relation.Trim())
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .OrderBy(relation => relation, StringComparer.OrdinalIgnoreCase)
            .Take(128)
            .ToArray();
    }

    private static IEnumerable<string> SplitIdentifier(string value)
    {
        var current = new List<char>();
        foreach (var character in value.Replace('_', ' ').Replace('-', ' '))
        {
            if (char.IsWhiteSpace(character))
            {
                if (current.Count > 0)
                {
                    yield return new string(current.ToArray());
                    current.Clear();
                }

                continue;
            }

            if (current.Count > 0 && char.IsUpper(character) && !char.IsUpper(current[^1]))
            {
                yield return new string(current.ToArray());
                current.Clear();
            }

            current.Add(character);
        }

        if (current.Count > 0)
        {
            yield return new string(current.ToArray());
        }
    }
}
