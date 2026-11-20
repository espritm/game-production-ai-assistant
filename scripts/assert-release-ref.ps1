[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $Tag,
    [Parameter(Mandatory)][string] $ExpectedCommit,
    [string] $RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($Tag -notmatch '^v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') { throw 'Release tags must use vMAJOR.MINOR.PATCH.' }
if ($ExpectedCommit -notmatch '^[a-fA-F0-9]{40}$') { throw 'An exact 40-character commit SHA is required.' }
Push-Location -LiteralPath $RepositoryRoot
try {
    $commit = git rev-parse --verify "$Tag^{commit}"
    if ($LASTEXITCODE -ne 0 -or $commit -ne $ExpectedCommit) { throw 'The tag does not resolve to the expected commit.' }
    git merge-base --is-ancestor $commit refs/remotes/origin/main
    if ($LASTEXITCODE -ne 0) { throw 'Release commit must belong to origin/main.' }
    $headCommit = git rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $headCommit -ne $commit) { throw 'Check out the exact tagged commit before building.' }
    $changes = @(git status --porcelain --untracked-files=normal)
    if ($LASTEXITCODE -ne 0 -or $changes.Count) { throw 'Release builds require a clean source checkout.' }
    $descriptor = Get-Content -LiteralPath 'plugin/GameProductionAIAssistant.uplugin' -Raw | ConvertFrom-Json
    if ($descriptor.VersionName -ne $Tag.Substring(1)) { throw 'Tag version does not match the plugin VersionName.' }
    Write-Output $commit
} finally { Pop-Location }
