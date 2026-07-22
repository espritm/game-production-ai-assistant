#include "Services/GPAASourceOpener.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

namespace
{
    const FString SourceOpenerAuto = TEXT("Auto");
    const FString SourceOpenerVSCode = TEXT("Visual Studio Code");
    const FString SourceOpenerRider = TEXT("Rider");
    const FString SourceOpenerVisualStudio = TEXT("Visual Studio");
    const FString SourceOpenerDefault = TEXT("Default Windows App");
    const FString SourceOpenerCustom = TEXT("Custom Command");
}

bool FGPAASourceOpener::OpenSource(
    const FGPAASourceReference& Source,
    const FString& ProjectRoot,
    const FString& Mode,
    const FString& CustomCommand,
    FString& OutStatus)
{
    if (Source.SourceType.Equals(TEXT("Asset"), ESearchCase::IgnoreCase)
        || Source.RelativePath.StartsWith(TEXT(".gpaa/unreal-assets/"), ESearchCase::IgnoreCase))
    {
        OutStatus = TEXT("Asset metadata is a virtual knowledge source. Use the package path shown in the result to locate the asset in Unreal.");
        return true;
    }

    const FString AbsolutePath = FPaths::ConvertRelativePathToFull(FPaths::Combine(ProjectRoot, Source.RelativePath));
    if (!FPaths::FileExists(AbsolutePath))
    {
        OutStatus = FString::Printf(TEXT("File not found: %s"), *AbsolutePath);
        return false;
    }

    if (!TryRunSourceOpenerMode(Mode, AbsolutePath, Source.StartLine, CustomCommand))
    {
        FPlatformProcess::LaunchFileInDefaultExternalApplication(*AbsolutePath);
    }

    OutStatus = FString::Printf(TEXT("Opened %s"), *Source.RelativePath);
    return true;
}

bool FGPAASourceOpener::TryRunSourceOpenerMode(const FString& Mode, const FString& AbsolutePath, int32 LineNumber, const FString& CustomCommand)
{
    if (Mode == SourceOpenerDefault)
    {
        return false;
    }

    if (Mode == SourceOpenerCustom)
    {
        return TryRunCustomSourceOpener(CustomCommand, AbsolutePath, LineNumber);
    }

    if (Mode == SourceOpenerVSCode)
    {
        return TryOpenWithVisualStudioCode(AbsolutePath, LineNumber);
    }

    if (Mode == SourceOpenerRider)
    {
        return TryOpenWithRider(AbsolutePath, LineNumber);
    }

    if (Mode == SourceOpenerVisualStudio)
    {
        return TryOpenWithVisualStudio(AbsolutePath);
    }

    return TryOpenWithVisualStudioCode(AbsolutePath, LineNumber)
        || TryOpenWithRider(AbsolutePath, LineNumber)
        || TryOpenWithVisualStudio(AbsolutePath);
}

bool FGPAASourceOpener::TryRunCustomSourceOpener(const FString& CommandTemplate, const FString& AbsolutePath, int32 LineNumber)
{
    FString Command = CommandTemplate.TrimStartAndEnd();
    if (Command.IsEmpty())
    {
        return false;
    }

    Command.ReplaceInline(TEXT("{file}"), *AbsolutePath);
    Command.ReplaceInline(TEXT("{line}"), *FString::FromInt(FMath::Max(LineNumber, 1)));

    FString Executable;
    FString Arguments;
    if (Command.StartsWith(TEXT("\"")))
    {
        const int32 ClosingQuoteIndex = Command.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, 1);
        if (ClosingQuoteIndex <= 0)
        {
            return false;
        }

        Executable = Command.Mid(1, ClosingQuoteIndex - 1);
        Arguments = Command.Mid(ClosingQuoteIndex + 1).TrimStartAndEnd();
    }
    else
    {
        FString Left;
        FString Right;
        if (Command.Split(TEXT(" "), &Left, &Right))
        {
            Executable = Left;
            Arguments = Right;
        }
        else
        {
            Executable = Command;
        }
    }

    return !Executable.IsEmpty() && TryRunExecutable(Executable, Arguments);
}

