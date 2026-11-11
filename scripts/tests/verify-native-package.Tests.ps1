[CmdletBinding()]
param([string] $PackagePath)
$ErrorActionPreference='Stop'
if ([string]::IsNullOrWhiteSpace($PackagePath)) {$PackagePath=Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) '.gpaa\NativePackage'}
$verifier=Join-Path (Split-Path -Parent $PSScriptRoot) 'verify-native-package.ps1'
& $verifier -PackagePath $PackagePath
$forbidden=Join-Path $PackagePath ('test-'+[guid]::NewGuid().ToString('N')+'.exe')
try {
    [IO.File]::WriteAllText($forbidden,'disposable policy fixture, not executable software')
    $rejected=$false
    try { & $verifier -PackagePath $PackagePath } catch { if ($_.Exception.Message -like '*Forbidden native package file*') {$rejected=$true} else {throw} }
    if (-not $rejected) {throw 'Verifier accepted a forbidden executable.'}
    Write-Host 'PASS rejects executable files in the native distribution.'
} finally {if(Test-Path -LiteralPath $forbidden){Remove-Item -LiteralPath $forbidden -Force}}
$vocab=Join-Path $PackagePath 'Source\ThirdParty\MiniLM\vocab.txt'
$original=[IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $vocab).Path)
try {
    [IO.File]::AppendAllText($vocab,"`ncorrupt-policy-fixture")
    $rejected=$false
    try { & $verifier -PackagePath $PackagePath } catch { if ($_.Exception.Message -like '*Native dependency integrity failed*') {$rejected=$true} else {throw} }
    if (-not $rejected) {throw 'Verifier accepted a corrupted tokenizer vocabulary.'}
    Write-Host 'PASS rejects modified model dependency bytes.'
} finally {[IO.File]::WriteAllBytes((Resolve-Path -LiteralPath $vocab).Path,$original)}
& $verifier -PackagePath $PackagePath
