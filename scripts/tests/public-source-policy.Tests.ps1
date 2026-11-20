$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$tracked = @(git -C $root ls-files)
if ($LASTEXITCODE -ne 0) { throw 'Could not enumerate tracked source files.' }
foreach ($name in $tracked) {
    if ($name -match '(?i)\.(dll|exe|pdb|lib|onnx|gguf|sqlite|db|pem|pfx)$' -or
        $name -match '(^|/)(\.env(?:\..*)?|\.credentials.*|\.runner)$') { throw "Private/generated file tracked: $name" }
    if ($name -like '.github/workflows/*') {
        $workflow = Get-Content -LiteralPath (Join-Path $root $name) -Raw
        if ($workflow -match 'self-hosted|pull_request_target') { throw "Public workflows must not use personal runners or privileged PR triggers: $name" }
    }
}
if (!(Test-Path -LiteralPath (Join-Path $root 'LICENSE'))) { throw 'Missing project license.' }
Write-Host 'PASS public source distribution and hosted-only CI policy.'
