$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$guard = Join-Path $root 'scripts/assert-release-ref.ps1'
$fixture = Join-Path $root ('.gpaa/release-ref-tests/' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $fixture 'plugin') -Force | Out-Null
git -C $fixture init -q -b main
git -C $fixture config user.name GPAA-Test
git -C $fixture config user.email gpaa-tests@example.invalid
git -C $fixture config commit.gpgsign false
git -C $fixture config tag.gpgsign false
'{"VersionName":"1.2.3"}' | Set-Content (Join-Path $fixture 'plugin/GameProductionAIAssistant.uplugin')
'.gpaa/' | Set-Content (Join-Path $fixture '.gitignore')
git -C $fixture add .
git -C $fixture commit -qm initial
$commit = git -C $fixture rev-parse HEAD
git -C $fixture update-ref refs/remotes/origin/main $commit
git -C $fixture tag -a v1.2.3 -m "Release guard fixture"
$null = & $guard -Tag v1.2.3 -ExpectedCommit $commit -RepositoryRoot $fixture
function Assert-Rejected([string] $Tag, [string] $Sha) {
    $rejected = $false
    try { $null = & $guard -Tag $Tag -ExpectedCommit $Sha -RepositoryRoot $fixture } catch { $rejected = $true }
    if (!$rejected) { throw "Release guard incorrectly accepted $Tag / $Sha" }
}
git -C $fixture tag v9.9.9
Assert-Rejected v9.9.9 $commit
Add-Content (Join-Path $fixture 'plugin/GameProductionAIAssistant.uplugin') ' '
Assert-Rejected v1.2.3 $commit
git -C $fixture checkout -- plugin/GameProductionAIAssistant.uplugin
Assert-Rejected 'v1.2.3;unsafe' $commit
Assert-Rejected v1.2.3 ('0' * 40)
git -C $fixture checkout -qb unmerged
'{"VersionName":"1.2.4"}' | Set-Content (Join-Path $fixture 'plugin/GameProductionAIAssistant.uplugin')
git -C $fixture add .
git -C $fixture commit -qm unmerged
$offMain = git -C $fixture rev-parse HEAD
git -C $fixture tag v1.2.4
Assert-Rejected v1.2.4 $offMain
git -C $fixture checkout -q main
Write-Host 'PASS release guards: valid tag, version mismatch, invalid input, SHA mismatch, and off-main commit.'
