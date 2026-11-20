[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string] $PackagePath
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=Split-Path -Parent $PSScriptRoot
$package=Get-Item -LiteralPath $PackagePath
if ($package.Extension -ine '.zip') { throw 'Provide a precompiled Windows ZIP.' }
$destination=Join-Path $root ('.gpaa\acceptance-kits\'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $destination | Out-Null
Copy-Item -LiteralPath $package.FullName -Destination (Join-Path $destination 'Plugin.zip')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'test-native-plugin.ps1') -Destination $destination
@{archive='Plugin.zip';sha256=(Get-FileHash -LiteralPath $package.FullName).Hash;originalName=$package.Name} |
    ConvertTo-Json | Set-Content (Join-Path $destination 'manifest.json') -Encoding UTF8
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'run-clean-machine-acceptance.ps1') -Destination $destination
Copy-Item -LiteralPath (Join-Path $root 'docs\clean-machine-acceptance.md') -Destination (Join-Path $destination 'README.md')
$zip=$destination+'.zip'
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory($destination,$zip,[IO.Compression.CompressionLevel]::NoCompression,$false)
Write-Host "Acceptance kit: $zip"
Write-Host "SHA256: $((Get-FileHash -LiteralPath $zip).Hash)"
