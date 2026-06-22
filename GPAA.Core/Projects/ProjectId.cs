namespace GPAA.Core.Projects;

public readonly record struct ProjectId(string Value)
{
    public static ProjectId FromPath(string projectRoot)
    {
        if (string.IsNullOrWhiteSpace(projectRoot))
        {
            throw new ArgumentException("Project root is required.", nameof(projectRoot));
        }

        return new ProjectId(Path.GetFullPath(projectRoot).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar));
    }

    public override string ToString() => Value;
}
