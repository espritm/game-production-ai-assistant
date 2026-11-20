$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$fixture = Join-Path $root ('.gpaa/publish-release-tests/' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path (Join-Path $fixture 'scripts'),(Join-Path $fixture 'plugin'),(Join-Path $fixture 'docs') | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'scripts/assert-release-ref.ps1'),(Join-Path $root 'scripts/publish-release.ps1') -Destination (Join-Path $fixture 'scripts')
'{"VersionName":"1.2.3"}' | Set-Content (Join-Path $fixture 'plugin/GameProductionAIAssistant.uplugin')
'.gpaa/' | Set-Content (Join-Path $fixture '.gitignore')
'Test release notes' | Set-Content (Join-Path $fixture 'docs/native-release-notes.md')
git -C $fixture init -q -b main
git -C $fixture config user.name GPAA-Test
git -C $fixture config user.email gpaa-tests@example.invalid
git -C $fixture config commit.gpgsign false
git -C $fixture config tag.gpgsign false
git -C $fixture add .
git -C $fixture commit -qm initial
$global:GPAAPublishTestFixtureCommit = git -C $fixture rev-parse HEAD
git -C $fixture update-ref refs/remotes/origin/main $global:GPAAPublishTestFixtureCommit
git -C $fixture tag v1.2.3
$out = Join-Path $fixture '.gpaa/release-output/v1.2.3'
New-Item -ItemType Directory -Force -Path $out | Out-Null
1..5 | ForEach-Object { 'Test payload' | Set-Content (Join-Path $out "test-$_.zip") }
$files = @(Get-ChildItem -LiteralPath $out -File | ForEach-Object { @{file=$_.Name;bytes=$_.Length;sha256=(Get-FileHash $_.FullName).Hash} })
@{commit=$global:GPAAPublishTestFixtureCommit;version='1.2.3';files=$files} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $out 'SHA256SUMS.json')
$global:GPAAPublishTestCalls = @()
$global:GPAAPublishTestPublished = $false
$global:GPAAPublishTestUploadFailure = $false
$global:GPAAPublishTestRemoteMoved = $false
$global:GPAAPublishTestMissing = $true
function global:gh {
    $call = @($args)
    $global:GPAAPublishTestCalls += ($call -join ' ')
    $global:LASTEXITCODE = 0
    if ($call[0] -eq 'api' -and $call[1] -like '*/commits/*') {
        if ($global:GPAAPublishTestRemoteMoved) { return ('0' * 40) }
        return $global:GPAAPublishTestFixtureCommit
    }
    if ($call[0] -eq 'api') {
        if ($global:GPAAPublishTestMissing) { $global:LASTEXITCODE = 1; return }
        return ('{"draft":' + (!$global:GPAAPublishTestPublished).ToString().ToLower() + '}')
    }
    if ($call[1] -eq 'create') { $global:GPAAPublishTestMissing = $false }
    if ($call[1] -eq 'upload' -and $global:GPAAPublishTestUploadFailure) { $global:LASTEXITCODE = 1 }
}
function Invoke-Publisher {
    & (Join-Path $fixture 'scripts/publish-release.ps1') -Tag v1.2.3 -ExpectedCommit $global:GPAAPublishTestFixtureCommit -Repository example/fixture
}
function Assert-PublishRejected {
    $global:GPAAPublishTestCalls = @()
    $rejected = $false
    try { Invoke-Publisher } catch { $rejected = $true }
    if (!$rejected) { throw 'Unsafe release publication was accepted.' }
    if (@($global:GPAAPublishTestCalls | Where-Object { $_ -like 'release edit*' }).Count) { throw 'A failed publication must never expose a release.' }
}
try {
    Invoke-Publisher
    if (@($global:GPAAPublishTestCalls | Where-Object { $_ -like 'release create*' }).Count -ne 1) { throw 'First publication did not create a draft.' }
    if (@($global:GPAAPublishTestCalls | Where-Object { $_ -like 'release edit*' }).Count -ne 1) { throw 'Complete release was not published.' }
    $global:GPAAPublishTestPublished = $true
    Assert-PublishRejected
    $global:GPAAPublishTestPublished = $false; $global:GPAAPublishTestUploadFailure = $true
    Assert-PublishRejected
    $global:GPAAPublishTestUploadFailure = $false; $global:GPAAPublishTestRemoteMoved = $true
    Assert-PublishRejected
    $global:GPAAPublishTestRemoteMoved = $false
    'Tampered' | Add-Content (Join-Path $out 'test-1.zip')
    Assert-PublishRejected
} finally {
    Remove-Item Function:\gh -ErrorAction SilentlyContinue
    Remove-Variable GPAAPublishTestFixtureCommit,GPAAPublishTestCalls,GPAAPublishTestPublished,GPAAPublishTestUploadFailure,GPAAPublishTestRemoteMoved -Scope Global -ErrorAction SilentlyContinue
    Remove-Variable GPAAPublishTestMissing -Scope Global -ErrorAction SilentlyContinue
}
$global:LASTEXITCODE = 0
Write-Host 'PASS mocked release publication: complete upload, immutable published release, upload failure, moved tag and tampered payload.'
