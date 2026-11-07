[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string] $UnrealEngineRoot,
    [switch] $RequireCleanBaseline
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'manifest.json') -Raw | ConvertFrom-Json
$archive=Join-Path $PSScriptRoot 'Plugin.zip'
if ((Get-FileHash -LiteralPath $archive).Hash -ine $manifest.sha256) { throw 'Archive hash does not match the acceptance manifest.' }
$tools=@{}
foreach ($name in @('dotnet','ollama','python')) {
    $tools[$name]=[bool](Get-Command $name -ErrorAction SilentlyContinue)
}
$evidence=Join-Path $PSScriptRoot 'acceptance-result.json'
$baseline=@{timeUtc=[DateTime]::UtcNow.ToString('o');machine=$env:COMPUTERNAME;toolsOnPath=$tools;
    archiveSHA256=$manifest.sha256;engine=$UnrealEngineRoot;passed=$false;
    cleanBaselineRequested=[bool]$RequireCleanBaseline;
    cleanMachineAttested=$false;
    note='Tool availability is recorded, not proof that software is absent. A clean-machine claim requires a known fresh environment and manual acceptance.'}
$baseline | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $evidence -Encoding UTF8
if ($RequireCleanBaseline -and ($tools.dotnet -or $tools.ollama)) { throw 'Clean baseline rejected: dotnet or ollama is available on PATH. Baseline evidence retained.' }
& (Join-Path $PSScriptRoot 'test-native-plugin.ps1') -UnrealEngineRoot $UnrealEngineRoot -PackagePath $archive -TestFilter GPAA -WorkspaceRoot $PSScriptRoot
$baseline.passed=$true
$baseline | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $evidence -Encoding UTF8
Write-Host "Automated acceptance passed. Complete the manual observations in README.md before claiming clean-machine validation. Evidence: $evidence"
