[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $Tag,
    [Parameter(Mandatory)][string] $ExpectedCommit,
    [string] $Repository = 'espritm/game-production-ai-assistant',
    [string] $RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = $RepositoryRoot
$null = & "$root/scripts/assert-release-ref.ps1" -Tag $Tag -ExpectedCommit $ExpectedCommit -RepositoryRoot $root
$directory = Join-Path $root ('.gpaa/release-output/' + $Tag)
$manifest = Get-Content -LiteralPath (Join-Path $directory 'SHA256SUMS.json') -Raw | ConvertFrom-Json
if ($manifest.commit -ne $ExpectedCommit -or $manifest.version -ne $Tag.Substring(1)) { throw 'Release manifest does not match the requested commit/version.' }
$assets = @((Join-Path $directory 'SHA256SUMS.json'))
foreach ($entry in $manifest.files) {
    if ([IO.Path]::GetFileName($entry.file) -ne $entry.file) { throw 'Manifest filenames must not contain paths.' }
    $path = Join-Path $directory $entry.file
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $entry.sha256 -or (Get-Item -LiteralPath $path).Length -ne $entry.bytes) { throw "Release file changed: $($entry.file)" }
    $assets += $path
}
if ($assets.Count -ne 6) { throw 'Expected four engine ZIPs, one Fab dossier and the hash manifest.' }
$remoteCommit = gh api "repos/$Repository/commits/$Tag" --jq .sha
if ($LASTEXITCODE -ne 0 -or $remoteCommit -ne $ExpectedCommit) { throw 'Remote tag changed or could not be verified; nothing was published.' }
# Windows PowerShell 5 promotes native stderr to a terminating error under Stop.
# A missing release is expected; capture it locally and reject every other failure.
$previousErrorPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = 'Continue'
    $response = @(gh api "repos/$Repository/releases/tags/$Tag" 2>&1)
    $lookupExitCode = $LASTEXITCODE
} finally {
    $ErrorActionPreference = $previousErrorPreference
}
if ($lookupExitCode -eq 0) {
    $existing = ($response -join "`n") | ConvertFrom-Json
    if (!$existing.draft) { throw 'Published releases are immutable; create a new version instead.' }
} elseif (($response -join "`n") -match 'HTTP 404') {
    $notes = Join-Path $root 'docs/native-release-notes.md'
    gh release create $Tag --repo $Repository --verify-tag --draft --title "Game Production AI Assistant $Tag" --notes-file $notes
    if ($LASTEXITCODE -ne 0) { throw 'Could not create draft release.' }
} else {
    throw 'Could not look up release; refusing to create or publish it.'
}
gh release upload $Tag @assets --repo $Repository --clobber
if ($LASTEXITCODE -ne 0) { throw 'Asset upload failed; release stays in draft.' }
gh release edit $Tag --repo $Repository --draft=false
if ($LASTEXITCODE -ne 0) { throw 'Could not publish the complete release.' }
