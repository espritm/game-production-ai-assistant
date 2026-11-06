[CmdletBinding()]
param(
    [string] $PackagePath = (Join-Path (Split-Path -Parent $PSScriptRoot) '.gpaa\PluginPackage'),
    [string] $UnrealEngineRoot = 'C:\Program Files\Epic Games\UE_5.6',
    [int] $TimeoutSeconds = 300
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Every run gets a fresh project. Preserve it, its logs, and the report on failure.
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$runRoot = Join-Path $repositoryRoot ('.gpaa\smoke\' + [guid]::NewGuid().ToString('N'))
$projectRoot = Join-Path $runRoot 'GPAASmoke'
$pluginRoot = Join-Path $projectRoot 'Plugins\GameProductionAIAssistant'
$editor = Join-Path $UnrealEngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $editor)) { throw "Unreal editor not found: $editor" }
& (Join-Path $PSScriptRoot 'verify-plugin-package.ps1') -PackagePath $PackagePath
New-Item -ItemType Directory -Force -Path $pluginRoot | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $projectRoot 'Content') | Out-Null
if ([IO.Path]::GetExtension($PackagePath) -ieq '.zip') {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::ExtractToDirectory((Resolve-Path -LiteralPath $PackagePath).Path, $pluginRoot)
} else {
    Get-ChildItem -LiteralPath $PackagePath -Force | Copy-Item -Destination $pluginRoot -Recurse
}