bool FGPAASourceOpener::TryRunExecutable(const FString& Executable, const FString& Arguments)
{
    if (Executable.IsEmpty())
    {
        return false;
    }

    FProcHandle Process = FPlatformProcess::CreateProc(*Executable, *Arguments, true, false, false, nullptr, 0, nullptr, nullptr);
    if (!Process.IsValid())
    {
        return false;
    }

    FPlatformProcess::CloseProc(Process);
    return true;
}

bool FGPAASourceOpener::TryOpenWithVisualStudioCode(const FString& AbsolutePath, int32 LineNumber)
{
    const FString LocalAppData = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));
    const FString ProgramFiles = FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles"));
    const FString ProgramFilesX86 = FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles(x86)"));

    TArray<FString> Candidates;
    Candidates.Add(FPaths::Combine(LocalAppData, TEXT("Programs/Microsoft VS Code/Code.exe")));
    Candidates.Add(FPaths::Combine(ProgramFiles, TEXT("Microsoft VS Code/Code.exe")));
    Candidates.Add(FPaths::Combine(ProgramFilesX86, TEXT("Microsoft VS Code/Code.exe")));

    const FString Arguments = FString::Printf(TEXT("-g \"%s:%d\""), *AbsolutePath, FMath::Max(LineNumber, 1));
    for (const FString& Candidate : Candidates)
    {
        if (FPaths::FileExists(Candidate) && TryRunExecutable(Candidate, Arguments))
        {
            return true;
        }
    }

    return TryRunExecutable(TEXT("code"), Arguments);
}

bool FGPAASourceOpener::TryOpenWithRider(const FString& AbsolutePath, int32 LineNumber)
{
    const FString LocalAppData = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));
    TArray<FString> Candidates;
    if (!LocalAppData.IsEmpty())
    {
        IFileManager::Get().FindFilesRecursive(
            Candidates,
            *FPaths::Combine(LocalAppData, TEXT("JetBrains/Toolbox/apps")),
            TEXT("rider64.exe"),
            true,
            false);
    }

    const FString Arguments = FString::Printf(TEXT("--line %d \"%s\""), FMath::Max(LineNumber, 1), *AbsolutePath);
    for (const FString& Candidate : Candidates)
    {
        if (TryRunExecutable(Candidate, Arguments))
        {
            return true;
        }
    }

    return TryRunExecutable(TEXT("rider64"), Arguments) || TryRunExecutable(TEXT("rider"), Arguments);
}

bool FGPAASourceOpener::TryOpenWithVisualStudio(const FString& AbsolutePath)
{
    const FString ProgramFiles = FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles"));
    const FString ProgramFilesX86 = FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles(x86)"));

    TArray<FString> Candidates;
    const TArray<FString> Roots = { ProgramFiles, ProgramFilesX86 };
    const TArray<FString> Versions = { TEXT("18"), TEXT("2022"), TEXT("2019") };
    const TArray<FString> Editions = { TEXT("Community"), TEXT("Professional"), TEXT("Enterprise") };

    for (const FString& Root : Roots)
    {
        if (Root.IsEmpty())
        {
            continue;
        }

        for (const FString& Version : Versions)
        {
            for (const FString& Edition : Editions)
            {
                Candidates.Add(FPaths::Combine(Root, TEXT("Microsoft Visual Studio"), Version, Edition, TEXT("Common7/IDE/devenv.exe")));
            }
        }
    }

    const FString Arguments = FString::Printf(TEXT("/Edit \"%s\""), *AbsolutePath);
    for (const FString& Candidate : Candidates)
    {
        if (FPaths::FileExists(Candidate) && TryRunExecutable(Candidate, Arguments))
        {
            return true;
        }
    }

    return TryRunExecutable(TEXT("devenv"), Arguments);
}
