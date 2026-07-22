using UnrealBuildTool;

public class GPAAssistant : ModuleRules
{
    public GPAAssistant(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "AssetRegistry",
            "CoreUObject",
            "DeveloperSettings",
            "Engine",
            "HTTP",
            "InputCore",
            "Json",
            "JsonUtilities",
            "Projects",
            "Settings",
            "Slate",
            "SlateCore",
            "ToolMenus",
            "UnrealEd"
        });
    }
}
