[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $Tag,
    [Parameter(Mandatory)][string] $ExpectedCommit,
    [string[]] $EngineVersions = @('5.6', '5.8')
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
$null = & "$PSScriptRoot/assert-release-ref.ps1" -Tag $Tag -ExpectedCommit $ExpectedCommit -RepositoryRoot $root
$output = Join-Path $root ('.gpaa/release-output/' + $Tag)
# Never mix two invocations or silently replace previously prepared files.
if (Test-Path -LiteralPath $output) { throw "Release output already exists: $output" }
New-Item -ItemType Directory -Path $output -Force | Out-Null
foreach ($engine in $EngineVersions) {
    if ($engine -notin @('5.6', '5.8')) { throw 'Only validated Unreal versions 5.6 and 5.8 may be released.' }
    $variable = 'UE_' + $engine.Replace('.', '_') + '_ROOT'
    $engineRoot = [Environment]::GetEnvironmentVariable($variable)
    if (!$engineRoot) { $engineRoot = "C:/Program Files/Epic Games/UE_$engine" }
    & "$PSScriptRoot/package-native-plugin.ps1" -UnrealEngineRoot $engineRoot
    $package = Join-Path $root ".gpaa/releases/GameProductionAIAssistant-native-$Tag-UE$engine-Win64.zip"
    & "$PSScriptRoot/test-native-plugin.ps1" -UnrealEngineRoot $engineRoot -PackagePath $package -TestFilter GPAA
    & "$PSScriptRoot/test-native-plugin.ps1" -UnrealEngineRoot $engineRoot -PackagePath $package -HttpShutdownAcceptance
    & "$PSScriptRoot/test-automatic-index.ps1" -UnrealEngineRoot $engineRoot -PackagePath $package
    & "$PSScriptRoot/tests/verify-native-package.Tests.ps1"
    foreach ($variant in @('Win64', 'FabSource')) {
        Copy-Item -LiteralPath (Join-Path $root ".gpaa/releases/GameProductionAIAssistant-native-$Tag-UE$engine-$variant.zip") -Destination $output
    }
}
$dossier = & "$PSScriptRoot/new-fab-submission-kit.ps1" -Version $Tag.Substring(1) -EngineVersions $EngineVersions -PassThru
Copy-Item -LiteralPath $dossier -Destination (Join-Path $output "GameProductionAIAssistant-FabDossier-$Tag.zip")
$files = @(Get-ChildItem -LiteralPath $output -File | Sort-Object Name | ForEach-Object {
    @{file=$_.Name; bytes=$_.Length; sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
})
@{version=$Tag.Substring(1); commit=$ExpectedCommit; engineVersions=$EngineVersions; files=$files} |
    ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $output 'SHA256SUMS.json') -Encoding utf8
Write-Output "Verified release files: $output"
