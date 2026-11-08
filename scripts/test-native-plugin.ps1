[CmdletBinding()]
param(
    [string] $PackagePath,
    [string] $UnrealEngineRoot = 'C:\Program Files\Epic Games\UE_5.6',
    [int] $TimeoutSeconds = 300,
    [string] $TestFilter = 'GPAA.Native',
    [switch] $EngineInstallation,
    [string] $AcceptanceProject,
    [string] $WorkspaceRoot,
    [switch] $InteractiveAssetEditor,
    [switch] $HttpShutdownAcceptance
)
$ErrorActionPreference = 'Stop'
if ($HttpShutdownAcceptance) {
    if ($AcceptanceProject -or $InteractiveAssetEditor) { throw 'HTTP shutdown acceptance must run in isolation.' }
    $TestFilter='GPAA.Acceptance.HttpShutdown'
}
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
if ($WorkspaceRoot) { $root=(Resolve-Path -LiteralPath $WorkspaceRoot).Path }
if ([string]::IsNullOrWhiteSpace($PackagePath)) {$PackagePath=Join-Path $root '.gpaa\NativePackage'}
$run = Join-Path $root ('.gpaa\native-smoke\' + [guid]::NewGuid().ToString('N'))
$projectRoot = Join-Path $run 'Host'
$plugin = Join-Path $projectRoot 'Plugins\GameProductionAIAssistant'
if ($EngineInstallation) {
    $plugin = Join-Path $UnrealEngineRoot 'Engine\Plugins\Marketplace\GameProductionAIAssistant'
    if (Test-Path -LiteralPath $plugin) { throw 'Engine plugin destination already exists; refusing to overwrite it.' }
}
New-Item -ItemType Directory -Force -Path $plugin | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $projectRoot 'Config'),(Join-Path $projectRoot 'Content'),(Join-Path $projectRoot 'Source') | Out-Null
if ([IO.Path]::GetExtension($PackagePath) -ieq '.zip') {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::ExtractToDirectory((Resolve-Path -LiteralPath $PackagePath).Path, $plugin)
} else { Get-ChildItem -LiteralPath $PackagePath -Force | Copy-Item -Destination $plugin -Recurse }
$pluginBefore=@{}
Get-ChildItem -LiteralPath $plugin -Recurse -File | ForEach-Object { $pluginBefore[$_.FullName]=(Get-FileHash -LiteralPath $_.FullName).Hash }
@{FileVersion=3;DisableEnginePluginsByDefault=$true;Plugins=@(@{Name='GameProductionAIAssistant';Enabled=$true})} |
    ConvertTo-Json -Depth 5 | Set-Content (Join-Path $projectRoot 'Host.uproject') -Encoding UTF8
@'
[GPAASmokeDDC]
Root=(Type=Memory, MaxCacheSize=256)
'@ | Set-Content (Join-Path $projectRoot 'Config\DefaultEngine.ini') -Encoding UTF8
@'
[/Script/GPAAssistant.GPAAEditorSettings]
bCloudContextAccepted=False
'@ | Set-Content (Join-Path $projectRoot 'Config\DefaultEditorPerProjectUserSettings.ini') -Encoding UTF8
'// Test fixture: stamina regenerates over time after a cooldown.' | Set-Content (Join-Path $projectRoot 'Source\StaminaComponent.cpp') -Encoding UTF8
if ($AcceptanceProject) {
    $content = Join-Path $AcceptanceProject 'Content'
    if (-not (Test-Path -LiteralPath $content -PathType Container)) { throw 'Acceptance project Content directory missing.' }
    Copy-Item -Path (Join-Path $content '*') -Destination (Join-Path $projectRoot 'Content') -Recurse -Force
    $originalProjects = @(Get-ChildItem -LiteralPath $AcceptanceProject -Filter *.uproject -File)
    if ($originalProjects.Count -ne 1) { throw 'Acceptance requires one project descriptor.' }
    $original = Get-Content -LiteralPath $originalProjects[0].FullName -Raw | ConvertFrom-Json
    $plugins = @($original.Plugins | Where-Object { $_.Name -ne 'GameProductionAIAssistant' }) + @(@{Name='GameProductionAIAssistant';Enabled=$true})
    @{FileVersion=3;Plugins=$plugins} | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $projectRoot 'Host.uproject') -Encoding UTF8
}
$editor = Join-Path $UnrealEngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$report = Join-Path $run 'Report'
$args = @('"' + (Join-Path $projectRoot 'Host.uproject') + '"', '-unattended','-nop4','-nosplash','-nullrhi','-nosound',
    '-DDC=GPAASmokeDDC','-NoZenAutoLaunch','-ExecCmds="Automation RunTests ' + $TestFilter + '"','-TestExit="Automation Test Queue Empty"',
    '-ReportExportPath="' + $report + '"','-abslog="' + (Join-Path $run 'Unreal.log') + '"')
