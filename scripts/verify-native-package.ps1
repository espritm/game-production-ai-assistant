[CmdletBinding()]
param([Parameter(Mandatory=$true)][string] $PackagePath, [switch] $FabSource)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$resolved=(Resolve-Path -LiteralPath $PackagePath).Path.TrimEnd('\','/')
$files=@(Get-ChildItem -LiteralPath $resolved -Recurse -File -Force)
foreach ($directory in @('Config','Content','Resources','Source')) {
    if (-not (Test-Path -LiteralPath (Join-Path $resolved $directory) -PathType Container)) { throw "Missing native package directory: $directory" }
}
foreach ($file in $files) {
    $relative=$file.FullName.Substring($resolved.Length+1) -replace '\\','/'
    if ($relative -match '(^|/)(Intermediate|Saved|\.gpaa|\.git|obj)(/|$)' -or $relative -match '\.(exe|msi|pdb|log|user|suo)$' -or $relative -match 'Resources/Backend' -or $file.Name -eq '.gitkeep') { throw "Forbidden native package file: $relative" }
    if ($FabSource -and ($relative -match '^(Binaries|Build)/' -or $relative.Length -gt 170)) { throw "Fab source package policy failed: $relative" }
    if ($relative -match '^Source/' -and $relative -notmatch '^Source/ThirdParty/' -and $file.Extension -in @('.cpp','.h','.cs')) {
        if ((Get-Content -LiteralPath $file.FullName -TotalCount 1) -notmatch '^// Copyright .+ 2026\.') { throw "Missing publisher copyright: $relative" }
    }
}
$required=@('Resources/Icon128.png','Docs/native-user-guide.txt','Docs/native-release-notes.md','GameProductionAIAssistant.uplugin','Source/GPAACore/GPAACore.Build.cs','Source/GPAAInference/GPAAInference.Build.cs','Source/GPAAssistant/GPAAssistant.Build.cs',
    'Source/ThirdParty/OnnxRuntime/Win64/gpaa_onnxruntime.dll','Source/ThirdParty/OnnxRuntime/LICENSE','Source/ThirdParty/OnnxRuntime/ThirdPartyNotices.txt',
    'Source/ThirdParty/MiniLM/model.onnx','Source/ThirdParty/MiniLM/vocab.txt','Source/ThirdParty/MiniLM/LICENSE','Docs/native-user-guide.md','Docs/native-third-party-notices.md')
if (-not $FabSource) { $required+=@('Binaries/Win64/UnrealEditor-GPAACore.dll','Binaries/Win64/UnrealEditor-GPAAInference.dll','Binaries/Win64/UnrealEditor-GPAAssistant.dll') }
foreach ($path in $required) { if (-not (Test-Path -LiteralPath (Join-Path $resolved $path) -PathType Leaf)) { throw "Missing native package file: $path" } }
$lock=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'native-dependencies.lock.json') -Raw | ConvertFrom-Json
foreach ($asset in $lock.assets) {
    $relative=$asset.destination -replace '^plugin/',''
    if ($asset.name.EndsWith('.zip')) {
        foreach ($entry in $asset.entries) {
            $path=Join-Path (Join-Path $resolved $relative) $entry.target
            if (-not (Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path).Hash -ine $entry.sha256) { throw "Native dependency integrity failed: $path" }
        }
    } else {
        $path=Join-Path $resolved $relative
        if (-not (Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path).Hash -ine $asset.sha256) { throw "Native dependency integrity failed: $path" }
    }
}
$descriptor=Get-Content -LiteralPath (Join-Path $resolved 'GameProductionAIAssistant.uplugin') -Raw | ConvertFrom-Json
if (-not $descriptor.EngineVersion -or $descriptor.Modules.Count -ne 3) { throw 'Invalid native plugin descriptor.' }
foreach ($module in $descriptor.Modules) { if ($module.PlatformAllowList -notcontains 'Win64') { throw "Missing Win64 allowlist: $($module.Name)" } }
Write-Host "PASS native package policy (Fab source: $FabSource). This does not constitute Fab approval."
