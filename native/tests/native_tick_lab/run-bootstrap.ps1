$ErrorActionPreference='Stop'
$workspace=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$lab=Join-Path $workspace 'work/m2/lab'
Add-Type -Path (Join-Path $lab 'Station.cs')
if([QuietStation]::StationName() -eq 'WinSta0'){throw 'Interactive desktop forbidden'}
$game=Join-Path $lab 'client/game'
$bin=Join-Path $game 'QueensDomain/Binaries/Win64'
$exe=Join-Path $bin 'QueensDomain-Win64-Shipping.exe'
$build=Join-Path $PSScriptRoot 'build'
$socialLab=$env:QD_BOOT_SOCIAL_LAB -eq '1'
$launch=Join-Path $build 'QDNativeLaunch.exe'
$probe=Join-Path $build 'QDNativeBootProbe.dll'
$native=@('xinput1_3.dll','QDNativeBootstrap.dll','QDCoopNativeM1.dll')
$nativeSource=@{}
foreach($name in $native){
 $folder=if($socialLab -and $name -eq 'QDNativeBootstrap.dll'){Join-Path $PSScriptRoot 'social-build'}else{$build}
 $nativeSource[$name]=Join-Path $folder $name
}
$proxy=Join-Path $bin 'dwmapi.dll'
$backup=Join-Path $bin 'dwmapi.dll.m26-disabled'
foreach($file in @($exe,$launch,$probe,$proxy)+@($nativeSource.Values)){
 if(!(Test-Path -LiteralPath $file -PathType Leaf)){throw "Missing $file"}
}
if((Test-Path -LiteralPath $backup) -or
   (Get-Process -Name QueensDomain-Win64-Shipping -ErrorAction SilentlyContinue|Where-Object Path -eq $exe)){
 throw 'Isolated copy is busy or its loader is already moved'
}
foreach($name in $native){
 if(Test-Path -LiteralPath (Join-Path $bin $name)){throw "Unexpected $name in isolated game"}
}

function Sha256([string]$Path){
 $file=[IO.File]::OpenRead($Path)
 $algorithm=[Security.Cryptography.SHA256]::Create()
 try{ [BitConverter]::ToString($algorithm.ComputeHash($file)).Replace('-','') }
 finally{ $file.Dispose(); $algorithm.Dispose() }
}
function SaveHashes([string]$Directory){
 @(Get-ChildItem -LiteralPath $Directory -Recurse -File|Sort-Object FullName|ForEach-Object{
  [pscustomobject]@{path=$_.FullName.Substring($Directory.Length);hash=(Sha256 $_.FullName)}
 })
}
$run=Join-Path $PSScriptRoot ('runs/'+[guid]::NewGuid().ToString('N'))
$profile=Join-Path $run 'profile'
$saved=Join-Path $profile 'Saved'
New-Item -ItemType Directory -Force $saved|Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'profile/Saved/Config') -Destination $saved -Recurse
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'profile/Saved/SaveGames') -Destination $saved -Recurse
$saves=Join-Path $saved 'SaveGames'
$before=SaveHashes $saves
$renderCache=Join-Path $profile 'AppData/Local/NVIDIA/DXCache'
New-Item -ItemType Directory -Force (Split-Path $renderCache -Parent)|Out-Null
Copy-Item -LiteralPath (Join-Path $lab 'client/profile/AppData/Local/NVIDIA/DXCache') -Destination $renderCache -Recurse
Set-Content -LiteralPath (Join-Path $PSScriptRoot 'latest-run.txt') -Value $run
$originalHash=Sha256 $proxy
$moved=$false
$deployed=@()
try{
 Move-Item -LiteralPath $proxy -Destination $backup
 $moved=$true
 foreach($name in $native){
  $target=Join-Path $bin $name
  $deployed+=@($target)
  Copy-Item -LiteralPath $nativeSource[$name] -Destination $target
 }
 $env:LOCALAPPDATA=Join-Path $profile 'AppData/Local'
 $env:APPDATA=Join-Path $profile 'AppData/Roaming'
 $env:USERPROFILE=$profile
 $env:SteamAppId='1799840'
 $env:SteamGameId='1799840'
 $env:QD_WORLD_ROLE='client'
 $env:QD_BOOT_REPORT=Join-Path $run 'bootstrap.json'
 $parameters=@($exe,$probe,$profile,$game)
 if(!$socialLab){$parameters+=@('-QDCoopHost=4')}
 $arguments=$parameters|ForEach-Object{'"'+$_+'"'}
 $test=Start-Process -FilePath $launch -ArgumentList $arguments -WindowStyle Hidden -PassThru `
  -RedirectStandardOutput (Join-Path $run 'launch.txt') -RedirectStandardError (Join-Path $run 'launcher-error.txt')
 $null=$test.Handle
 $test.WaitForExit()
 $log=Get-Content (Join-Path $run 'launch.txt') -Raw
 if($test.ExitCode -ne 0 -or $log -notmatch '(?m)^game_exit=0\s*$' -or
    $log -notmatch '(?m)^ue4ss_module=0\s*$' -or $log -notmatch '(?m)^owned_pid=(\d+)\s*$'){
  throw "Isolated game exit or module check failed. See $run"
 }
 $gamePid=[int]$Matches[1]
 $evidence=Get-Content -LiteralPath $env:QD_BOOT_REPORT -Raw|ConvertFrom-Json
 if(!$evidence.passed){throw "Bootstrap room sequence failed. See $run"}
 $after=SaveHashes $saves
 if($before.Count -eq 0 -or (Compare-Object $before $after -Property path,hash)){throw 'Isolated saves differ'}
}finally{
 foreach($target in $deployed){if(Test-Path -LiteralPath $target){Remove-Item -LiteralPath $target}}
 if($moved){Move-Item -LiteralPath $backup -Destination $proxy}
}
if((Sha256 $proxy) -ne $originalHash){throw 'Original isolated loader differs'}
@{passed=$true;socialLab=$socialLab;gamePid=$gamePid;station=[QuietStation]::StationName();saveFilesMatched=$before.Count;
  shippingHash=(Sha256 $exe);proxyRestored=$true;gameExit=$test.ExitCode;room=$evidence}|
 ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $run 'summary.json')
