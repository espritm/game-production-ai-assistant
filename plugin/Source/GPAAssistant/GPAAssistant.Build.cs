// Copyright Game Production AI Assistant contributors 2026.
using UnrealBuildTool;

public class GPAAssistant : ModuleRules
{
    public GPAAssistant(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicSystemLibraries.Add("Advapi32.lib");
        if (Target.bBuildDeveloperTools) PrivateDependencyModuleNames.AddRange(new[] { "HTTPServer", "Sockets" });

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "AssetRegistry",
            "GPAACore",
            "GPAAInference",
            "SQLiteCore",
            "CoreUObject",
            "DeveloperSettings",
            "Engine",
            "HTTP",
            "InputCore",
            "Json",
            "JsonUtilities",
            "Projects",
            "PropertyEditor",
            "Settings",
            "Slate",
            "SlateCore",
            "ToolMenus",
            "UnrealEd"
        });
    }
}
