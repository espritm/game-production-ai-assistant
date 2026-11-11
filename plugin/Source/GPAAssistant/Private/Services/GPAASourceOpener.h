// Copyright Game Production AI Assistant contributors 2026.
#pragma once

#include "CoreMinimal.h"
#include "Models/GPAAApiModels.h"

class FGPAASourceOpener
{
public:
    static bool ResolveAssetObjectPath(const FGPAASourceReference& Source, FString& OutPath);
    static bool OpenSource(
        const FGPAASourceReference& Source,
        const FString& ProjectRoot,
        const FString& Mode,
        const FString& CustomCommand,
        FString& OutStatus);

private:
    static bool TryRunSourceOpenerMode(const FString& Mode, const FString& AbsolutePath, int32 LineNumber, const FString& CustomCommand);
    static bool TryRunCustomSourceOpener(const FString& CommandTemplate, const FString& AbsolutePath, int32 LineNumber);
    static bool TryRunExecutable(const FString& Executable, const FString& Arguments);
    static bool TryOpenWithVisualStudioCode(const FString& AbsolutePath, int32 LineNumber);
    static bool TryOpenWithRider(const FString& AbsolutePath, int32 LineNumber);
    static bool TryOpenWithVisualStudio(const FString& AbsolutePath);
};
