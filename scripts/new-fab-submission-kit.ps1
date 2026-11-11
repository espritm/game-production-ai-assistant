[CmdletBinding()]
param([string] $Version='0.2.0', [string[]] $EngineVersions=@('5.6','5.8'))
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=Split-Path -Parent $PSScriptRoot
$destination=Join-Path $root ('.gpaa\fab-submission\'+$Version+'-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $destination | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archives=Join-Path $destination 'Archives'
$media=Join-Path $destination 'Media'
$documentation=Join-Path $destination 'Documentation'
New-Item -ItemType Directory -Path $archives,$media,$documentation | Out-Null
foreach($engine in $EngineVersions) {
    foreach($variant in @('FabSource','Win64')) {
        $name="GameProductionAIAssistant-native-v$Version-UE$engine-$variant.zip"
        $source=Join-Path $root ".gpaa\releases\$name"
        if(!(Test-Path -LiteralPath $source -PathType Leaf)) {throw "Missing release archive: $source"}
        $zip=[IO.Compression.ZipFile]::OpenRead($source)
        try {
            $prefix=if($variant -eq 'FabSource'){'GameProductionAIAssistant/'}else{''}
            foreach($entry in $zip.Entries) {
                if($entry.FullName -match '(^|/)(\.\.|Saved|Intermediate|Backend|\.git|\.gpaa)(/|$)' -or $entry.FullName -match '\.(exe|msi|pdb|log)$') {
                    throw "Unexpected archive entry: $($entry.FullName)"
                }
                if($variant -eq 'FabSource' -and !$entry.FullName.StartsWith($prefix,[StringComparison]::Ordinal)) {throw 'Source ZIP must contain exactly one plugin folder.'}
            }
            foreach($required in @('GameProductionAIAssistant.uplugin','Resources/Icon128.png','Docs/native-user-guide.txt','Docs/native-third-party-notices.md','LICENSE')) {
                if(!$zip.GetEntry($prefix+$required)) {throw "Missing $required in $name"}
            }
        } finally {$zip.Dispose()}
        Copy-Item -LiteralPath $source -Destination $archives
    }
}
foreach($name in @('fab-listing-draft.md','fab-submission-readiness.md')) {
    Copy-Item -LiteralPath (Join-Path $root "docs\$name") -Destination $destination
}
Copy-Item -LiteralPath (Join-Path $root 'docs\fab\third-party-declaration.md') -Destination $destination
Copy-Item -LiteralPath (Join-Path $root 'scripts\native-dependencies.lock.json') -Destination $destination
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination $destination
foreach($name in @('native-user-guide.md','native-user-guide.txt','native-third-party-notices.md','native-release-notes.md')) {
    Copy-Item -LiteralPath (Join-Path $root "docs\$name") -Destination $documentation
}
foreach($file in (Get-ChildItem -LiteralPath (Join-Path $root 'docs\fab\media') -File | Where-Object {$_.Extension -in @('.png','.svg','.json')})) {
    Copy-Item -LiteralPath $file.FullName -Destination $media
}
@'
# Fab upload dossier

1. Read fab-listing-draft.md for the copy-ready English description, technical fields and FAQ.
2. Use Media/01-cover.png, 02-in-editor.png and 03-local-and-cloud.png in that order.
3. Upload the FabSource ZIP matching the intended Unreal version. The Win64 ZIPs are precompiled manual-install packages, not substitutes for source review. Each ZIP contains one plugin; do not upload this entire dossier as the Unreal plugin file.
4. Select only Windows x64, UE 5.6 and UE 5.8. Do not select macOS, Linux, runtime/game support or untested engine versions.
5. Complete the actual license and third-party/AI disclosures using the included inventory. Pricing is Free. Existing MIT and third-party notices must remain.
6. Use the local documentation for upload or host it at an owner-approved public URL if requested. The repository is private; its links are not public customer/download links.
7. When Fab assigns an ID, fill the descriptor FabURL with the real listing URL, rebuild the source archive and rerun package verification. No placeholder URL is shipped.
8. Review the listing preview with the owner before step 5 (submission). No listing, upload, public hosting, review submission or publication is performed by preparing this dossier.

SHA256SUMS.json covers every dossier payload file. Archive hashes identify the exact packages being reviewed. The machine-specific test logs and raw screenshots are intentionally not included in the customer/upload dossier.
'@ | Set-Content -LiteralPath (Join-Path $destination 'README.md') -Encoding UTF8
$manifest=@(Get-ChildItem -LiteralPath $destination -Recurse -File | Sort-Object FullName | ForEach-Object {
    @{file=$_.FullName.Substring($destination.Length+1).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
})
@{version=$Version;createdUtc=[DateTime]::UtcNow.ToString('o');engineVersions=$EngineVersions;platform='Win64';files=$manifest} |
    ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $destination 'SHA256SUMS.json') -Encoding UTF8
$zipPath=$destination+'.zip'
[IO.Compression.ZipFile]::CreateFromDirectory($destination,$zipPath,[IO.Compression.CompressionLevel]::NoCompression,$false)
Write-Host "Fab dossier: $destination"
Write-Host "Dossier ZIP: $zipPath"
Write-Host "SHA256: $((Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash)"
