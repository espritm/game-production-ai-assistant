param(
    [Parameter(Mandatory = $true)]
    [string] $UnrealProjectRoot,

    [switch] $UsePrecompiledPackage
)

$ErrorActionPreference = "Stop"

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$sourcePlugin = if ($UsePrecompiledPackage) {
    Join-Path $repositoryRoot ".gpaa\PluginPackage"
} else {
    Join-Path $repositoryRoot "plugin"
}
$targetPluginsRoot = Join-Path $UnrealProjectRoot "Plugins"
$targetPlugin = Join-Path $targetPluginsRoot "GameProductionAIAssistant"

if (-not (Test-Path $sourcePlugin)) {
    throw "Source plugin directory not found: $sourcePlugin"
}

if (-not (Test-Path $UnrealProjectRoot)) {
    throw "Unreal project root not found: $UnrealProjectRoot"
}

New-Item -ItemType Directory -Force -Path $targetPluginsRoot | Out-Null

if (Test-Path $targetPlugin) {
    Remove-Item -LiteralPath $targetPlugin -Recurse -Force
}

Copy-Item -LiteralPath $sourcePlugin -Destination $targetPlugin -Recurse

Write-Host "Plugin installed to $targetPlugin"