if ($AcceptanceProject) { $args += '-GPAARealAssets' }
if ($InteractiveAssetEditor) { $args += '-GPAAInteractiveAssetTest' }
if ($HttpShutdownAcceptance) { $args += '-GPAAHttpShutdownAcceptance' }
Write-Host "Native smoke evidence: $run"
$proc = Start-Process $editor -ArgumentList $args -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $run 'stdout.log') -RedirectStandardError (Join-Path $run 'stderr.log')
$null = $proc.Handle
try {
    if (-not $proc.WaitForExit($TimeoutSeconds*1000)) { throw "Native Unreal tests timed out. See $run" }
    if ($proc.ExitCode -ne 0) { throw "Unreal exited with $($proc.ExitCode). See $run" }
    $result = Get-Content (Join-Path $report 'index.json') -Raw | ConvertFrom-Json
    $passed = $result.succeeded + $result.succeededWithWarnings
    if ($passed -lt 1 -or $result.failed -ne 0 -or $result.notRun -ne 0 -or @($result.tests | Where-Object { $_.state -ne 'Success' -or $_.errors -gt 0 }).Count -gt 0) {
        throw "Native Unreal tests failed. See $report"
    }
    $requiredTests=@('GPAA.Native.UI.ContextualCancellation','GPAA.Native.Storage.MixedHistoryMigration','GPAA.Native.UI.SegmentedViewSelection','GPAA.Native.UI.SearchScopeRadios','GPAA.Native.Workflow.ScopedAssetSearch','GPAA.Native.UI.PromptBottomAlignment','GPAA.Native.UI.HistoryExclusiveView','GPAA.Native.Workflow.BackgroundIndexNonPreemptive','GPAA.Native.UI.SettingsCredentialActions','GPAA.Native.Cloud.StreamingBoundaries','GPAA.Native.Cloud.HttpContract.Responses','GPAA.Native.Cloud.HttpContract.ChatCompletions','GPAA.Native.Embeddings.Corpus','GPAA.Native.Storage.AtomicRollbackAndHistory','GPAA.Native.Workflow.OfflineIndexAndSearch')
    if ($HttpShutdownAcceptance) { $requiredTests=@('GPAA.Acceptance.HttpShutdown.PendingAndLateRequests') }
    foreach ($test in $requiredTests) {
        if (@($result.tests | Where-Object { $_.fullTestPath -eq $test -and $_.state -eq 'Success' }).Count -ne 1) { throw "Required native test missing: $test" }
    }
    if ($AcceptanceProject -and @($result.tests | Where-Object { $_.fullTestPath -like 'GPAA.Acceptance.ProjectAssets*' -and $_.state -eq 'Success' }).Count -ne 1) { throw 'Real project asset acceptance test missing.' }
    if ($InteractiveAssetEditor -and @($result.tests | Where-Object { $_.fullTestPath -eq 'GPAA.Acceptance.AssetEditor.Activation' -and $_.state -eq 'Success' }).Count -ne 1) { throw 'Interactive asset-editor test missing.' }
    if ($HttpShutdownAcceptance -and (Get-Content -LiteralPath (Join-Path $run 'Unreal.log') -Raw) -notmatch 'GPAA HTTP request shutdown complete\.') {throw 'Shutdown cleanup marker missing.'}
    $suiteStarted=$false
    foreach ($line in (Get-Content -LiteralPath (Join-Path $run 'Unreal.log'))) {
        if ($line -match 'Test Started\..*Path=\{GPAA\.') {$suiteStarted=$true}
        if (-not $suiteStarted -and $line -match 'LogAutomationTest: Error: Condition failed\s*$') {continue}
        if ($line -match '(Fatal error:|Assertion failed:|Log\w+: Error:)') {throw "Release-blocking error in native test log: $line"}
    }
    if (Test-Path -LiteralPath (Join-Path $plugin 'Resources\Backend\GPAA.Api.exe')) {throw 'Native package unexpectedly contains a backend executable.'}
    $after=@(Get-ChildItem -LiteralPath $plugin -Recurse -File)
    if ($after.Count -ne $pluginBefore.Count) {throw 'The installed plugin directory changed during the native smoke test.'}
    foreach ($file in $after) {
        if (-not $pluginBefore.ContainsKey($file.FullName) -or $pluginBefore[$file.FullName] -ne (Get-FileHash -LiteralPath $file.FullName).Hash) {throw "The plugin wrote into its installation: $($file.FullName)"}
    }
    @{succeeded=$true;tests=$passed;testsWithWarnings=$result.succeededWithWarnings;installationUnchanged=$true;engineInstallation=[bool]$EngineInstallation;package=(Resolve-Path -LiteralPath $PackagePath).Path;
        archiveSHA256=$(if(Test-Path -LiteralPath $PackagePath -PathType Leaf){(Get-FileHash -LiteralPath $PackagePath).Hash}else{$null})} |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'smoke-result.json') -Encoding UTF8
    Write-Host "PASS $passed native tests. Evidence: $run"
} finally { if (-not $proc.HasExited) { $proc.Kill(); $proc.WaitForExit() } }