# A content-only host loads the precompiled C++ editor plugin without rebuilding
# it, so this checks the exact distribution binaries rather than source fallback.
$project = Join-Path $projectRoot 'GPAASmoke.uproject'
@{ FileVersion = 3; DisableEnginePluginsByDefault = $true; Plugins = @(@{ Name = 'GameProductionAIAssistant'; Enabled = $true }) } |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $project -Encoding UTF8
$listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, 0)
$listener.Start()
$port = $listener.LocalEndpoint.Port
$listener.Stop()
$url = "http://127.0.0.1:$port"
$configRoot = Join-Path $projectRoot 'Config'
New-Item -ItemType Directory -Force -Path $configRoot | Out-Null
@"
[/Script/GPAAssistant.GPAAEditorSettings]
BackendUrl="$url"
bAutoStartBackend=True
bStopBackendOnEditorShutdown=True
BackendExecutablePath=
BackendLaunchArguments=
"@ | Set-Content -LiteralPath (Join-Path $configRoot 'DefaultEditorPerProjectUserSettings.ini') -Encoding UTF8
@'
[GPAASmokeDDC]
Root=(Type=Memory, MaxCacheSize=256)
'@ | Set-Content -LiteralPath (Join-Path $configRoot 'DefaultEngine.ini') -Encoding UTF8
$report = Join-Path $runRoot 'Report'
$log = Join-Path $runRoot 'Unreal.log'
$backend = $null
$editorProcess = $null
$healthy = $false
$backendPathSuffix = '\' + (Split-Path -Leaf $runRoot) + '\GPAASmoke\Plugins\GameProductionAIAssistant\Resources\Backend\GPAA.Api.exe'
$packageHash = if (Test-Path -LiteralPath $PackagePath -PathType Leaf) { (Get-FileHash -LiteralPath $PackagePath).Hash } else { $null }
Write-Host "Smoke test evidence: $runRoot"
try {
    $arguments = @('"' + $project + '"', '-unattended', '-nop4', '-nosplash', '-nullrhi', '-nosound',
        '-DDC=GPAASmokeDDC', '-NoZenAutoLaunch', '-stdout', '-FullStdOutLogOutput', '-GPAASmokeBackendUrl=' + $url,
        '-ExecCmds="Automation RunTests GPAA.Plugin"', '-TestExit="Automation Test Queue Empty"',
        '-ReportExportPath="' + $report + '"', '-abslog="' + $log + '"')
    $editorProcess = Start-Process -FilePath $editor -ArgumentList $arguments -PassThru -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $runRoot 'stdout.log') -RedirectStandardError (Join-Path $runRoot 'stderr.log')
    $null = $editorProcess.Handle
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while (-not $editorProcess.HasExited) {
        if ([DateTime]::UtcNow -gt $deadline) { throw 'Unreal smoke test timed out.' }
        if ($null -eq $backend) {
            $candidate = Get-CimInstance Win32_Process -Filter "Name = 'GPAA.Api.exe'" |
                # WMI may report the physical drive rather than a SUBST alias.
                # Match the exact executable under this unique run directory.
                Where-Object { $_.ExecutablePath -and $_.ExecutablePath.EndsWith($backendPathSuffix, [StringComparison]::OrdinalIgnoreCase) -and
                    $_.CommandLine -match "--GpaaParentProcessId $($editorProcess.Id)(\s|$)" } | Select-Object -First 1
            if ($null -ne $candidate) { $backend = Get-Process -Id $candidate.ProcessId -ErrorAction Stop; $null = $backend.Handle }
        }
        if ($null -ne $backend -and -not $healthy) {
            try { $healthy = (Invoke-RestMethod "$url/health" -TimeoutSec 1).status -eq 'ok' } catch { }
        }
        Start-Sleep -Milliseconds 500
        $editorProcess.Refresh()
    }
    $editorProcess.WaitForExit()
    if ($editorProcess.ExitCode -ne 0) { throw "Unreal exited with code $($editorProcess.ExitCode). See $log" }
    if ($null -eq $backend -or -not $healthy) { throw 'The installed packaged backend was not observed healthy.' }
    if (-not $backend.WaitForExit(15000)) { throw 'Packaged backend survived editor shutdown.' }
    $resultPath = Join-Path $report 'index.json'
    if (-not (Test-Path -LiteralPath $resultPath)) { throw 'Unreal automation report is missing.' }
    $result = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
    if ($result.failed -gt 0 -or $result.succeeded -lt 5 -or $result.notRun -gt 0) {
        throw "Unreal automation failed or did not run all expected tests. See $resultPath"
    }
    $integration = @($result.tests | Where-Object { $_.fullTestPath -eq 'GPAA.Plugin.Integration.PackagedBackend' })
    if ($integration.Count -ne 1 -or $integration[0].state -ne 'Success') { throw 'Packaged backend integration test did not succeed.' }
    $logText = Get-Content -LiteralPath $log -Raw
    # UE 5.6's low-level test adapter emits generic CHECK diagnostics during
    # discovery, before the selected suite runs. Only that exact startup message
    # is excluded; selected test errors are always checked in the JSON report.
    $suiteStarted = $false
    foreach ($line in ($logText -split "`n")) {
        if ($line -match 'Test Started\..*Path=\{GPAA\.Plugin\.') { $suiteStarted = $true }
        if (-not $suiteStarted -and $line -match 'LogAutomationTest: Error: Condition failed\s*$') { continue }
        if ($line -match '(Fatal error:|Assertion failed:|Log\w+: Error:)') { throw "Release-blocking error in $log : $line" }
    }
    @{ succeeded = $true; backendHealthy = $healthy; backendStopped = $true; tests = $result.succeeded;
        package = (Resolve-Path -LiteralPath $PackagePath).Path; archiveSHA256 = $packageHash } |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $runRoot 'smoke-result.json') -Encoding UTF8
    Write-Host "PASS installed package, Unreal automation, backend health and shutdown. Evidence: $runRoot"
} finally {
    # Only terminate process handles discovered from this run's unique executable
    # and parent ID; never touch an existing user editor or backend.
    if ($null -ne $editorProcess -and -not $editorProcess.HasExited) { $editorProcess.Kill(); $editorProcess.WaitForExit() }
    if ($null -ne $backend -and -not $backend.HasExited) { $backend.Kill(); $backend.WaitForExit() }
}
