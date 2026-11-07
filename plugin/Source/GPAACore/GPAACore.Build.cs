// Copyright Game Production AI Assistant contributors 2026.
using UnrealBuildTool;
public class GPAACore : ModuleRules
{
    public GPAACore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.Add("Core");
    }
}
