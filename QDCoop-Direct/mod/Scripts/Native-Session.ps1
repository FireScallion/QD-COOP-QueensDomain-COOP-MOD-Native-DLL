param([Parameter(Mandatory=$true)][int]$OwnerPid,[Parameter(Mandatory=$true)][long]$OwnerFileTime,[Parameter(Mandatory=$true)][string]$SaveDirectory)
$ErrorActionPreference='Stop'
# A PowerShell 7 parent may supply a PSModulePath without Windows PowerShell's
# built-in script modules. Bootstrap always launches Windows PowerShell 5.1.
Import-Module (Join-Path $PSHOME 'Modules/Microsoft.PowerShell.Utility/Microsoft.PowerShell.Utility.psd1')
Import-Module (Join-Path $PSHOME 'Modules/Microsoft.PowerShell.Security/Microsoft.PowerShell.Security.psd1')
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../../../../../'))
$mod=Join-Path $root 'QDCoop-Direct'
$state=Join-Path $mod 'Local/native'
$ipc=Join-Path $state 'ipc'
$run="${OwnerPid}_${OwnerFileTime}"
function WriteUtf8($p,$s){[IO.File]::WriteAllText($p,$s,[Text.UTF8Encoding]::new($false))}
function LuaString($s){if($s.Contains(']===]')){throw 'Unsupported path'};return '[===['+$s.Replace('\','/')+']===]'}
$owner=$null;$bridge=$null;$steam=$null;$lock=$null;$nextSteamStart=[DateTime]::MinValue
function EnsureSteamService {
 if($script:steam -and !$script:steam.HasExited){return $true}
 if(!$steamDll){return $false}
 if($script:steam){$script:steam.Dispose();$script:steam=$null}
 try{
  Remove-Item -LiteralPath (Join-Path $ipc 'stop.steam') -ErrorAction SilentlyContinue
  $script:steam=Start-Process -FilePath (Join-Path $mod 'standalone/QDCoopBridge.exe') -ArgumentList ('--steam-service "'+$steamCfg+'"') -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $state 'steam-stdout.txt') -RedirectStandardError (Join-Path $state 'steam-stderr.txt')
  WriteUtf8 (Join-Path $state 'steam.pid') ([string]$script:steam.Id)
  WriteUtf8 (Join-Path $ipc 'steam-ui.txt') 'starting|0|'
  return $true
 }catch{WriteUtf8 (Join-Path $ipc 'steam-ui.txt') 'unavailable|0|Steam service could not start';return $false}
 finally{$script:nextSteamStart=[DateTime]::UtcNow.AddSeconds(30)}
}
function SteamCommand($role,$token,$port=0,$lobby=''){
 $p=Join-Path $ipc 'steam.command';$json=@{Role=$role;Token=$token;Port=$port;Lobby=$lobby}|ConvertTo-Json
 # Readers can briefly hold the previous control file without delete sharing.
 # Retry only in this background supervisor; never stall or tear down LAN.
 for($attempt=0;$attempt -lt 5;$attempt++){
  try{WriteUtf8 ($p+'.tmp') $json;Move-Item -LiteralPath ($p+'.tmp') -Destination $p -Force;return}catch{if($attempt -lt 4){Start-Sleep -Milliseconds 20}}
 }
}
try {
 $owner=Get-Process -Id $OwnerPid
 if($owner.StartTime.ToUniversalTime().ToFileTimeUtc() -ne $OwnerFileTime){throw 'Game process identity mismatch'}
 $heldHandle=$owner.Handle
 [IO.Directory]::CreateDirectory($ipc) | Out-Null
 $lock=[IO.File]::Open((Join-Path $state 'session.lock'),'OpenOrCreate','ReadWrite','None')
 WriteUtf8 (Join-Path $state 'bootstrap.log') ('Starting '+$run+"`n")
 WriteUtf8 (Join-Path $state 'game.pid') ([string]$OwnerPid)
 # Backup real saves before exposing Multiplayer; never import old isolated saves.
 $savePath=[IO.Path]::GetFullPath($SaveDirectory)
 $saveGames=Join-Path $savePath 'SaveGames'
 if(Test-Path -LiteralPath $saveGames){
  $backup=Join-Path $mod ('Backups/'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')+'-'+$run)
  [IO.Directory]::CreateDirectory($backup) | Out-Null
  Copy-Item -LiteralPath $saveGames -Destination $backup -Recurse
 }
 # Only volatile mailboxes are cleared. Persistent rescue/character data is in SaveGames.
 foreach($f in Get-ChildItem -LiteralPath $ipc -File){Remove-Item -LiteralPath $f.FullName}
 # The compatibility key includes original game archives, cached by file stamps.
 $archives=@(Get-ChildItem -LiteralPath (Join-Path $root 'QueensDomain/Content/Paks') -File -Recurse|Where-Object {$_.Extension -in @('.pak','.utoc','.ucas') -and $_.Name -notlike 'QDCoop-*'}|Sort-Object FullName)
 if(!$archives.Count){throw 'Original game archives missing'}
 $stamp=($archives|ForEach-Object {$_.FullName.Substring($root.Length)+'|'+$_.Length+'|'+$_.LastWriteTimeUtc.Ticks}) -join "`n"
 $cachePath=Join-Path $state 'archive-identity.json'
 $cache=$null;try{if(Test-Path -LiteralPath $cachePath){$cache=Get-Content -LiteralPath $cachePath -Raw|ConvertFrom-Json}}catch{}
 if(!$cache -or $cache.Stamp -ne $stamp){
  $hashes=($archives|ForEach-Object { (Get-FileHash -LiteralPath $_.FullName).Hash }) -join '|'
  $sha=[Security.Cryptography.SHA256]::Create();try{$fingerprint=[BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($hashes))).Replace('-','').Substring(0,24)}finally{$sha.Dispose()}
  $cache=@{Stamp=$stamp;Fingerprint=$fingerprint};WriteUtf8 $cachePath ($cache|ConvertTo-Json)
 }
 $sessionToken=[Guid]::NewGuid().ToString('N')
 $channel='Local\QDCoopIPC2_'+$sessionToken
 # Load only the Valve-signed Steam API belonging to this game installation.
 # An unavailable Steam service never prevents the existing LAN mode starting.
 $steamDll=$null
 try{
  $candidates=@(Get-ChildItem -LiteralPath (Join-Path $root 'Engine/Binaries/ThirdParty/Steamworks') -Filter steam_api64.dll -File -Recurse)
  foreach($candidate in $candidates){$signature=Get-AuthenticodeSignature -LiteralPath $candidate.FullName;if($signature.Status -eq 'Valid' -and $signature.SignerCertificate.Subject -match 'O=Valve Corp\.'){$steamDll=$candidate.FullName;break}}
 }catch{}
 $config='return {NativeStartup=true,RunOwner="'+$run+'",RunId="'+$sessionToken+'",Role="menu",MenuMode=true,AutoLoadSlot=false,TargetHP=400,Ipc='+(LuaString $ipc)+',IpcChannel='+('[['+$channel+']]')+',ExpectedSaveDirectory='+(LuaString $savePath)+',GenerationalGC=true,InlineWeakValidation=true,NativeObjectReferences=true,NativeEffectValidation=true,NativeKnownEnemyDiscovery=true,NativeSpear=true,OffscreenEnemyAnimation=true,NativeFarAnimation=true,NativePresentation=true,NativeBodyRead=true,ReuseBodyPose=true,NativeEnemyCapture=true,NativeEnemyPlacement=true,MotionTrace=false}'
 $config=$config.Replace('NativeStartup=true,',('NativeStartup=true,SteamOverlay='+$(if($steamDll){'true'}else{'false'})+','))
 if($steamDll){$config=$config.Replace('NativeStartup=true,',('NativeStartup=true,SteamLibrary='+(LuaString $steamDll)+','))}
 WriteUtf8 (Join-Path $state 'config.lua') $config
 $cfg=@{Role='host';Host='127.0.0.1';Port=47771;Key='QDCoop-LAN-test-2026';Directory=$ipc;OwnerPid=$OwnerPid;OwnerStartTicks=$owner.StartTime.ToUniversalTime().Ticks;Build=('qdcoop-1.18.0-preview3-'+$cache.Fingerprint);IpcChannel=$channel}
 if($steamDll){try{
  $steamCfg=Join-Path $state 'steam.json'
  WriteUtf8 $steamCfg (@{Directory=$ipc;Dll=$steamDll;Build=$cfg.Build;OwnerPid=$OwnerPid;OwnerStartTicks=$cfg.OwnerStartTicks}|ConvertTo-Json)
  $null=EnsureSteamService
 }catch{WriteUtf8 (Join-Path $ipc 'steam-ui.txt') 'unavailable|0|Steam service could not start'}}else{WriteUtf8 (Join-Path $ipc 'steam-ui.txt') 'unavailable|0|Official Steam installation required; LAN remains available'}
 $cfgPath=Join-Path $state 'bridge.json'
 $lastRequest=''
 while(!$owner.HasExited){
  $request='';try{if(Test-Path -LiteralPath (Join-Path $ipc 'session.request')){$request=[IO.File]::ReadAllText((Join-Path $ipc 'session.request'))}}catch [IO.IOException]{$request=$lastRequest}
  if($request -ne $lastRequest){
   if($bridge -and !$bridge.HasExited){WriteUtf8 (Join-Path $ipc 'stop.bridge') '';if(!$bridge.WaitForExit(2000)){$bridge.Kill();$bridge.WaitForExit()}}
   if($bridge){$bridge.Dispose();$bridge=$null}
   $lastRequest=$request
   SteamCommand 'idle' ([Guid]::NewGuid().ToString('N'))
   foreach($f in @('session.error','session.ready','in.state','status.txt')){WriteUtf8 (Join-Path $ipc $f) ''}
   if($request){try{
    $parts=$request.Split('|');if($parts.Count -ne 5 -or $parts[0] -notin @('host','client','steam')){throw 'Invalid room request'}
    if($parts[0] -in @('host','steam')){$null=EnsureSteamService}
    if($parts[0] -eq 'steam'){
     if(!$steam -or $steam.HasExited -or $parts[1] -notmatch '^\d{1,20}$'){throw 'Steam is unavailable'}
     SteamCommand 'join' $parts[4] 0 $parts[1]
     $deadline=[DateTime]::UtcNow.AddSeconds(35);$steamReady=$null
     while(!$owner.HasExited -and !$steam.HasExited -and [DateTime]::UtcNow -lt $deadline){
      if([IO.File]::ReadAllText((Join-Path $ipc 'session.request')) -ne $request){break}
      try{$ss=Get-Content -LiteralPath (Join-Path $ipc 'steam.status') -Raw|ConvertFrom-Json}catch{$ss=$null}
      if($ss -and $ss.Token -eq $parts[4]){if($ss.State -eq 'joined'){$steamReady=$ss;break};if($ss.State -eq 'error'){throw $ss.Error}}
      $null=$owner.WaitForExit(100)
     }
     if(!$steamReady){throw 'Steam connection was cancelled or timed out'}
     $cfg.Role='client';$cfg.Host='127.0.0.1';$cfg.Port=[int]$steamReady.Port
    }else{
     $address=[Net.IPAddress]::Parse($parts[1]);$port=[int]$parts[2]
     if($address.AddressFamily -ne [Net.Sockets.AddressFamily]::InterNetwork -or $port -lt 1024 -or $port -gt 65535){throw 'Invalid IP or port'}
     $cfg.Role=$parts[0];$cfg.Host=$parts[1];$cfg.Port=$port
    }
    WriteUtf8 $cfgPath ($cfg|ConvertTo-Json)
    Remove-Item -LiteralPath (Join-Path $ipc 'stop.bridge') -ErrorAction SilentlyContinue
    $bridge=Start-Process -FilePath (Join-Path $mod 'standalone/QDCoopBridge.exe') -ArgumentList ('"'+$cfgPath+'"') -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $state 'bridge-stdout.txt') -RedirectStandardError (Join-Path $state 'bridge-stderr.txt')
    WriteUtf8 (Join-Path $state 'bridge.pid') ([string]$bridge.Id)
    WriteUtf8 (Join-Path $ipc 'session.ready') $request
    if($parts[0] -eq 'host' -and $steam -and !$steam.HasExited){SteamCommand 'host' $parts[4] $port}
   }catch{WriteUtf8 (Join-Path $ipc 'session.error') $_.Exception.Message}}
  }
  if($bridge -and $bridge.HasExited){WriteUtf8 (Join-Path $ipc 'session.error') 'Connection service exited. Return to Multiplayer and try again.';SteamCommand 'idle' ([Guid]::NewGuid().ToString('N'));$bridge.Dispose();$bridge=$null}
  if($steam -and $steam.HasExited){WriteUtf8 (Join-Path $ipc 'steam-ui.txt') 'unavailable|0|Steam service unavailable; LAN remains available';$steam.Dispose();$steam=$null}
  # A failed Steam helper used to stay unavailable until the entire game was
  # restarted. Recover the host adapter independently of its working LAN room.
  # Guest transport ports cannot be changed under a live session; guests retry
  # through the menu, which also restarts the helper above.
  if(!$steam -and $steamDll -and [DateTime]::UtcNow -ge $nextSteamStart -and (!$bridge -or $cfg.Role -eq 'host')){
   if(EnsureSteamService){if($bridge -and !$bridge.HasExited -and $lastRequest.StartsWith('host|')){SteamCommand 'host' ([Guid]::NewGuid().ToString('N')) $cfg.Port}}
  }
  $null=$owner.WaitForExit(500)
 }
} catch {
 if($lock){WriteUtf8 (Join-Path $state 'bootstrap.log') $_.Exception.ToString()}
} finally {
 if($lock){
  try{WriteUtf8 (Join-Path $ipc 'stop.bridge') '';if($bridge -and !$bridge.HasExited){if(!$bridge.WaitForExit(2000)){$bridge.Kill()}}}catch{}
  try{WriteUtf8 (Join-Path $ipc 'stop.steam') '';if($steam -and !$steam.HasExited){if(!$steam.WaitForExit(2000)){$steam.Kill()}}}catch{}
  $lock.Dispose()
 }
 if($steam){$steam.Dispose()};if($bridge){$bridge.Dispose()};if($owner){$owner.Dispose()}
}
