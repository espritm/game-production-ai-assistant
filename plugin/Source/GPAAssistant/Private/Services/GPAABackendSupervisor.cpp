#include "Services/GPAABackendSupervisor.h"

#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Interfaces/IPluginManager.h"
#include "Settings/GPAAEditorSettings.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

namespace
{
    constexpr int32 BackendStartupAttempts = 20;
    constexpr float BackendStartupRetrySeconds = 0.5f;
    FProcHandle LaunchedBackendProcess;

    bool IsBackendProjectFile(const FString& Candidate)
    {
        return !Candidate.IsEmpty() && FPaths::FileExists(Candidate);
    }

    bool IsExecutableFile(const FString& Candidate)
    {
        return !Candidate.IsEmpty() && FPaths::FileExists(Candidate);
    }

    FString FindBackendProjectNear(const FString& StartDirectory)
    {
        FString Directory = FPaths::ConvertRelativePathToFull(StartDirectory);
        FPaths::NormalizeDirectoryName(Directory);

        for (int32 Depth = 0; Depth < 8 && !Directory.IsEmpty(); ++Depth)
        {
            const FString Candidate = FPaths::Combine(Directory, TEXT("GPAA.Api"), TEXT("GPAA.Api.csproj"));
            if (IsBackendProjectFile(Candidate))
            {
                return Candidate;
            }

            FString Parent = FPaths::GetPath(Directory);
            if (Parent.Equals(Directory, ESearchCase::IgnoreCase))
            {
                break;
            }

            Directory = Parent;
        }

        return FString();
    }
}

void FGPAABackendSupervisor::ShutdownLaunchedBackend()
{
    const UGPAAEditorSettings* Settings = GetDefault<UGPAAEditorSettings>();
    if (!Settings || !Settings->bStopBackendOnEditorShutdown || !LaunchedBackendProcess.IsValid())
    {
        return;
    }

    FPlatformProcess::TerminateProc(LaunchedBackendProcess, true);
    FPlatformProcess::CloseProc(LaunchedBackendProcess);
    LaunchedBackendProcess.Reset();
}

void FGPAABackendSupervisor::EnsureBackendReady(
    const UGPAAEditorSettings& Settings,
    FGPAARequestCompleted OnStatusChanged,
    FGPAARequestCompleted OnCompleted)
{
    const FString BackendUrl = NormalizeBackendUrl(Settings.BackendUrl);
    CheckHealth(BackendUrl, FGPAARequestCompleted::CreateLambda(
        [this, BackendUrl, OnStatusChanged, OnCompleted](bool bSucceeded, const FString& Message)
        {
            RetryHealthCheck(BackendUrl, -1, OnStatusChanged, OnCompleted, bSucceeded, Message);
        }));
}

void FGPAABackendSupervisor::CheckHealth(const FString& BackendUrl, FGPAARequestCompleted OnCompleted)
{
    const FString Url = NormalizeBackendUrl(BackendUrl) + TEXT("/health");
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Url);
    Request->SetVerb(TEXT("GET"));
    Request->SetTimeout(2.0f);
    Request->SetActivityTimeout(2.0f);
    Request->OnProcessRequestComplete().BindLambda([OnCompleted](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
    {
        const bool bHealthy = bWasSuccessful && Response.IsValid() && Response->GetResponseCode() >= 200 && Response->GetResponseCode() < 300;
        OnCompleted.ExecuteIfBound(bHealthy, bHealthy ? TEXT("Backend is ready.") : TEXT("Backend is not responding."));
    });

    if (!Request->ProcessRequest())
    {
        OnCompleted.ExecuteIfBound(false, TEXT("Could not send backend health request."));
    }
}

