[CmdletBinding()]
param(
    [string] $UnrealEngineRoot = "C:\Program Files\Epic Games\UE_5.6",
    [string] $UnrealVersion = "5.6",
    [string] $Configuration = "Release",
    [string] $Runtime = "win-x64",
    [string] $ExpectedVersion,
    [string] $ArchiveOutputDirectory,
    [switch] $FrameworkDependent
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Assert-WorkspaceOutput {
    param([string] $Path)
    $fullPath = [IO.Path]::GetFullPath($Path).TrimEnd('\')
    $workspace = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot)).TrimEnd('\')
    if (-not $fullPath.StartsWith($workspace + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Output must be inside the repository: $fullPath"
    }
    $ancestor = $fullPath
    while ($ancestor.Length -gt $workspace.Length) {
        if ((Test-Path -LiteralPath $ancestor) -and
            ((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Output traverses a reparse point: $ancestor"
        }
        $ancestor = Split-Path -Parent $ancestor
    }
}

function Remove-DirectoryContentsExcept {
    param(
        [Parameter(Mandatory = $true)][string] $Path,
        [string[]] $Except = @()
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Force -Path $Path | Out-Null
        return
    }

    Assert-WorkspaceOutput $Path
    Get-ChildItem -LiteralPath $Path -Force |
        Where-Object { $Except -notcontains $_.Name } |
        Remove-Item -Recurse -Force
}

function New-DeterministicZip {
    param(
        [Parameter(Mandatory = $true)][string] $SourceDirectory,
        [Parameter(Mandatory = $true)][string] $DestinationPath
    )

    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    if (Test-Path -LiteralPath $DestinationPath) {
        Remove-Item -LiteralPath $DestinationPath -Force
    }

    $sourceRoot = (Resolve-Path -LiteralPath $SourceDirectory).Path.TrimEnd('\', '/')
    $archive = [System.IO.Compression.ZipFile]::Open(
        $DestinationPath,
        [System.IO.Compression.ZipArchiveMode]::Create
    )
    $stableTimestamp = [System.DateTimeOffset]::new(2000, 1, 1, 0, 0, 0, [System.TimeSpan]::Zero)

    try {
        $files = Get-ChildItem -LiteralPath $sourceRoot -Recurse -File -Force |
            Sort-Object { $_.FullName.Substring($sourceRoot.Length + 1) }

        foreach ($file in $files) {
            $relativePath = $file.FullName.Substring($sourceRoot.Length + 1) -replace '\\', '/'
            $entry = $archive.CreateEntry($relativePath, [System.IO.Compression.CompressionLevel]::Optimal)
            $entry.LastWriteTime = $stableTimestamp
            $inputStream = $file.OpenRead()
            $outputStream = $entry.Open()
            try {
                $inputStream.CopyTo($outputStream)
            }
            finally {
                $outputStream.Dispose()
                $inputStream.Dispose()
            }
        }
    }
    finally {
        $archive.Dispose()
    }
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$backendProject = Join-Path $repositoryRoot "GPAA.Api\GPAA.Api.csproj"
$backendOutput = Join-Path $repositoryRoot "plugin\Resources\Backend"
$packageOutput = Join-Path $repositoryRoot ".gpaa\PluginPackage"
$runUat = Join-Path $UnrealEngineRoot "Engine\Build\BatchFiles\RunUAT.bat"
$pluginDescriptor = Join-Path $repositoryRoot "plugin\GameProductionAIAssistant.uplugin"
$packageVerifier = Join-Path $PSScriptRoot "verify-plugin-package.ps1"
Assert-WorkspaceOutput $backendOutput
Assert-WorkspaceOutput $packageOutput
if ($Runtime -ne 'win-x64') { throw 'The Win64 Unreal package requires runtime win-x64.' }
$engineVersion = Get-Content -LiteralPath (Join-Path $UnrealEngineRoot 'Engine\Build\Build.version') -Raw | ConvertFrom-Json
if ($UnrealVersion -ne "$($engineVersion.MajorVersion).$($engineVersion.MinorVersion)") {
    throw 'UnrealVersion does not match the selected engine installation.'
}

foreach ($requiredPath in @($backendProject, $runUat, $pluginDescriptor, $packageVerifier)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required file not found: '$requiredPath'."
    }
}

try {
    $descriptor = Get-Content -LiteralPath $pluginDescriptor -Raw | ConvertFrom-Json
}
catch {
    throw "Plugin descriptor is not valid JSON: $($_.Exception.Message)"
}

$releaseVersion = if ([string]::IsNullOrWhiteSpace($ExpectedVersion)) {
    [string] $descriptor.VersionName
}
else {
    $ExpectedVersion.Trim()
}
if ($releaseVersion.StartsWith('v')) {
    $releaseVersion = $releaseVersion.Substring(1)
}
if ($releaseVersion -notmatch '^[0-9A-Za-z][0-9A-Za-z.-]*$') {
    throw "Version '$releaseVersion' cannot be used in a release artifact name."
}
if ($releaseVersion -ne [string] $descriptor.VersionName) { throw 'ExpectedVersion does not match the plugin descriptor.' }
if ([string]::IsNullOrWhiteSpace($ArchiveOutputDirectory)) {
    $ArchiveOutputDirectory = Join-Path $repositoryRoot ".gpaa\releases"
}

Remove-DirectoryContentsExcept -Path $backendOutput -Except @('.gitkeep')
if (Test-Path -LiteralPath $packageOutput) {
    Remove-Item -LiteralPath $packageOutput -Recurse -Force
}

$publishArgs = @(
    'publish',
    $backendProject,
    '-c',
    $Configuration,
    '-r',
    $Runtime,
    '-o',
    $backendOutput,
    '--self-contained',
    $(if ($FrameworkDependent) { 'false' } else { 'true' })
)

Write-Host "Publishing backend ($Runtime, self-contained: $(-not $FrameworkDependent))..."
dotnet @publishArgs
if ($LASTEXITCODE -ne 0) {
    throw "dotnet publish failed with exit code $LASTEXITCODE."
}

Write-Host "Building Unreal plugin with UE $UnrealVersion..."
& $runUat BuildPlugin `
    -Plugin="$pluginDescriptor" `
    -Package="$packageOutput" `
    -TargetPlatforms=Win64 `
    -StrictIncludes `
    -utf8output
if ($LASTEXITCODE -ne 0) {
    throw "BuildPlugin failed with exit code $LASTEXITCODE."
}

Write-Host "Removing development-only build output..."
foreach ($directoryName in @('Intermediate', 'Saved')) {
    $directoryPath = Join-Path $packageOutput $directoryName
    if (Test-Path -LiteralPath $directoryPath) {
        Remove-Item -LiteralPath $directoryPath -Recurse -Force
    }
}
Get-ChildItem -LiteralPath $packageOutput -Recurse -File -Force |
    Where-Object {
        $_.Name -eq '.gitkeep' -or
        $_.Name -like '*.pdb' -or
        $_.Name -like '*.log' -or
        $_.Name -ieq 'appsettings.Development.json'
    } |
    Remove-Item -Force

Copy-Item -LiteralPath (Join-Path $repositoryRoot 'LICENSE') -Destination (Join-Path $packageOutput 'LICENSE') -Force
$packageDocs = Join-Path $packageOutput 'Docs'
New-Item -ItemType Directory -Force -Path $packageDocs | Out-Null
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'docs\unreal-plugin.md') -Destination (Join-Path $packageDocs 'UnrealPlugin.md') -Force

$verifyParams = @{
    PackagePath = $packageOutput
    ExpectedVersion = $releaseVersion
}
if ($FrameworkDependent) {
    $verifyParams.AllowFrameworkDependentBackend = $true
}

Write-Host "Verifying sanitized package..."
& $packageVerifier @verifyParams

New-Item -ItemType Directory -Force -Path $ArchiveOutputDirectory | Out-Null
$dependencySuffix = if ($FrameworkDependent) { '-framework-dependent' } else { '' }
$archiveName = "GameProductionAIAssistant-v$releaseVersion-UE$UnrealVersion-Win64$dependencySuffix.zip"
$archivePath = Join-Path $ArchiveOutputDirectory $archiveName

Write-Host "Creating deterministic release archive..."
New-DeterministicZip -SourceDirectory $packageOutput -DestinationPath $archivePath

Write-Host "Verifying final archive..."
$verifyParams.PackagePath = $archivePath
& $packageVerifier @verifyParams

$archiveHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Host "Plugin package directory: $packageOutput"
Write-Host "Release archive: $archivePath"
Write-Host "SHA256: $archiveHash"
