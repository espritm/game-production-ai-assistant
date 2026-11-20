// Copyright Game Production AI Assistant contributors 2026.
using UnrealBuildTool;
using System.IO;
public class GPAAInference : ModuleRules
{
    public GPAAInference(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "GPAACore" });
        PrivateDependencyModuleNames.AddRange(new[] { "HTTP", "Json", "Projects", "CoreUObject", "Engine", "ICU" });
        string Dependencies = Path.Combine(ModuleDirectory, "..", "ThirdParty");
        PrivateIncludePaths.Add(Path.Combine(Dependencies, "OnnxRuntime", "include"));
        PrivateIncludePaths.Add(Path.Combine(Dependencies, "Llama", "include"));
        PublicSystemLibraries.AddRange(new[] { "winhttp.lib", "bcrypt.lib" });
        foreach (string Library in new[] { "llama", "ggml", "ggml-base", "ggml-cpu" })
            PublicAdditionalLibraries.Add(Path.Combine(Dependencies, "Llama", "Win64", Library + ".lib"));
        RuntimeDependencies.Add(Path.Combine(Dependencies, "OnnxRuntime", "Win64", "gpaa_onnxruntime.dll"));
        RuntimeDependencies.Add(Path.Combine(Dependencies, "MiniLM", "model.onnx"));
        RuntimeDependencies.Add(Path.Combine(Dependencies, "MiniLM", "vocab.txt"));
    }
}