void FGPAABackendSupervisor::RetryHealthCheck(
    const FString& BackendUrl,
    int32 RemainingAttempts,
    FGPAARequestCompleted OnStatusChanged,
    FGPAARequestCompleted OnCompleted,
    bool bSucceeded,
    const FString& Message)
{
    if (bSucceeded)
    {
        OnCompleted.ExecuteIfBound(true, Message);
        return;
    }

    if (RemainingAttempts == 0)
    {
        OnCompleted.ExecuteIfBound(false, TEXT("The local backend did not become ready before the startup timeout."));
        return;
    }

    if (RemainingAttempts < 0)
    {
        const UGPAAEditorSettings* Settings = GetDefault<UGPAAEditorSettings>();
        if (!Settings || !Settings->bAutoStartBackend)
        {
            OnCompleted.ExecuteIfBound(false, TEXT("The local backend did not respond."));
            return;
        }

        FString StartMessage;
        if (!TryStartBackend(*Settings, StartMessage))
        {
            OnCompleted.ExecuteIfBound(false, StartMessage);
            return;
        }

        OnStatusChanged.ExecuteIfBound(true, StartMessage);
        RetryHealthCheck(BackendUrl, BackendStartupAttempts, OnStatusChanged, OnCompleted, false, Message);
        return;
    }

    const TSharedRef<FGPAABackendSupervisor, ESPMode::ThreadSafe> Self = AsShared();
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
        [Self, BackendUrl, RemainingAttempts, OnStatusChanged, OnCompleted](float)
        {
            Self->CheckHealth(
                BackendUrl,
                FGPAARequestCompleted::CreateLambda(
                    [Self, BackendUrl, RemainingAttempts, OnStatusChanged, OnCompleted](bool bSucceeded, const FString& Message)
                    {
                        Self->RetryHealthCheck(BackendUrl, RemainingAttempts - 1, OnStatusChanged, OnCompleted, bSucceeded, Message);
                    }));

            return false;
        }),
        BackendStartupRetrySeconds);
}

bool FGPAABackendSupervisor::TryStartBackend(const UGPAAEditorSettings& Settings, FString& OutMessage)
{
    if (LaunchedBackendProcess.IsValid())
    {
        if (FPlatformProcess::IsProcRunning(LaunchedBackendProcess))
        {
            OutMessage = TEXT("Starting local backend...");
            return true;
        }

        FPlatformProcess::CloseProc(LaunchedBackendProcess);
        LaunchedBackendProcess.Reset();
    }

    FString Executable;
    FString Arguments;
    FString WorkingDirectory;

    const bool bHasCustomExecutable = !Settings.BackendExecutablePath.TrimStartAndEnd().IsEmpty()
        && !IsDefaultDotnetExecutable(Settings.BackendExecutablePath);

    if (bHasCustomExecutable)
    {
        Executable = Settings.BackendExecutablePath;
        Arguments = Settings.BackendLaunchArguments;
        Arguments.ReplaceInline(TEXT("{url}"), *NormalizeBackendUrl(Settings.BackendUrl));
        WorkingDirectory = FPaths::GetPath(FPaths::ConvertRelativePathToFull(Executable));
    }
    else if (!TryResolvePackagedBackendCommand(Settings, Executable, Arguments, WorkingDirectory)
        && !TryResolveDeveloperBackendCommand(Settings, Executable, Arguments, WorkingDirectory))
    {
        OutMessage = TEXT("The local backend is not running, and GPAA could not find a packaged backend or GPAA.Api.csproj. Install a package built with scripts/package-unreal-plugin.ps1, or set Backend Project Path in Project Settings.");
        return false;
    }

    FProcHandle Process = FPlatformProcess::CreateProc(
        *Executable,
        *AppendParentProcessArgument(Arguments),
        true,
        true,
        true,
        nullptr,
        0,
        *WorkingDirectory,
        nullptr,
        nullptr);

    if (!Process.IsValid())
    {
        OutMessage = FString::Printf(TEXT("The local backend is not running, and GPAA could not launch %s %s."), *Executable, *Arguments);
        return false;
    }

    LaunchedBackendProcess = Process;
    OutMessage = TEXT("Starting local backend...");
    return true;
}

bool FGPAABackendSupervisor::TryResolvePackagedBackendCommand(
    const UGPAAEditorSettings& Settings,
    FString& OutExecutable,
    FString& OutArguments,
    FString& OutWorkingDirectory) const
{
    const FString PackagedExecutable = ResolvePackagedBackendExecutable();
    if (!IsExecutableFile(PackagedExecutable))
    {
        return false;
    }

    OutExecutable = PackagedExecutable;
    OutWorkingDirectory = FPaths::GetPath(PackagedExecutable);

    const bool bHasCustomArguments = !Settings.BackendLaunchArguments.TrimStartAndEnd().IsEmpty()
        && !IsDefaultDeveloperLaunchArguments(Settings.BackendLaunchArguments);

    OutArguments = bHasCustomArguments
        ? Settings.BackendLaunchArguments
        : FString::Printf(TEXT("--urls %s"), *NormalizeBackendUrl(Settings.BackendUrl));
    OutArguments.ReplaceInline(TEXT("{url}"), *NormalizeBackendUrl(Settings.BackendUrl));
    OutArguments.ReplaceInline(TEXT("{backend}"), *PackagedExecutable);
    return true;
}

