#pragma once

#include "CoreMinimal.h"

#include "Services/GPAAHttpClient.h"

class UGPAAEditorSettings;

class FGPAABackendSupervisor : public TSharedFromThis<FGPAABackendSupervisor, ESPMode::ThreadSafe>
{
public:
    void EnsureBackendReady(
        const UGPAAEditorSettings& Settings,
        FGPAARequestCompleted OnStatusChanged,
        FGPAARequestCompleted OnCompleted);

    static void ShutdownLaunchedBackend();

private:
    void CheckHealth(const FString& BackendUrl, FGPAARequestCompleted OnCompleted);
    void RetryHealthCheck(
        const FString& BackendUrl,
        int32 RemainingAttempts,
        FGPAARequestCompleted OnStatusChanged,
        FGPAARequestCompleted OnCompleted,
        bool bSucceeded,
        const FString& Message);
    bool TryStartBackend(const UGPAAEditorSettings& Settings, FString& OutMessage);
    bool TryResolvePackagedBackendCommand(const UGPAAEditorSettings& Settings, FString& OutExecutable, FString& OutArguments, FString& OutWorkingDirectory) const;
    bool TryResolveDeveloperBackendCommand(const UGPAAEditorSettings& Settings, FString& OutExecutable, FString& OutArguments, FString& OutWorkingDirectory) const;
    FString ResolveBackendProjectPath(const UGPAAEditorSettings& Settings) const;
    FString ResolvePackagedBackendExecutable() const;
    FString ResolveLaunchArguments(const UGPAAEditorSettings& Settings, const FString& BackendProjectPath) const;
    static bool IsDefaultDotnetExecutable(const FString& ExecutablePath);
    static bool IsDefaultDeveloperLaunchArguments(const FString& Arguments);
    static FString NormalizeBackendUrl(const FString& BackendUrl);
    static FString AppendParentProcessArgument(const FString& Arguments);
};
