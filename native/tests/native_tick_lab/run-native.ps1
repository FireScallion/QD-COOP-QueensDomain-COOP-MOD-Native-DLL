$ErrorActionPreference='Stop'
$workspace=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$lab=Join-Path $workspace 'work/m2/lab'
Add-Type -Path (Join-Path $lab 'Station.cs')
if([QuietStation]::StationName() -eq 'WinSta0'){throw 'Interactive desktop forbidden'}
$game=Join-Path $lab 'client/game'
$meta=Get-Content (Join-Path $lab 'client/process.json') -Raw|ConvertFrom-Json
$old=Get-Process -Id $meta.pid -ErrorAction SilentlyContinue
if($old -and $old.Path -eq $meta.exe -and $old.StartTime.ToUniversalTime().Ticks -eq ([datetime]$meta.startedUtc).ToUniversalTime().Ticks){throw 'Previous owned game still live'}
$bin=Join-Path $game 'QueensDomain/Binaries/Win64'
$proxy=Join-Path $bin 'dwmapi.dll';$disabled=Join-Path $bin 'dwmapi.dll.m12-disabled'
if((Test-Path -LiteralPath $disabled) -or !(Test-Path -LiteralPath $proxy)){throw 'Unexpected proxy state'}
foreach($name in @('native-tick.json','native-core.json','launch.txt','exit.json')){
 if(Test-Path -LiteralPath (Join-Path $PSScriptRoot $name)){throw "Archive the previous run file $name first"}
}
$profile=Join-Path $PSScriptRoot 'profile';New-Item -ItemType Directory -Force $profile|Out-Null
# Rendered private profiles need a prepared driver cache in this lab. Copy it,
# never hard-link it: this run must not modify the baseline cache or real profile.
$cache=Join-Path $profile 'AppData/Local/NVIDIA/DXCache'
if(!(Test-Path -LiteralPath $cache)){
 $baselineCache=Join-Path $lab 'client/profile/AppData/Local/NVIDIA/DXCache'
 if(!(Test-Path -LiteralPath $baselineCache)){throw 'Prepare an isolated rendering cache before this test'}
 New-Item -ItemType Directory -Force (Split-Path $cache -Parent)|Out-Null
 Copy-Item -LiteralPath $baselineCache -Destination $cache -Recurse
}
$env:LOCALAPPDATA=Join-Path $profile 'AppData/Local';$env:APPDATA=Join-Path $profile 'AppData/Roaming';$env:USERPROFILE=$profile
$env:SteamAppId='1799840';$env:SteamGameId='1799840';$env:QD_NATIVE_REPORT=Join-Path $PSScriptRoot 'native-tick.json'
$env:QD_NATIVE_CORE=Join-Path $PSScriptRoot 'build/QDCoopNativeM1.dll'
$env:QD_NATIVE_CORE_REPORT=Join-Path $PSScriptRoot 'native-core.json'
Move-Item -LiteralPath $proxy -Destination $disabled
try{
 & (Join-Path $PSScriptRoot 'build/QDNativeLaunch.exe') (Join-Path $bin 'QueensDomain-Win64-Shipping.exe') (Join-Path $PSScriptRoot 'build/QDNativeTick.dll') $profile $game | Tee-Object -FilePath (Join-Path $PSScriptRoot 'launch.txt')
 $code=$LASTEXITCODE
 @{launcherExit=$code;station=[QuietStation]::StationName();endedUtc=[datetime]::UtcNow.ToString('o')}|ConvertTo-Json|Set-Content (Join-Path $PSScriptRoot 'exit.json')
 if($code){throw "Native launch failed: $code"}
 $boot=Get-Content (Join-Path $PSScriptRoot 'native-tick.json') -Raw|ConvertFrom-Json
 $core=Get-Content (Join-Path $PSScriptRoot 'native-core.json') -Raw|ConvertFrom-Json
 if($boot.state -ne 2 -or $boot.ue4ss_loaded -or $boot.metadata_found -ne 42 -or
    $boot.core_state -ne 2 -or $boot.packets_sent -ne 3 -or $boot.wrong_thread -ne 0 -or
    $core.build -ne 1 -or $core.calls -le 0 -or $core.thread -ne $boot.game_thread -or
    $core.wrong_thread -ne 0 -or $core.rx -ne 2 -or $core.foreign -ne 1 -or
    $core.stale -ne 1 -or $core.rejected -ne 1 -or $core.socket_errors -ne 0 -or
    $core.native_weak -ne 1 -or $core.native_event -ne 1 -or
    $core.native_reflection -ne 1 -or $core.native_construct -ne 1){
  throw 'Native core evidence did not satisfy the bootstrap and UDP checks'
 }
 Write-Output 'PASS native core metadata, game thread, UDP guards and normal shutdown'
}finally{if(Test-Path -LiteralPath $disabled){Move-Item -LiteralPath $disabled -Destination $proxy}}