bool FGPAABackendSupervisor::TryResolveDeveloperBackendCommand(
    const UGPAAEditorSettings& Settings,
    FString& OutExecutable,
    FString& OutArguments,
    FString& OutWorkingDirectory) const
{
    const FString BackendProjectPath = ResolveBackendProjectPath(Settings);
    if (BackendProjectPath.IsEmpty())
    {
        return false;
    }

    OutExecutable = Settings.BackendExecutablePath.IsEmpty() ? TEXT("dotnet") : Settings.BackendExecutablePath;
    OutArguments = ResolveLaunchArguments(Settings, BackendProjectPath);
    OutWorkingDirectory = FPaths::GetPath(BackendProjectPath);
    return true;
}

FString FGPAABackendSupervisor::ResolveBackendProjectPath(const UGPAAEditorSettings& Settings) const
{
    if (IsBackendProjectFile(Settings.BackendProjectPath))
    {
        return FPaths::ConvertRelativePathToFull(Settings.BackendProjectPath);
    }

    if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("GameProductionAIAssistant")))
    {
        const FString Candidate = FindBackendProjectNear(Plugin->GetBaseDir());
        if (!Candidate.IsEmpty())
        {
            return Candidate;
        }
    }

    const FString ProjectCandidate = FindBackendProjectNear(FPaths::ProjectDir());
    if (!ProjectCandidate.IsEmpty())
    {
        return ProjectCandidate;
    }

    const FString WorkspaceCandidate = TEXT("C:/Workspace/Game-Production-AI-Assistant/GPAA.Api/GPAA.Api.csproj");
    return IsBackendProjectFile(WorkspaceCandidate) ? WorkspaceCandidate : FString();
}

FString FGPAABackendSupervisor::ResolvePackagedBackendExecutable() const
{
    if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("GameProductionAIAssistant")))
    {
        return FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("Backend"), TEXT("GPAA.Api.exe"));
    }

    return FString();
}

FString FGPAABackendSupervisor::ResolveLaunchArguments(const UGPAAEditorSettings& Settings, const FString& BackendProjectPath) const
{
    if (!Settings.BackendLaunchArguments.IsEmpty())
    {
        FString Arguments = Settings.BackendLaunchArguments;
        Arguments.ReplaceInline(TEXT("{project}"), *BackendProjectPath);
        Arguments.ReplaceInline(TEXT("{url}"), *NormalizeBackendUrl(Settings.BackendUrl));
        return Arguments;
    }

    return FString::Printf(
        TEXT("run --project \"%s\" --urls %s"),
        *BackendProjectPath,
        *NormalizeBackendUrl(Settings.BackendUrl));
}

bool FGPAABackendSupervisor::IsDefaultDotnetExecutable(const FString& ExecutablePath)
{
    const FString Normalized = FPaths::GetBaseFilename(ExecutablePath.TrimStartAndEnd());
    return Normalized.IsEmpty() || Normalized.Equals(TEXT("dotnet"), ESearchCase::IgnoreCase);
}

bool FGPAABackendSupervisor::IsDefaultDeveloperLaunchArguments(const FString& Arguments)
{
    const FString Normalized = Arguments.TrimStartAndEnd();
    return Normalized.IsEmpty()
        || Normalized.Equals(TEXT("run --project \"{project}\" --urls {url}"), ESearchCase::IgnoreCase);
}

FString FGPAABackendSupervisor::NormalizeBackendUrl(const FString& BackendUrl)
{
    FString Normalized = BackendUrl.TrimStartAndEnd();
    while (Normalized.EndsWith(TEXT("/")))
    {
        Normalized.LeftChopInline(1);
    }

    return Normalized.IsEmpty() ? TEXT("http://localhost:5000") : Normalized;
}

FString FGPAABackendSupervisor::AppendParentProcessArgument(const FString& Arguments)
{
    return FString::Printf(
        TEXT("%s --GpaaParentProcessId %u"),
        *Arguments.TrimStartAndEnd(),
        FPlatformProcess::GetCurrentProcessId());
}
