param(
    [string] $UnrealEngineRoot = "C:\Program Files\Epic Games\UE_5.6",
    [string] $Configuration = "Release",
    [string] $Runtime = "win-x64",
    [switch] $FrameworkDependent
)

$ErrorActionPreference = "Stop"

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$backendProject = Join-Path $repositoryRoot "GPAA.Api\GPAA.Api.csproj"
$backendOutput = Join-Path $repositoryRoot "plugin\Resources\Backend"
$packageOutput = Join-Path $repositoryRoot ".gpaa\PluginPackage"
$runUat = Join-Path $UnrealEngineRoot "Engine\Build\BatchFiles\RunUAT.bat"
$pluginDescriptor = Join-Path $repositoryRoot "plugin\GameProductionAIAssistant.uplugin"

if (-not (Test-Path -LiteralPath $backendProject)) {
    throw "Backend project not found: $backendProject"
}

if (-not (Test-Path -LiteralPath $runUat)) {
    throw "RunUAT not found: $runUat"
}

if (Test-Path -LiteralPath $backendOutput) {
    Get-ChildItem -LiteralPath $backendOutput -Force |
        Where-Object { $_.Name -ne ".gitkeep" } |
        Remove-Item -Recurse -Force
}
else {
    New-Item -ItemType Directory -Force -Path $backendOutput | Out-Null
}

$publishArgs = @(
    "publish",
    $backendProject,
    "-c",
    $Configuration,
    "-r",
    $Runtime,
    "-o",
    $backendOutput
)

if ($FrameworkDependent) {
    $publishArgs += "--self-contained"
    $publishArgs += "false"
}
else {
    $publishArgs += "--self-contained"
    $publishArgs += "true"
}

dotnet @publishArgs

& $runUat BuildPlugin `
    -Plugin="$pluginDescriptor" `
    -Package="$packageOutput" `
    -TargetPlatforms=Win64 `
    -StrictIncludes

Write-Host "Plugin package created at $packageOutput"
