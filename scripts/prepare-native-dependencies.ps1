[CmdletBinding()]
param([switch] $RecordHashes)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
$manifestPath = Join-Path $PSScriptRoot 'native-dependencies.lock.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$cache = Join-Path $root '.gpaa\dependencies'
if ($env:GPAA_NATIVE_DEPENDENCY_CACHE) { $cache=$env:GPAA_NATIVE_DEPENDENCY_CACHE }
New-Item -ItemType Directory -Force -Path $cache | Out-Null
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
foreach ($asset in $manifest.assets) {
    $download = Join-Path $cache $asset.name
    if (-not (Test-Path -LiteralPath $download) -or
        ($asset.sha256 -and (Get-FileHash -LiteralPath $download).Hash -ine $asset.sha256)) {
        Write-Host "Downloading pinned dependency $($asset.name)..."
        for ($attempt=1; $attempt -le 3; $attempt++) {
            $client = New-Object Net.WebClient
            try { $client.DownloadFile($asset.url, "$download.partial"); break }
            catch {
                if ($attempt -eq 3) {throw}
                Write-Host "Pinned download failed; retrying attempt $($attempt+1)/3."
                Start-Sleep -Seconds (2*$attempt)
            } finally { $client.Dispose() }
        }
        Move-Item -LiteralPath "$download.partial" -Destination $download -Force
    }
    $hash = (Get-FileHash -LiteralPath $download -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($RecordHashes) { $asset.sha256 = $hash }
    if (-not $asset.sha256 -or $hash -cne $asset.sha256) { throw "SHA256 mismatch for $($asset.name)" }
    $target = Join-Path $root $asset.destination
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
    if ($asset.name.EndsWith('.zip')) {
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $archive = [IO.Compression.ZipFile]::OpenRead($download)
        try {
            foreach ($item in $asset.entries) {
                $entry = $archive.GetEntry($item.source)
                if (-not $entry) { throw "Missing archive entry $($item.source)" }
                $output = Join-Path $target $item.target
                New-Item -ItemType Directory -Force -Path (Split-Path -Parent $output) | Out-Null
                [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $output, $true)
            }
        } finally { $archive.Dispose() }
    } else { Copy-Item -LiteralPath $download -Destination $target -Force }
}
if ($RecordHashes) { $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8 }
Write-Host 'Native dependencies are ready and SHA256 verified.'
