[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string] $PackagePath,
    [Parameter(Mandatory=$true)][string] $UnrealEngineRoot
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=Split-Path -Parent $PSScriptRoot
$run=Join-Path $root ('.gpaa\automatic-index-smoke\'+[guid]::NewGuid().ToString('N'))
$hostRoot=Join-Path $run 'Host'
$plugin=Join-Path $hostRoot 'Plugins\GameProductionAIAssistant'
New-Item -ItemType Directory -Force -Path $plugin,(Join-Path $hostRoot 'Content'),(Join-Path $hostRoot 'Config'),(Join-Path $hostRoot 'Source') | Out-Null
Expand-Archive -LiteralPath $PackagePath -DestinationPath $plugin
@{FileVersion=3;DisableEnginePluginsByDefault=$true;Plugins=@(@{Name='GameProductionAIAssistant';Enabled=$true})} |
    ConvertTo-Json -Depth 5 | Set-Content (Join-Path $hostRoot 'Host.uproject') -Encoding UTF8
@'
[GPAASmokeDDC]
Root=(Type=Memory, MaxCacheSize=256)
'@ | Set-Content (Join-Path $hostRoot 'Config\DefaultEngine.ini') -Encoding UTF8
'// Stamina recovers after cooldown.' | Set-Content (Join-Path $hostRoot 'Source\Stamina.cpp')
$log=Join-Path $run 'Unreal.log'
$arguments=@('"'+(Join-Path $hostRoot 'Host.uproject')+'"','-unattended','-nop4','-nosplash','-nullrhi','-nosound',
    '-DDC=GPAASmokeDDC','-NoZenAutoLaunch','-abslog="'+$log+'"')
Write-Host "Automatic index evidence: $run"
$process=Start-Process (Join-Path $UnrealEngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') -ArgumentList $arguments -PassThru -WindowStyle Hidden
$null=$process.Handle
try {
    $deadline=[DateTime]::UtcNow.AddSeconds(180)
    $changed=$false
    $firstCount=0
    $firstObserved=$null
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($process.HasExited) {throw 'Editor exited before the automatic indexing assertions.'}
        $text=if(Test-Path -LiteralPath $log){Get-Content -LiteralPath $log -Raw}else{''}
        if ($text -match 'Fatal error:|Assertion failed:|GPAA automatic index refresh failed:') {throw "Automatic indexing failed; see $log"}
        $events=[regex]::Matches($text,'GPAA automatic index ready: files=(\d+) changed=(\d+) reused=(\d+)')
        if (-not $changed -and $events.Count -ge 1) {
            $firstCount=[int]$events[0].Groups[1].Value
            if($firstCount -lt 1){throw 'Startup did not index the fixture.'}
            $firstObserved=[DateTime]::UtcNow
            '// Inventory contains weapons.' | Set-Content (Join-Path $hostRoot 'Source\Inventory.cpp')
            $changed=$true
        }
        if ($changed -and $events.Count -ge 2) {
            $secondCount=[int]$events[1].Groups[1].Value
            if($secondCount -ne $firstCount+1 -or [int]$events[1].Groups[2].Value -lt 1){throw 'Periodic refresh did not index the newly added file.'}
            $elapsed=([DateTime]::UtcNow-$firstObserved).TotalSeconds
            # The first cold model load can take tens of seconds. Measure job
            # starts (their SQLite connection opens), not completion intervals.
            $starts=[regex]::Matches($text,'(?m)^\[([^\]]+)\].*LogSQLiteDatabase: Opened database.*native-v1\.sqlite')
            if($starts.Count -lt 2){throw 'Index start timestamps are missing from the engine log.'}
            $culture=[Globalization.CultureInfo]::InvariantCulture
            $firstStart=[DateTime]::ParseExact($starts[0].Groups[1].Value,'yyyy.MM.dd-HH.mm.ss:fff',$culture)
            $secondStart=[DateTime]::ParseExact($starts[1].Groups[1].Value,'yyyy.MM.dd-HH.mm.ss:fff',$culture)
            $cadence=($secondStart-$firstStart).TotalSeconds
            if($cadence -lt 59){throw 'Periodic refresh fired too early for a one-minute schedule.'}
            @{passed=$true;startupFiles=$firstCount;periodicFiles=$secondCount;secondsAfterFirstObservation=$elapsed;
                secondsBetweenIndexStarts=$cadence;
                panelOpened=$false;questionSent=$false;archiveSHA256=(Get-FileHash -LiteralPath $PackagePath).Hash} |
                ConvertTo-Json | Set-Content (Join-Path $run 'result.json') -Encoding UTF8
            Write-Host "PASS startup refresh without a panel and one-minute refresh after source modification. Evidence: $run"
            return
        }
        Start-Sleep -Seconds 2
    }
    throw "Timed out waiting for startup/one-minute indexing; see $log"
} finally {
    if(-not $process.HasExited){$process.Kill();$process.WaitForExit()}
    $process.Dispose()
}
