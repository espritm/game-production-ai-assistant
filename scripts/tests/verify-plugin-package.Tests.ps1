$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$verifier = Join-Path $repositoryRoot "scripts\verify-plugin-package.ps1"
$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) "gpaa-package-verifier-tests-$([guid]::NewGuid().ToString('N'))"

function New-ValidPackageFixture {
    param([Parameter(Mandatory = $true)][string] $Root)

    $requiredFiles = @(
        'Binaries\Win64\UnrealEditor-GPAAssistant.dll',
        'Binaries\Win64\UnrealEditor.modules',
        'Source\GPAAssistant\GPAAssistant.Build.cs',
        'Resources\Backend\GPAA.Api.exe',
        'Resources\Backend\GPAA.Api.deps.json',
        'Resources\Backend\GPAA.Api.runtimeconfig.json',
        'Resources\Backend\coreclr.dll',
        'Resources\Backend\hostfxr.dll',
        'Resources\Backend\hostpolicy.dll'
    )

    New-Item -ItemType Directory -Force -Path $Root | Out-Null
    foreach ($relativePath in $requiredFiles) {
        $path = Join-Path $Root $relativePath
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $path) | Out-Null
        New-Item -ItemType File -Force -Path $path | Out-Null
    }

    @{
        FileVersion = 3
        Version = 1
        VersionName = '0.1.0'
        Modules = @(@{ Name = 'GPAAssistant'; Type = 'Editor'; LoadingPhase = 'Default' })
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Root 'GameProductionAIAssistant.uplugin') -Encoding UTF8
}

function Invoke-Verifier {
    param(
        [Parameter(Mandatory = $true)][string] $Path,
        [string] $ExpectedVersion
    )

    $arguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $verifier, '-PackagePath', $Path)
    if (-not [string]::IsNullOrWhiteSpace($ExpectedVersion)) {
        $arguments += @('-ExpectedVersion', $ExpectedVersion)
    }

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & powershell.exe @arguments 2>&1
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    return [pscustomobject]@{ ExitCode = $exitCode; Output = ($output -join [Environment]::NewLine) }
}

function Assert-Succeeds {
    param([Parameter(Mandatory = $true)][string] $Name, [Parameter(Mandatory = $true)] $Result)
    if ($Result.ExitCode -ne 0) {
        throw "FAIL $Name`n$($Result.Output)"
    }
    Write-Host "PASS $Name"
}

function Assert-FailsWith {
    param(
        [Parameter(Mandatory = $true)][string] $Name,
        [Parameter(Mandatory = $true)] $Result,
        [Parameter(Mandatory = $true)][string] $Message
    )
    if ($Result.ExitCode -eq 0 -or $Result.Output -notlike "*$Message*") {
        throw "FAIL $Name`nExpected failure containing '$Message'.`n$($Result.Output)"
    }
    Write-Host "PASS $Name"
}

try {
    $valid = Join-Path $testRoot 'valid'
    New-ValidPackageFixture $valid
    Assert-Succeeds 'accepts a valid release directory' (Invoke-Verifier $valid 'v0.1.0')

    $wrongVersion = Join-Path $testRoot 'wrong-version'
    Copy-Item -LiteralPath $valid -Destination $wrongVersion -Recurse
    Assert-FailsWith 'rejects a mismatched release version' (Invoke-Verifier $wrongVersion 'v0.2.0') 'Version mismatch'

    $forbidden = Join-Path $testRoot 'forbidden'
    Copy-Item -LiteralPath $valid -Destination $forbidden -Recurse
    New-Item -ItemType Directory -Force -Path (Join-Path $forbidden 'Intermediate') | Out-Null
    New-Item -ItemType File -Force -Path (Join-Path $forbidden 'Intermediate\build.obj') | Out-Null
    Assert-FailsWith 'rejects forbidden build output' (Invoke-Verifier $forbidden) 'Unexpected top-level directory'

    $missingBackend = Join-Path $testRoot 'missing-backend'
    Copy-Item -LiteralPath $valid -Destination $missingBackend -Recurse
    Remove-Item -LiteralPath (Join-Path $missingBackend 'Resources\Backend\coreclr.dll') -Force
    Assert-FailsWith 'rejects an incomplete self-contained backend' (Invoke-Verifier $missingBackend) 'coreclr.dll'

    $zipPath = Join-Path $testRoot 'valid.zip'
    Compress-Archive -Path (Join-Path $valid '*') -DestinationPath $zipPath
    Assert-Succeeds 'accepts a valid release zip' (Invoke-Verifier $zipPath '0.1.0')

    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $unsafeZip = Join-Path $testRoot 'unsafe.zip'
    $archive = [System.IO.Compression.ZipFile]::Open($unsafeZip, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        [void] $archive.CreateEntry('../outside.txt')
    }
    finally {
        $archive.Dispose()
    }
    Assert-FailsWith 'rejects unsafe archive paths' (Invoke-Verifier $unsafeZip) 'Unsafe path'

    foreach ($unsafePath in @('/outside.txt', 'C:/outside.txt', 'Resources/../../outside.txt', '../outside/', 'Resources/file:stream', 'Resources/..')) {
        $unsafeZip = Join-Path $testRoot ([guid]::NewGuid().ToString('N') + '.zip')
        $archive = [System.IO.Compression.ZipFile]::Open($unsafeZip, [System.IO.Compression.ZipArchiveMode]::Create)
        try { [void] $archive.CreateEntry($unsafePath) } finally { $archive.Dispose() }
        Assert-FailsWith "rejects rooted or escaping path $unsafePath" (Invoke-Verifier $unsafeZip) 'Unsafe path'
    }
}
finally {
    if (Test-Path -LiteralPath $testRoot) {
        $resolvedTestRoot = (Resolve-Path -LiteralPath $testRoot).Path
        $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
        if (-not $resolvedTestRoot.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe test cleanup path.' }
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
}
