[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $PackagePath,

    [string] $ExpectedVersion,

    [switch] $AllowFrameworkDependentBackend
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Normalize-PackagePath {
    param([Parameter(Mandatory = $true)][string] $Path)

    return ($Path -replace '\\', '/').TrimStart('/')
}

function Test-UnsafeArchivePath {
    param([Parameter(Mandatory = $true)][string] $Path)

    if ($Path.StartsWith('/') -or $Path.StartsWith('\')) { return $true }
    $normalized = (Normalize-PackagePath $Path).TrimEnd('/')
    if ([string]::IsNullOrWhiteSpace($normalized)) { return $true }

    return $normalized.Contains(':') -or
        @($normalized.Split('/') | Where-Object { $_ -in @('..', '.', '') }).Count -gt 0 -or
        [System.IO.Path]::IsPathRooted($normalized) -or
        $normalized -match '^[A-Za-z]:'
}

function Get-DirectoryPackage {
    param([Parameter(Mandatory = $true)][string] $Root)

    $resolvedRoot = (Resolve-Path -LiteralPath $Root).Path.TrimEnd('\', '/')
    $files = Get-ChildItem -LiteralPath $resolvedRoot -Recurse -File -Force | ForEach-Object {
        Normalize-PackagePath $_.FullName.Substring($resolvedRoot.Length + 1)
    }

    $readText = {
        param([string] $RelativePath)
        Get-Content -LiteralPath (Join-Path $resolvedRoot ($RelativePath -replace '/', '\')) -Raw
    }.GetNewClosure()

    return [pscustomobject]@{
        Files = @($files)
        ReadText = $readText
        Dispose = { }
    }
}

function Get-ZipPackage {
    param([Parameter(Mandatory = $true)][string] $ZipPath)

    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $resolvedZip = (Resolve-Path -LiteralPath $ZipPath).Path
    $archive = [System.IO.Compression.ZipFile]::OpenRead($resolvedZip)

    try {
        foreach ($entry in $archive.Entries) {
            if (Test-UnsafeArchivePath $entry.FullName) {
                throw "Unsafe path found in archive: '$($entry.FullName)'."
            }
        }

        $entries = @($archive.Entries | Where-Object { -not [string]::IsNullOrEmpty($_.Name) })

        $entryMap = @{}
        foreach ($entry in $entries) {
            $normalized = Normalize-PackagePath $entry.FullName
            if ($entryMap.ContainsKey($normalized)) {
                throw "Duplicate archive entry: '$normalized'."
            }
            $entryMap[$normalized] = $entry
        }

        $readText = {
            param([string] $RelativePath)
            $entry = $entryMap[$RelativePath]
            $stream = $entry.Open()
            $reader = [System.IO.StreamReader]::new($stream)
            try {
                $reader.ReadToEnd()
            }
            finally {
                $reader.Dispose()
                $stream.Dispose()
            }
        }.GetNewClosure()
        $dispose = { $archive.Dispose() }.GetNewClosure()

        return [pscustomobject]@{
            Files = @($entryMap.Keys)
            ReadText = $readText
            Dispose = $dispose
        }
    }
    catch {
        $archive.Dispose()
        throw
    }
}

function Assert-PackageFile {
    param(
        [Parameter(Mandatory = $true)][hashtable] $FileSet,
        [Parameter(Mandatory = $true)][string] $Path
    )

    if (-not $FileSet.ContainsKey($Path)) {
        throw "Required package file is missing: '$Path'."
    }
}

if (-not (Test-Path -LiteralPath $PackagePath)) {
    throw "Package path not found: '$PackagePath'."
}

$resolvedPackage = (Resolve-Path -LiteralPath $PackagePath).Path
$package = if (Test-Path -LiteralPath $resolvedPackage -PathType Container) {
    Get-DirectoryPackage $resolvedPackage
}
elseif ([System.IO.Path]::GetExtension($resolvedPackage) -ieq '.zip') {
    Get-ZipPackage $resolvedPackage
}
else {
    throw "PackagePath must be a plugin directory or a .zip archive: '$resolvedPackage'."
}

try {
    $fileSet = @{}
    foreach ($file in $package.Files) {
        $normalized = Normalize-PackagePath $file
        $fileSet[$normalized] = $true
    }

    $allowedTopLevelDirectories = @('Binaries', 'Config', 'Content', 'Docs', 'Resources', 'Source')
    $allowedTopLevelFiles = @('GameProductionAIAssistant.uplugin', 'LICENSE', 'LICENSE.md', 'README.md', 'README.txt')

    foreach ($file in $fileSet.Keys) {
        $segments = $file.Split('/')
        $topLevel = $segments[0]
        if ($segments.Count -eq 1) {
            if ($allowedTopLevelFiles -notcontains $topLevel) {
                throw "Unexpected top-level file in package: '$file'."
            }
        }
        elseif ($allowedTopLevelDirectories -notcontains $topLevel) {
            throw "Unexpected top-level directory in package: '$topLevel'."
        }

        if ($segments | Where-Object { $_ -in @('Intermediate', 'Saved', '.gpaa', '.git', '.vs', '.vscode', 'obj') }) {
            throw "Forbidden directory found in package: '$file'."
        }

        $fileName = $segments[-1]
        if ($fileName -eq '.gitkeep' -or
            $fileName -like '*.pdb' -or
            $fileName -like '*.log' -or
            $fileName -like '*.user' -or
            $fileName -like '*.suo' -or
            $fileName -ieq 'appsettings.Development.json') {
            throw "Forbidden development file found in package: '$file'."
        }
    }

    $requiredFiles = @(
        'GameProductionAIAssistant.uplugin',
        'Binaries/Win64/UnrealEditor-GPAAssistant.dll',
        'Binaries/Win64/UnrealEditor.modules',
        'Source/GPAAssistant/GPAAssistant.Build.cs',
        'Resources/Backend/GPAA.Api.exe',
        'Resources/Backend/GPAA.Api.deps.json',
        'Resources/Backend/GPAA.Api.runtimeconfig.json'
    )

    if (-not $AllowFrameworkDependentBackend) {
        $requiredFiles += @(
            'Resources/Backend/coreclr.dll',
            'Resources/Backend/hostfxr.dll',
            'Resources/Backend/hostpolicy.dll'
        )
    }

    foreach ($requiredFile in $requiredFiles) {
        Assert-PackageFile -FileSet $fileSet -Path $requiredFile
    }

    try {
        $descriptor = & $package.ReadText 'GameProductionAIAssistant.uplugin' | ConvertFrom-Json
    }
    catch {
        throw "Plugin descriptor is not valid JSON: $($_.Exception.Message)"
    }

    if ($descriptor.FileVersion -ne 3) {
        throw "Plugin descriptor FileVersion must be 3, found '$($descriptor.FileVersion)'."
    }
    if ([string]::IsNullOrWhiteSpace([string] $descriptor.VersionName)) {
        throw "Plugin descriptor VersionName is missing."
    }
    if ($descriptor.Modules.Count -eq 0 -or $descriptor.Modules.Name -notcontains 'GPAAssistant') {
        throw "Plugin descriptor does not declare the GPAAssistant module."
    }

    if (-not [string]::IsNullOrWhiteSpace($ExpectedVersion)) {
        $normalizedExpectedVersion = $ExpectedVersion.Trim()
        if ($normalizedExpectedVersion.StartsWith('v')) {
            $normalizedExpectedVersion = $normalizedExpectedVersion.Substring(1)
        }
        if ([string] $descriptor.VersionName -ne $normalizedExpectedVersion) {
            throw "Version mismatch: expected '$normalizedExpectedVersion', descriptor contains '$($descriptor.VersionName)'."
        }
    }

    Write-Host "Plugin package verification succeeded."
    Write-Host "Path: $resolvedPackage"
    Write-Host "Version: $($descriptor.VersionName)"
    Write-Host "Files: $($fileSet.Count)"
}
finally {
    & $package.Dispose
}
