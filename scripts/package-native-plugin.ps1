[CmdletBinding()]
param([string] $UnrealEngineRoot='C:\Program Files\Epic Games\UE_5.6')
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'prepare-native-dependencies.ps1')
$build=Get-Content -LiteralPath (Join-Path $UnrealEngineRoot 'Engine\Build\Build.version') -Raw | ConvertFrom-Json
$engine="$($build.MajorVersion).$($build.MinorVersion)"
$stage=Join-Path $root ('.gpaa\native-build\'+[guid]::NewGuid().ToString('N')+'\GameProductionAIAssistant')
$output=Join-Path $root '.gpaa\NativePackage'
New-Item -ItemType Directory -Force -Path $stage | Out-Null
foreach ($directory in @('Source','Config')) { Copy-Item -LiteralPath (Join-Path $root "plugin\$directory") -Destination $stage -Recurse }
New-Item -ItemType Directory -Force -Path (Join-Path $stage 'Resources'),(Join-Path $stage 'Content'),(Join-Path $stage 'Docs') | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination $stage
Copy-Item -LiteralPath (Join-Path $root 'plugin\Resources\Icon128.png') -Destination (Join-Path $stage 'Resources')
foreach ($doc in @('native-user-guide.md','native-user-guide.txt','native-third-party-notices.md','native-release-notes.md')) { Copy-Item -LiteralPath (Join-Path $root "docs\$doc") -Destination (Join-Path $stage 'Docs') }
$descriptor=Get-Content (Join-Path $root 'plugin\GameProductionAIAssistant.uplugin') -Raw | ConvertFrom-Json
$descriptor.EngineVersion="$engine.0"
$descriptor | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $stage 'GameProductionAIAssistant.uplugin') -Encoding UTF8
if (Test-Path -LiteralPath $output) {
    $full=(Resolve-Path -LiteralPath $output).Path
    $workspace=[IO.Path]::GetFullPath($root).TrimEnd('\')+'\'
    if (-not $full.StartsWith($workspace,[StringComparison]::OrdinalIgnoreCase) -or ((Get-Item -LiteralPath $output -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Unsafe native package cleanup path.' }
    Remove-Item -LiteralPath $output -Recurse -Force
}
$uat=Join-Path $UnrealEngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
& $uat BuildPlugin -Plugin="$(Join-Path $stage 'GameProductionAIAssistant.uplugin')" -Package="$output" -TargetPlatforms=Win64 -StrictIncludes -utf8output
if ($LASTEXITCODE -ne 0) { throw "Native BuildPlugin failed with exit code $LASTEXITCODE" }
foreach ($directory in @('Intermediate','Saved')) {
    $path=Join-Path $output $directory
    if (Test-Path -LiteralPath $path) {
        $full=(Resolve-Path -LiteralPath $path).Path
        if (-not $full.StartsWith([IO.Path]::GetFullPath($output).TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe generated output path.' }
        Remove-Item -LiteralPath $path -Recurse -Force
    }
}
Get-ChildItem -LiteralPath $output -Recurse -File -Force | Where-Object { $_.Extension -in @('.pdb','.log') -or $_.Name -eq '.gitkeep' } | Remove-Item -Force
Copy-Item -LiteralPath (Join-Path $stage 'Docs') -Destination $output -Recurse -Force
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination $output -Force
New-Item -ItemType Directory -Force -Path (Join-Path $output 'Config'),(Join-Path $output 'Resources'),(Join-Path $output 'Content') | Out-Null
& (Join-Path $PSScriptRoot 'verify-native-package.ps1') -PackagePath $stage -FabSource
& (Join-Path $PSScriptRoot 'verify-native-package.ps1') -PackagePath $output
Add-Type -AssemblyName System.IO.Compression.FileSystem
$releases=Join-Path $root '.gpaa\releases'
New-Item -ItemType Directory -Force -Path $releases | Out-Null
foreach ($variant in @(@{Path=$output;Suffix='Win64'},@{Path=$stage;Suffix='FabSource'})) {
    $zip=Join-Path $releases "GameProductionAIAssistant-native-v$($descriptor.VersionName)-UE$engine-$($variant.Suffix).zip"
    if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
    $archive=[IO.Compression.ZipFile]::Open($zip,[IO.Compression.ZipArchiveMode]::Create)
    try {
        $source=(Resolve-Path -LiteralPath $variant.Path).Path.TrimEnd('\')
        # Preserve empty standard directories too; enumerating files alone loses them.
        foreach ($directory in (Get-ChildItem -LiteralPath $source -Recurse -Directory | Sort-Object FullName)) {
            $name=($directory.FullName.Substring($source.Length+1) -replace '\\','/')+'/'
            if ($variant.Suffix -eq 'FabSource') { $name='GameProductionAIAssistant/'+$name }
            $entry=$archive.CreateEntry($name)
            $entry.LastWriteTime=[DateTimeOffset]::new(2000,1,1,0,0,0,[TimeSpan]::Zero)
        }
        foreach ($file in (Get-ChildItem -LiteralPath $source -Recurse -File -Force | Sort-Object FullName)) {
            $name=$file.FullName.Substring($source.Length+1) -replace '\\','/'
            if ($variant.Suffix -eq 'FabSource') { $name='GameProductionAIAssistant/'+$name }
            $entry=$archive.CreateEntry($name,[IO.Compression.CompressionLevel]::Optimal)
            $entry.LastWriteTime=[DateTimeOffset]::new(2000,1,1,0,0,0,[TimeSpan]::Zero)
            $input=$file.OpenRead();$stream=$entry.Open()
            try {$input.CopyTo($stream)} finally {$stream.Dispose();$input.Dispose()}
        }
    } finally {$archive.Dispose()}
    Write-Host "Native archive: $zip"
    Write-Host "SHA256: $((Get-FileHash -LiteralPath $zip).Hash)"
}
