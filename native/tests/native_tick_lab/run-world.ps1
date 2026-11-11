$ErrorActionPreference='Stop'
$automatic=$env:QD_WORLD_AUTOMATIC -eq '1'
$reopen=$automatic -and $env:QD_WORLD_REOPEN -eq '1'
$steamSmoke=$env:QD_WORLD_STEAM_SMOKE -eq '1'
$recoveryLab=$env:QD_WORLD_RECOVERY_LAB -eq '1'
$players=$env:QD_WORLD_PLAYERS -eq '1'
$worldLan=$players -and [bool]$env:QD_WORLD_LAN_IPV4
$playerBody=$players -and $env:QD_WORLD_PLAYER_BODY -eq '1'
$playerBodyFallback=$players -and $env:QD_WORLD_PLAYER_BODY_FALLBACK -eq '1'
$playerGear=$players -and $env:QD_WORLD_PLAYER_GEAR -eq '1'
$playerBones=$playerGear -and $env:QD_WORLD_PLAYER_BONES -eq '1'
$playerRescue=$players -and $env:QD_WORLD_PLAYER_RESCUE -eq '1'
$playerMedic=$playerRescue -and $env:QD_WORLD_PLAYER_MEDIC -eq '1'
$playerPresentation=$playerRescue -and $env:QD_WORLD_PLAYER_PRESENTATION -eq '1'
$playerCheckpoint=$players -and $env:QD_WORLD_PLAYER_CHECKPOINT -eq '1'
$worldProgress=$players -and $env:QD_WORLD_PROGRESS_LAB -eq '1'
$worldQuest=$worldProgress -and $env:QD_WORLD_QUEST_LAB -eq '1'
$worldPause=$players -and $env:QD_WORLD_PAUSE_LAB -eq '1'
$worldPhoto=$players -and $env:QD_WORLD_PHOTO_LAB -eq '1'
$damageLab=$players -and $env:QD_WORLD_DAMAGE_LAB -eq '1'
$hitboxLab=$players -and $env:QD_WORLD_HITBOX_LAB -eq '1'
$hostMeleeLab=$hitboxLab -and $env:QD_WORLD_HOST_MELEE_LAB -eq '1'
$enemyDamageLab=$players -and $env:QD_WORLD_ENEMY_DAMAGE_LAB -eq '1'
$threatLab=$players -and $env:QD_WORLD_THREAT_LAB -eq '1'
$threatMemoryLab=$threatLab -and $env:QD_WORLD_THREAT_MEMORY_LAB -eq '1'
$travelGuardLab=$players -and $env:QD_WORLD_TRAVEL_GUARD_LAB -eq '1'
$streamingLab=$players -and $env:QD_WORLD_STREAM_LAB -eq '1'
$arrivalLab=$players -and $env:QD_WORLD_ARRIVAL_LAB -eq '1'
$arrivalCancelLab=$arrivalLab -and $env:QD_WORLD_ARRIVAL_CANCEL_LAB -eq '1'
$persistentSaveProbe=$players -and $env:QD_WORLD_PERSISTENT_SAVE_PROBE -eq '1'
$persistentReload=$players -and $env:QD_WORLD_PERSISTENT_RELOAD_LAB -eq '1'
$persistentCross=$persistentReload -and $env:QD_WORLD_PERSISTENT_CROSS_LAB -eq '1'
$worldMusic=$players -and $env:QD_WORLD_MUSIC_LAB -eq '1'
$worldStory=$players -and $env:QD_WORLD_STORY_LAB -eq '1'
$worldLibrary=$players -and $env:QD_WORLD_LIBRARY_LAB -eq '1'
$worldElevator=$players -and $env:QD_WORLD_ELEVATOR_LAB -eq '1'
$elevatorGuest=$players -and $env:QD_WORLD_ELEVATOR_GUEST_LAB -eq '1'
$guestSafeSave=$players -and $env:QD_WORLD_GUEST_SAFE_SAVE_LAB -eq '1'
$worldChest=$players -and $env:QD_WORLD_CHEST_LAB -eq '1'
$worldMechanism=$players -and $env:QD_WORLD_MECHANISM_LAB -eq '1'
$shrineGate=$worldMechanism -and $env:QD_WORLD_SHRINE_GATE_LAB -eq '1'
$castleGate=$players -and $env:QD_WORLD_CASTLE_GATE_LAB -eq '1'
$worldBarrier=$castleGate -and $env:QD_WORLD_BARRIER_LAB -eq '1'
$worldArena=$players -and $env:QD_WORLD_ARENA_LAB -eq '1'
$worldCutscene=$players -and $env:QD_WORLD_CUTSCENE_LAB -eq '1'
$worldBoss=$players -and $env:QD_WORLD_BOSS_LAB -eq '1'
$worldBossInteract=$worldBoss -and $env:QD_WORLD_BOSS_INTERACT_LAB -eq '1'
$checkpointRefresh=$playerCheckpoint -and $env:QD_WORLD_CHECKPOINT_REFRESH -eq '1'
$checkpointWipe=$playerCheckpoint -and $env:QD_WORLD_CHECKPOINT_WIPE -eq '1'
$checkpointWipeMissing=$checkpointWipe -and $env:QD_WORLD_CHECKPOINT_WIPE_MISSING -eq '1'
$checkpointReopened=$checkpointWipe -and !$checkpointWipeMissing
$medicInterrupt=$playerMedic -and $env:QD_WORLD_MEDIC_INTERRUPT -eq '1'
$medicReplay=$env:QD_WORLD_MEDIC_REPLAY
$refundReplay=$false
if($medicReplay){
 $medicReplay=[IO.Path]::GetFullPath($medicReplay)
 if(!$players -or $playerRescue -or (Split-Path $medicReplay -Parent) -ne (Join-Path $PSScriptRoot 'runs') -or
    (Split-Path $medicReplay -Leaf) -notmatch '^[a-f0-9]{32}$'){
   throw 'Use a completed isolated medicine run from this lab for replay'
 }
 $previous=Get-Content -LiteralPath (Join-Path $medicReplay 'summary.json') -Raw|ConvertFrom-Json
 if(!$previous.passed -or !$previous.playerMedic){throw 'The replay source must have passed medic verification'}
 $refundReplay=$previous.playerMedicInterrupt -eq $true
 $env:QD_WORLD_MEDIC_REPLAY_REFUND=if($refundReplay){'1'}else{'0'}
}
if(@($playerBody,$playerBodyFallback,$playerGear,$playerRescue).Where({$_}).Count -gt 1){
 throw 'Select a single player presentation lab mode'
}
if($recoveryLab -and ($automatic -or $steamSmoke)){throw 'Select a single recovery lab mode'}
if($players -and ($automatic -or $steamSmoke -or $recoveryLab)){throw 'Select a single player lab mode'}
$clientCount=3
if($env:QD_WORLD_CLIENTS -and
   (![int]::TryParse($env:QD_WORLD_CLIENTS,[ref]$clientCount) -or $clientCount -lt 1 -or $clientCount -gt 3)){
 throw 'QD_WORLD_CLIENTS must be between 1 and 3'
}
if($players -and $clientCount -ne 3){throw 'Player verification needs four game processes'}
function SaveSha256([string]$Path){
 $file=[IO.File]::OpenRead($Path)
 $algorithm=[Security.Cryptography.SHA256]::Create()
 try{ [BitConverter]::ToString($algorithm.ComputeHash($file)) }
 finally{ $file.Dispose(); $algorithm.Dispose() }
}
$workspace=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$lab=Join-Path $workspace 'work/m2/lab'
Add-Type -Path (Join-Path $lab 'Station.cs')
if([QuietStation]::StationName() -eq 'WinSta0'){throw 'Interactive desktop forbidden'}
$bodyPaks=@{
 'QDCoop-NativeBody.pak'='8A-4A-B9-63-88-58-1E-BC-D6-C3-5E-E3-8F-3B-9E-AD-27-61-A4-BF-2C-00-81-CE-A5-3C-80-48-44-3D-F1-29'
 'QDCoop-NativeSpear.pak'='45-74-1B-B3-49-AA-74-7B-19-95-17-01-B7-CF-F3-65-0E-FA-B7-EF-5B-38-4F-56-D6-4D-6E-8F-B5-A4-05-09'
 'QDCoop-Skins.pak'='83-2F-19-64-C3-B3-E7-F4-02-89-29-8A-0D-61-58-D9-34-39-95-F7-7D-87-B2-F2-7A-67-2F-85-A3-51-23-BB'
}
$selectedPaks=@(if($playerGear){$bodyPaks.Keys}elseif($playerBody -or $playerRescue){'QDCoop-NativeBody.pak';'QDCoop-NativeSpear.pak'}elseif($playerBodyFallback){'QDCoop-NativeBody.pak'})
if($damageLab -or $hitboxLab -or $enemyDamageLab -or $threatLab){
 $pak='QDCoop-GuestTarget.pak'
 $bodyPaks[$pak]='AF-10-79-61-D4-01-07-66-2C-11-77-BA-EF-57-F3-0F-06-F5-D9-B3-30-3C-FC-81-50-35-D4-F9-86-93-96-A1'
 $selectedPaks+=@($pak)
}
if($selectedPaks.Count){
 foreach($pak in $selectedPaks){
  if((SaveSha256 (Join-Path $PSScriptRoot "assets/$pak")) -ne $bodyPaks[$pak]){
   throw "Use the original preview3 asset pack $pak"
  }
 }
}

$roles=@(if($steamSmoke -or $recoveryLab){'host'}else{@('host')+@('client','client2','client3')[0..($clientCount-1)]})
$launch=Join-Path $PSScriptRoot 'build/QDNativeLaunch.exe'
$hook=Join-Path $PSScriptRoot 'build/QDNativeWorld.dll'
$coreDll=Join-Path $PSScriptRoot $(if($recoveryLab){'recovery-build/QDCoopNativeM1.dll'}else{'build/QDCoopNativeM1.dll'})
foreach($file in @($launch,$hook,$coreDll)){
 if(!(Test-Path -LiteralPath $file -PathType Leaf)){throw "Missing build file $file"}
}
$games=@{}
$proxies=@{}
$copies=if($steamSmoke -or $recoveryLab){@('host')}else{@('host','client')}
foreach($copy in $copies){
 $game=Join-Path $lab "$copy/game"
 $bin=Join-Path $game 'QueensDomain/Binaries/Win64'
 $exe=Join-Path $bin 'QueensDomain-Win64-Shipping.exe'
 if(!(Test-Path -LiteralPath $exe -PathType Leaf)){throw "Missing isolated game $exe"}
 if(Get-Process -Name QueensDomain-Win64-Shipping -ErrorAction SilentlyContinue | Where-Object Path -eq $exe){
  throw "Isolated copy is running at $exe"
 }
 $proxy=Join-Path $bin 'dwmapi.dll'
 $disabled=Join-Path $bin 'dwmapi.dll.m25-disabled'
 if((Test-Path -LiteralPath $disabled) -or !(Test-Path -LiteralPath $proxy -PathType Leaf)){
  throw "Unexpected proxy state in $bin"
 }
 $games[$copy]=@{root=$game;exe=$exe}
 $proxies[$copy]=@{active=$proxy;disabled=$disabled}
}

$runRoot=Join-Path $PSScriptRoot ('runs/'+[guid]::NewGuid().ToString('N'))
foreach($role in $roles){
 $profile=Join-Path $runRoot "$role/profile"
 New-Item -ItemType Directory -Force (Join-Path $profile 'Saved')|Out-Null
 foreach($name in @('Config','SaveGames')){
  Copy-Item -LiteralPath (Join-Path $PSScriptRoot "profile/Saved/$name") -Destination (Join-Path $profile "Saved/$name") -Recurse
 }
 if($medicReplay){
  Copy-Item -LiteralPath (Join-Path $medicReplay "$role/profile/Saved/SaveGames/World1.sav.qdrescue-native-v1") `
    -Destination (Join-Path $profile 'Saved/SaveGames')
 }
}
if(!$steamSmoke -and !$recoveryLab){
 $renderCache=Join-Path $runRoot 'client/profile/AppData/Local/NVIDIA/DXCache'
 New-Item -ItemType Directory -Force (Split-Path $renderCache -Parent)|Out-Null
 Copy-Item -LiteralPath (Join-Path $lab 'client/profile/AppData/Local/NVIDIA/DXCache') -Destination $renderCache -Recurse
}
Set-Content -LiteralPath (Join-Path $PSScriptRoot 'latest-run.txt') -Value $runRoot

$children=@{}
$moved=@()
$stagedPaks=@()
try{
 foreach($copy in $copies){
  $proxy=$proxies[$copy]
  Move-Item -LiteralPath $proxy.active -Destination $proxy.disabled
  $moved+=@($proxy)
  if($selectedPaks.Count){
   foreach($pak in $selectedPaks){
    $target=Join-Path $games[$copy].root "QueensDomain/Content/Paks/$pak"
    if(Test-Path -LiteralPath $target){throw "Unexpected isolated asset pack $target"}
    $stagedPaks+=@($target)
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "assets/$pak") -Destination $target
   }
  }
 }
 foreach($role in $roles){
  $copy=if($role -eq 'host'){'host'}else{'client'}
  $game=$games[$copy]
  $run=Join-Path $runRoot $role
  $profile=Join-Path $run 'profile'
  $env:LOCALAPPDATA=Join-Path $profile 'AppData/Local'
  $env:APPDATA=Join-Path $profile 'AppData/Roaming'
  $env:USERPROFILE=$profile
  $env:SteamAppId='1799840'
  $env:SteamGameId='1799840'
  $env:QD_NATIVE_CORE=$coreDll
  $env:QD_WORLD_ROOT=$runRoot
  $env:QD_WORLD_CLIENTS=[string]$clientCount
  $env:QD_WORLD_ROLE=$role
  $env:QD_NATIVE_REPORT=Join-Path $run 'native-world.json'
  $env:QD_NATIVE_CORE_REPORT=Join-Path $run 'native-core.json'
  $env:QD_WORLD_VERIFIED=Join-Path $run 'apply-verified.txt'
  $env:QD_WORLD_READY=Join-Path $run 'socket-ready.txt'
  $arguments=@($game.exe,$hook,$profile,$game.root)|ForEach-Object{'"'+$_+'"'}
  $children[$role]=Start-Process -FilePath $launch -ArgumentList $arguments -WindowStyle Hidden -PassThru `
   -RedirectStandardOutput (Join-Path $run 'launch.txt') -RedirectStandardError (Join-Path $run 'launcher-error.txt')
  # Keep a handle while the launcher is live so ExitCode survives process exit.
  $null=$children[$role].Handle
 }
 foreach($role in $roles){
  $child=$children[$role]
  $child.WaitForExit()
  @{launcherExit=$child.ExitCode;station=[QuietStation]::StationName();endedUtc=[datetime]::UtcNow.ToString('o')}|
   ConvertTo-Json|Set-Content (Join-Path $runRoot "$role/exit.json")
 }

 $worlds=@{}
 $cores=@{}
 $automaticReports=@{}
 $playerReports=@{}
 $weaponResults=@{}
 $checkpointReports=@{}
 $lanScopes=@{}
 $lanSlots=@{}
 $lanFirstScopes=@{}
 $lanFirstSlots=@{}
 $saveDifferences=@{}
 $gamePids=@()
 for($index=0;$index -lt $roles.Count;++$index){
  $role=$roles[$index]
  $run=Join-Path $runRoot $role
  if($children[$role].ExitCode -ne 0){throw "$role launcher failed with $($children[$role].ExitCode)"}
  $log=Get-Content (Join-Path $run 'launch.txt') -Raw
  if($log -notmatch '(?m)^owned_pid=(\d+)\s*$'){throw "Missing $role game PID"}
  $gamePids+=[int]$Matches[1]
  if($log -notmatch '(?m)^game_exit=0\s*$' -or $log -notmatch '(?m)^ue4ss_module=0\s*$'){
   throw "$role game exit or module check failed"
  }
  $world=Get-Content (Join-Path $run 'native-world.json') -Raw|ConvertFrom-Json
  $core=Get-Content (Join-Path $run 'native-core.json') -Raw|ConvertFrom-Json
  if($players){
   $player=Get-Content (Join-Path $run 'native-players.json') -Raw|ConvertFrom-Json
   $expectedBuild=if($worldLan -and $role -ne 'host'){-1}else{1}
   $ownSlot=$index
   if($worldLan){
    $assigned=[IO.File]::ReadAllBytes((Join-Path $run 'epoch2-lan-slot.bin'))
    if($assigned.Length -ne 1 -or $assigned[0] -gt 3){throw "$role invalid assigned LAN slot"}
    $ownSlot=[int]$assigned[0]
   }
   if(!$world.passed -or $world.ue4ss_loaded -or $world.wrong_thread -ne 0 -or
      $core.build -ne $expectedBuild -or $core.wrong_thread -ne 0 -or $core.queue_drops -ne 0 -or
      $core.player_sent -lt 4 -or $core.player_received -lt 9 -or
      $core.player_created -ne (3+3*[int]($checkpointReopened -or $persistentReload)+3*[int]$persistentCross+3*[int]$worldLan) -or
      $core.player_destroyed -ne (3+3*[int]($checkpointReopened -or $persistentReload)+3*[int]$persistentCross+3*[int]$worldLan) -or
      !$player.clean -or $player.stage -ne 5 -or $player.poses.Count -ne 4 -or
      @($player.poses|Where-Object serial -gt 0).Count -ne 3 -or
      $player.poses[$ownSlot].serial -ne 0 -or
      (!$worldLan -and !$medicInterrupt -and !$checkpointWipeMissing -and !$streamingLab -and !$arrivalLab -and !$persistentSaveProbe -and !$persistentReload -and !$worldMusic -and !$worldStory -and !$worldLibrary -and !$worldElevator -and !$elevatorGuest -and !$castleGate -and !$worldArena -and !$worldCutscene -and !$worldBoss -and !$guestSafeSave -and $role -ne 'client3' -and (!$player.hidden_on_timeout -or !$player.retired_on_timeout)) -or
      ($role -eq 'host' -and $core.player_relayed -lt 9) -or
      ($role -ne 'host' -and $core.player_relayed -ne 0)){
     throw "$role failed four-player pose verification"
   }
   if($playerBody -and (!$player.body_verified -or $core.player_graph_bound -ne 3 -or
      $core.player_body_frames -lt 10 -or
      ($role -ne 'host' -and ($core.player_action_starts -lt 2 -or $core.player_action_stops -lt 2)))){
     throw "$role failed native locomotion and action verification"
   }
   if($worldProgress){
    if(!(Test-Path -LiteralPath (Join-Path $run 'world-progress-done.txt') -PathType Leaf) -or
       !(Test-Path -LiteralPath (Join-Path $run 'world-progress-items.txt') -PathType Leaf) -or
       $core.world_progress_errors -ne 0 -or
       ($role -eq 'host' -and $core.world_progress_sent -lt 6) -or
       ($role -ne 'host' -and ($core.world_progress_applied -lt 3 -or $core.world_progress_writes -lt 5))){
     throw "$role failed typed world progress overlay verification"
    }
   }
   if($worldQuest){
    if(($role -eq 'host' -and ($core.world_state_notifies -ne 0 -or $core.world_quest_notifies -ne 0)) -or
       ($role -ne 'host' -and ($core.world_state_notifies -ne 3 -or $core.world_quest_notifies -ne 4))){
     throw "$role failed scoped world and quest callback verification"
    }
   }
   if($worldPause){
    if($core.pause_unpaused -lt 1 -or $core.pause_menu_dilation -lt 1 -or $core.pause_enemy_dilation -lt 1 -or
       $core.pause_restored -ne 1 -or
       !(Test-Path -LiteralPath (Join-Path $run 'world-pause-ok.txt') -PathType Leaf) -or
       !(Test-Path -LiteralPath (Join-Path $run 'world-pause-clean.txt') -PathType Leaf)){
     throw "$role failed native multiplayer pause verification"
    }
   }
   if($worldPhoto){
    if($core.pause_modal_dilation -ne 1 -or $core.pause_modal_restored -ne 1 -or $core.pause_restored -ne 1 -or
       !(Test-Path -LiteralPath (Join-Path $run 'world-photo-ok.txt') -PathType Leaf) -or
       !(Test-Path -LiteralPath (Join-Path $run 'world-photo-clean.txt') -PathType Leaf)){
     throw "$role failed visible modal time-dilation verification"
    }
   }
   if($hitboxLab){
    $attacker=if($hostMeleeLab){'host'}else{'client'}
    $sent=[int](!$hostMeleeLab -and $role -eq $attacker)
    $settled=[int](!$hostMeleeLab -and $role -eq 'host')
    $hostObserved=[int]($hostMeleeLab -and $role -eq 'host')
    $mayCounterattack=!$hostMeleeLab -and $role -eq 'host'
    $mayBeHit=!$hostMeleeLab -and $role -ne 'host'
    if(!(Test-Path -LiteralPath (Join-Path $run 'weapon-hit.txt') -PathType Leaf) -or
       !(Test-Path -LiteralPath (Join-Path $run 'damage-clean.txt') -PathType Leaf) -or
       (!$mayCounterattack -and $core.damage_sent -ne $sent) -or $core.damage_acked -ne $core.damage_sent -or
       $core.damage_suppressed -ne $sent -or (!$mayBeHit -and $core.damage_applied -ne $settled) -or
       $core.threat_host_hits -ne $hostObserved -or
       ($role -eq 'host' -and $core.damage_proxies -ne 3) -or $core.damage_rejected -ne 0){
     throw "$role failed stock weapon input and enemy hit query verification"
    }
    $proof=Get-Content -LiteralPath (Join-Path $run 'weapon-hit.txt') -Raw
    if($proof -notmatch '^generation=1 hp_before=60\.000 hp_after=(\d+\.\d+) actions='){
     throw "$role has invalid weapon impact evidence"
    }
    $weaponResults[$role]=$Matches[1]
    # A successful local stock hit is already in the attack's hit set by the
    # time the supplemental probe runs. Guest replay does not modify that set.
    if(!$hostMeleeLab -and $role -eq $attacker -and !((Get-ChildItem -LiteralPath $run -Filter 'weapon-trace-*.txt' -File |
      Select-String 'enemy=1\b' -List)|Select-Object -First 1)){
     throw 'The original melee trace did not report the enrolled enemy'
    }
   }
   if($streamingLab){
    if(!(Test-Path -LiteralPath (Join-Path $run 'streaming-synchronized.txt') -PathType Leaf) -or
       ($role -eq 'host' -and ($core.world_stream_published -lt 1 -or $core.world_stream_restores -ne 0)) -or
       ($role -ne 'host' -and ($core.world_stream_restores -lt 1 -or $core.world_stream_verified -lt 1))){
     throw "$role failed authoritative stock level streaming verification"
    }
   }
   if($persistentSaveProbe -and $role -eq 'host' -and !(Test-Path -LiteralPath (Join-Path $run 'persistent-save-cloned.txt'))){
    throw 'Stock persistent travel save clone evidence missing'
   }
   if($persistentReload){
    $guest=$role -ne 'host'
    $changes=1+[int]$persistentCross
    $marker=if($persistentCross){'persistent-settled-return.txt'}else{'persistent-settled.txt'}
    if(!(Test-Path -LiteralPath (Join-Path $run $marker)) -or
       $core.persistent_host_changes -ne ($changes*[int](!$guest)) -or $core.persistent_guest_changes -ne ($changes*[int]$guest) -or
       $core.rooms_started -ne 1 -or $core.rooms_stopped -ne 1){
     throw "$role failed stock map reload with the original transport room"
    }
    if($guest -and $cores.host.world_epoch -ne $core.world_epoch){throw "$role did not agree on the new world epoch"}
   }
   if($worldMusic){
    $guest=$role -ne 'host'
    foreach($proof in 'music-boss.txt','music-area.txt','music-clean.txt'){
     if(!(Test-Path -LiteralPath (Join-Path $run $proof))){throw "$role music verification missing $proof"}
    }
    if(($guest -and $core.world_music_changes -lt 2) -or
       (!$guest -and $core.world_music_changes -ne 0) -or $core.world_music_restored -ne [int]$guest){
     throw "$role failed native music scope or cleanup verification"
    }
   }
   if($worldStory){
    $expected=if($role -eq 'client2'){1}else{2}
    foreach($proof in 'story-duck-unclaimed.txt','story-duck-claimed.txt','story-duck-consumed.txt','story-duck-repaired.txt'){
     if(!(Test-Path -LiteralPath (Join-Path $run $proof))){throw "$role story reward verification missing $proof"}
    }
    if($core.story_duck_grants -ne $expected){throw "$role did not keep personal Stone Duck quantities"}
   }
   if($worldLibrary){
    $guest=$role -ne 'host'
    if(($core.world_library_locks -gt 0) -ne $guest -or ($core.world_library_restores -gt 0) -ne $guest -or
       $core.world_library_gate_locks -ne [int]$guest -or $core.world_progress_errors -ne 0){
     throw "$role failed scoped library book and gate verification"
    }
    foreach($proof in 'library-book-ready.txt','library-book-restored.txt','library-book-clean.txt'){
     if(!(Test-Path -LiteralPath (Join-Path $run $proof) -PathType Leaf)){throw "$role missing $proof"}
    }
   }
   if($worldElevator){
    $guest=$role -ne 'host'
    if($core.world_elevator_moves -ne 2*[int]$guest -or $core.world_elevator_leases -le 0 -or
       $core.world_elevator_leases -ne $core.world_elevator_releases -or $core.world_progress_errors -ne 0){
     throw "$role failed elevator travel or busy lever cleanup"
    }
    foreach($proof in 'elevator-up-running.txt','elevator-stale-held.txt','elevator-up-arrived.txt','elevator-down-running.txt','elevator-down-arrived.txt'){
     if(!(Test-Path -LiteralPath (Join-Path $run $proof) -PathType Leaf)){throw "$role missing $proof"}
    }
   }
   if($elevatorGuest){
    if($core.elevator_host_accepts -ne 2*[int]($role -eq 'host') -or
       $core.elevator_host_refusals -ne [int]($role -eq 'host') -or
       $core.elevator_guest_accepts -ne 2*[int]($role -eq 'client') -or
       $core.elevator_guest_refusals -ne [int]($role -eq 'client2') -or
       $core.elevator_intents -ne (2*[int]($role -eq 'client')+[int]($role -eq 'client2')) -or
       $core.world_elevator_leases -ne $core.world_elevator_releases -or $core.world_progress_errors -ne 0){
     throw "$role failed elevator intent, proximity, or echo verification"
    }
    foreach($proof in 'elevator-intent-refused.txt','elevator-intent-up.txt','elevator-intent-down.txt'){
     if(!(Test-Path -LiteralPath (Join-Path $run $proof) -PathType Leaf)){throw "$role missing $proof"}
    }
   }
   if($arrivalLab){
    $guest=$role -ne 'host'
    $cancelled=$arrivalCancelLab -and $role -eq 'client3'
    if(!(Test-Path -LiteralPath (Join-Path $run 'arrival-verified.txt') -PathType Leaf) -or
       $core.world_arrival_published -ne [int](!$guest) -or
       $core.world_arrival_applied -ne [int]($guest -and !$cancelled) -or $core.world_arrival_cancelled -ne [int]$cancelled -or
       $core.world_stream_restores -ne ([int]$guest+[int]$cancelled)){
     throw "$role failed stock stream-before-arrival verification"
    }
   }
   if($travelGuardLab){
    $expectedGuards=3*[int]($role -ne 'host')
    if(!(Test-Path -LiteralPath (Join-Path $run 'travel-guard.txt') -PathType Leaf) -or
       !(Test-Path -LiteralPath (Join-Path $run 'travel-guard-clean.txt') -PathType Leaf) -or
       $core.travel_guards_installed -ne $expectedGuards -or $core.travel_guards_restored -ne $expectedGuards){
     throw "$role failed the three stock guest travel guards"
    }
   }
   if($threatMemoryLab){
    if(!(Test-Path -LiteralPath (Join-Path $run 'damage-clean.txt') -PathType Leaf) -or
       ($role -eq 'host' -and (!(Test-Path -LiteralPath (Join-Path $run 'threat-memory-used.txt') -PathType Leaf) -or
        !(Test-Path -LiteralPath (Join-Path $run 'threat-memory-expired.txt') -PathType Leaf) -or
        !(Test-Path -LiteralPath (Join-Path $run 'threat-clean.txt') -PathType Leaf) -or
        $core.threat_memory_created -lt 1 -or $core.threat_memory_used -lt 2 -or
        $core.threat_memory_retired -ne $core.threat_memory_created))){
     throw "$role failed last-visible enemy chase verification"
    }
   }elseif($threatLab){
    if(!(Test-Path -LiteralPath (Join-Path $run 'threat-hit.txt') -PathType Leaf) -or
       !(Test-Path -LiteralPath (Join-Path $run 'damage-clean.txt') -PathType Leaf) -or
       ($role -eq 'host' -and (!(Test-Path -LiteralPath (Join-Path $run 'threat-clean.txt') -PathType Leaf) -or
        $core.threat_switches -lt 1 -or $core.threat_restores -lt 1 -or
        $core.damage_sent -lt 1 -or $core.damage_acked -lt 1)) -or
       ($role -eq 'client2' -and $core.damage_applied -lt 1) -or
       ($role -notin @('client2','host') -and $core.damage_applied -ne 0)){
     throw "$role failed native enemy target and attack verification"
    }
   }
   if($enemyDamageLab){
    $sender=[int]($role -eq 'host')
    $receiver=[int]($role -eq 'client2')
    if(!(Test-Path -LiteralPath (Join-Path $run 'enemy-damage-hit.txt') -PathType Leaf) -or
       !(Test-Path -LiteralPath (Join-Path $run 'damage-clean.txt') -PathType Leaf) -or
       ($sender -and (!(Test-Path -LiteralPath (Join-Path $run 'enemy-proxy-game-state.txt') -PathType Leaf) -or
                     !(Test-Path -LiteralPath (Join-Path $run 'enemy-proxy-stale.txt') -PathType Leaf))) -or
       $core.damage_applied -ne $receiver -or $core.damage_sent -ne $sender -or $core.damage_acked -ne $sender -or
       $core.damage_proxies -ne (3*$sender) -or $core.combat_proxies_armed -ne (3*$sender) -or
       $core.combat_proxies_registered -ne $core.combat_proxies_unregistered -or $core.damage_rejected -ne 0){
     throw "$role failed targeted native enemy damage"
    }
   }
   if($damageLab){
    $guest=[int]($role -ne 'host')
    if(!(Test-Path -LiteralPath (Join-Path $run 'damage-three.txt') -PathType Leaf) -or
       !(Test-Path -LiteralPath (Join-Path $run 'damage-clean.txt') -PathType Leaf) -or
       $core.damage_applied -ne (3*(1-$guest)) -or $core.damage_proxies -ne (3*(1-$guest)) -or
       $core.damage_sent -ne $guest -or $core.damage_acked -ne $guest -or
       $core.damage_suppressed -ne $guest -or $core.damage_rejected -ne 0){
     throw "$role failed native guest damage authority verification"
    }
   }
   if($worldChest){
    if(!(Test-Path -LiteralPath (Join-Path $run 'world-chest-open.txt') -PathType Leaf) -or
       ($role -eq 'host' -and $core.world_actor_chests -ne 0) -or
       ($role -ne 'host' -and ($core.world_actor_restores -ne 1 -or $core.world_actor_chests -ne 1))){
     throw "$role failed stock chest reconciliation verification"
    }
   }
   if($worldMechanism){
    if(!(Test-Path -LiteralPath (Join-Path $run 'world-mechanism-closed.txt') -PathType Leaf) -or
       ($role -eq 'host' -and ($core.world_mechanism_doors -ne 0 -or $core.world_mechanism_levers -ne 0)) -or
       ($role -ne 'host' -and ($core.world_mechanism_doors -lt 2 -or $core.world_mechanism_levers -lt 2))){
     throw "$role failed stock door and lever reconciliation verification"
    }
   }
   if($shrineGate){
    foreach($state in 'ready','open','closed'){
     if(!(Test-Path -LiteralPath (Join-Path $run "shrine-gate-$state.txt") -PathType Leaf)){
      throw "$role failed shrine timeline and collision verification at $state"
     }
    }
   }
   if($castleGate){
    if(($role -eq 'host' -and $core.world_mechanism_doors -ne 0) -or
       ($role -ne 'host' -and $core.world_mechanism_doors -lt 4)){
     throw "$role failed castle gate replication counts"
    }
    foreach($state in 'ready','open','closed'){
     if(!(Test-Path -LiteralPath (Join-Path $run "castle-gate-$state.txt") -PathType Leaf)){
      throw "$role missing castle gate physical and timeline proof at $state"
     }
    }
   }
   if($worldBarrier){
    if(($role -eq 'host' -and ($core.world_barrier_captured -lt 2 -or $core.world_barrier_applied -ne 0)) -or
       ($role -ne 'host' -and $core.world_barrier_applied -lt 4)){
     throw "$role failed independent blocker replication"
    }
    foreach($phase in 'ready','changed','restored'){
     if(!(Test-Path -LiteralPath (Join-Path $run "barrier-$phase.txt") -PathType Leaf)){
      throw "$role missing stock blocker proof at $phase"
     }
    }
   }
   if($worldArena){
    if(($role -eq 'host' -and ($core.world_arena_captured -lt 2 -or $core.world_arena_applied -ne 0)) -or
       ($role -ne 'host' -and ($core.world_arena_applied -lt 6 -or $core.world_arena_necro_material -lt 2))){
     throw "$role failed arena state and material replication"
    }
    foreach($phase in 'ready','changed','masked','restored'){
     if(!(Test-Path -LiteralPath (Join-Path $run "arena-$phase.txt") -PathType Leaf)){
      throw "$role missing original arena barrier proof at $phase"
     }
    }
   }
   if($worldCutscene){
    if($core.cine_starts -ne 1 -or $core.cine_plays -ne 1 -or $core.cine_broadcasts -ne 1 -or $core.cine_commits -ne 1 -or $core.cine_errors -ne 0 -or
       $core.cine_callbacks -ne [int]($role -eq 'host') -or
       ($role -ne 'host' -and $core.cine_skips -lt 1)){
     throw "$role failed native cutscene playback or completion authority"
    }
    foreach($proof in @('playing','sample','restored')+@(if($role -ne 'host'){'local-skip'})){
     if(!(Test-Path -LiteralPath (Join-Path $run "cine-$proof.txt") -PathType Leaf)){throw "$role missing original sequence proof at $proof"}
    }
   }
   if($worldBoss){
    if(($role -eq 'host' -and $core.boss_phase_sent -lt 2) -or
       ($role -ne 'host' -and ($core.boss_phase_received -lt 2 -or $core.boss_phase_applied -lt 2 -or
       $core.boss_phase_interactable -ne 1 -or $core.death_applied -ne 0))){throw "$role failed native Boss phase authority"}
    foreach($proof in 'ready','defeated'){
     if(!(Test-Path -LiteralPath (Join-Path $run "boss-phase-$proof.txt") -PathType Leaf)){throw "$role missing Boss phase proof at $proof"}
    }
   }
   if($worldBossInteract){
    $expected=[int]($role -eq 'host')
    $proof=Get-Content -LiteralPath (Join-Path $run 'boss-interact-result.txt') -Raw
    if($proof -notmatch "box=0 original_dialogue_actor=$expected" -or
       $core.boss_interact_host_accepts -ne $expected -or $core.boss_interact_host_refusals -ne $expected -or
       $core.boss_interact_guest_accepts -ne [int]($role -eq 'client') -or
       $core.boss_interact_guest_refusals -ne [int]($role -eq 'client2') -or
       $core.boss_interact_intercepted -ne $(if($role -eq 'client'){2}elseif($role -eq 'client2'){1}else{0}) -or
       $core.boss_interact_requests -ne [int]($role -in @('client','client2'))){throw "$role failed original Boss interaction authority"}
   }
   if($worldLan){
    $active=Get-Content -LiteralPath (Join-Path $run 'lan-active.txt') -Raw
    $mask=if($role -eq 'host'){7}else{1}
    if($active -notmatch "slot=([0-3]) seats=4 connected=3 mask=$mask epoch=1 scope=([0-9a-f]+\.[0-9a-f]+)"){
     throw "$role LAN handshake proof missing"
    }
    $lanFirstSlots[$role]=[int]$Matches[1]
    $lanFirstScopes[$role]=$Matches[2]
    $active=Get-Content -LiteralPath (Join-Path $run 'epoch2-lan-active.txt') -Raw
    if($active -notmatch "slot=([0-3]) seats=4 connected=3 mask=$mask epoch=2 scope=([0-9a-f]+\.[0-9a-f]+)"){
     throw "$role LAN rejoin handshake proof missing"
    }
    $lanSlots[$role]=[int]$Matches[1]
    if($lanSlots[$role] -ne $ownSlot){throw "$role LAN assignment does not match the player proof"}
    $lanScopes[$role]=$Matches[2]
    $departure=Get-Content -LiteralPath (Join-Path $run 'lan-departure.txt') -Raw
    $expectedDeparture=if($role -eq 'host'){'phase=3 failure=0'}else{'phase=4 failure=6'}
    if($departure -notmatch $expectedDeparture){throw "$role LAN departure cleanup failed"}
    $proof=if($role -eq 'client3'){'lan-rejoined.txt'}else{'lan-recovered.txt'}
    if(!(Test-Path -LiteralPath (Join-Path $run $proof)) -or
       $core.recoveries -ne [int]($role -ne 'client3') -or $core.rooms_started -ne (1+[int]($role -eq 'client3'))){
     throw "$role LAN recovery proof incomplete"
    }
   }
   if($playerBodyFallback -and ($core.player_graph_bound -ne 0 -or $core.player_asset_stage -ne 5)){
     throw "$role failed stock presentation fallback with missing compression resources"
   }
   if($playerRescue -and !$medicInterrupt){
    if($core.life_stage -ne 8 -or $core.life_restored -ne 1 -or
       ($role -in @('host','client2') -and ($core.life_intercepted -ne 1 -or $core.life_downed -ne 1 -or $core.life_revived -ne 1)) -or
       ($role -in @('host','client') -and $core.life_requests -lt 1) -or
       !(Test-Path (Join-Path $run 'rescue-a-ok.txt')) -or !(Test-Path (Join-Path $run 'rescue-b-ok.txt'))){
     throw "$role failed native downed and rescue verification"
    }
   }
   if($playerMedic -and !$medicInterrupt -and ($core.medic_errors -ne 0 -or $core.medic_refunds -ne 0 -or
      $core.medic_debits -ne [int]($role -in @('host','client')) -or
      $core.medic_revives -ne [int]($role -in @('host','client2')))){
     throw "$role failed native medical receipt verification"
   }
   if($playerPresentation){
    $lookTarget=$role -in @('host','client2')
    $hudDonor=$role -in @('host','client')
    if($core.view_look -ne (4*[int]$lookTarget) -or
       ($lookTarget -and ($core.view_camera -le 0 -or $core.view_camera_ticks -le 0)) -or
       ($hudDonor -and ($core.view_highlight -le 0 -or $core.view_power -le 0)) -or
       !(Test-Path (Join-Path $run 'presentation-a.txt')) -or
       !(Test-Path (Join-Path $run 'presentation-b.txt')) -or
       !(Test-Path (Join-Path $run 'presentation-clean.txt'))){
      throw "$role failed native rescue presentation verification"
    }
   }
   if($medicInterrupt -and ($core.medic_errors -ne 0 -or $core.medic_refunds -ne 0 -or
       $core.medic_debits -ne [int]($role -eq 'client') -or $core.medic_revives -ne 0 -or
       $core.life_downed -ne [int]($role -eq 'host') -or !(Test-Path (Join-Path $run 'medic-interrupt-ok.txt')))){
     throw "$role failed pre-application interruption verification"
   }
   if($medicReplay -and ($core.medic_errors -ne 0 -or $core.medic_refunds -ne [int]($refundReplay -and $role -eq 'client') -or
        !(Test-Path (Join-Path $run 'medic-replay.txt')))){
     throw "$role failed native medicine replay verification"
   }
   if($playerGear -and (!$player.gear_verified -or $core.player_equipment_stage -ne 3 -or
       $core.player_skin_changed -lt 6 -or $core.player_equipped -lt 6 -or
       $core.player_parts_created -lt 6 -or $core.player_parts_removed -lt 6)){
     throw "$role failed native equipment and skin verification"
   }
   if($playerBones -and (!$player.bone_verified -or $core.gear_pose_stage -ne 4 -or
       $core.gear_pose_sent -le 0 -or $core.gear_pose_applied -le 0 -or $core.gear_pose_writes -le 0 -or
       ($role -ne 'host' -and ($player.bone_sequences[0] -le 0 -or
        $player.bone_sequences[1] -le $player.bone_sequences[0])))){
     throw "$role failed skeletal weapon pose and offhand verification"
   }
   $sourceSaves=Join-Path $PSScriptRoot 'profile/Saved/SaveGames'
   $runSaves=Join-Path $run 'profile/Saved/SaveGames'
   $expected=@(Get-ChildItem -LiteralPath $sourceSaves -File -Recurse)
   $nativeRescueLedgers=@(Get-ChildItem -LiteralPath $runSaves -File|Where-Object Name -Match '^World\d+\.sav\.qdrescue-native-v1(-[a-f0-9]{32})?$')
   if($nativeRescueLedgers.Count -gt (1+[int]$checkpointReopened) -or
      (($playerMedic -or $medicReplay) -and $nativeRescueLedgers.Count -ne 1) -or
      ($checkpointReopened -and $nativeRescueLedgers.Count -ne 2)){
    throw "$role unexpected number of native rescue ledgers"
   }
   $checkpoints=@(Get-ChildItem -LiteralPath $runSaves -File|Where-Object Name -Match '^World\d+\.sav\.qdc-[a-f0-9]{32}-[a-f0-9]{32}\.(sav|commit)$')
   if($playerCheckpoint){
    $commits=1+[int]$checkpointRefresh+[int]$checkpointReopened
    $completed=@(if($checkpointRefresh){'checkpoint-base.txt';'checkpoint-refresh.txt'}else{'checkpoint-base.txt'})
    if($core.cp_commits -ne $commits -or $core.cp_archives -ne $commits -or
       (($core.cp_errors -ne 0) -ne $checkpointWipeMissing) -or
       $core.cp_stock -ne [int]$checkpointRefresh -or $checkpoints.Count -ne 2*$commits -or
       !(Test-Path -LiteralPath (Join-Path $run 'checkpoint-clean.txt') -PathType Leaf)){
     throw "$role failed shared checkpoint and stock hook cleanup verification"
    }
    $checkpointReports[$role]=@{}
    foreach($name in $completed){
     $proof=(Get-Content -LiteralPath (Join-Path $run $name) -Raw).Trim()
     if($proof -notmatch '^token=([a-f0-9]{16}) slot=([0-3]) serial=([12]) '){throw "$role invalid $name evidence"}
     $checkpointReports[$role][$name]=@{token=$Matches[1];slot=[int]$Matches[2];serial=[int]$Matches[3];proof=$proof}
    }
   }
   $sidecars=@($nativeRescueLedgers)+@($checkpoints)
   if($checkpointRefresh -or $guestSafeSave){
    $stockBackup=Get-Item -LiteralPath (Join-Path $runSaves 'World1_backup.sav')
    if((SaveSha256 $stockBackup.FullName) -ne (SaveSha256 (Join-Path $runSaves 'World1.sav'))){
     throw "$role stock backup differs from its refreshed primary"
    }
    $sidecars+=@($stockBackup)
   }
   if($guestSafeSave){
    if(($core.guest_safe_save_writes -gt 0) -ne ($role -ne 'host') -or
       !(Test-Path -LiteralPath (Join-Path $run 'guest-safe-save.txt')) -or
       ($role -ne 'host' -and !(Test-Path -LiteralPath (Join-Path $run 'guest-safe-reload.sav'))) -or
       !(Test-Path -LiteralPath (Join-Path $run 'guest-safe-save-regular.txt'))){
     throw "$role guest safe save missing stock write or read-back proof"
    }
   }
   if($checkpointWipe){
    $wipeRecords=@(Get-ChildItem -LiteralPath $runSaves -File|Where-Object Name -Match '^World\d+\.sav\.qdc-[a-f0-9]{32}-[a-f0-9]{32}\.wipe$')
    if($wipeRecords.Count -ne [int]$checkpointReopened){throw "$role unexpected durable rollback authorization count"}
    $sidecars+=@($wipeRecords)
    if($checkpointReopened){
     $proof=(Get-Content -LiteralPath (Join-Path $run 'checkpoint-wipe.txt') -Raw).Trim()
     if($proof -notmatch '^nonce=([a-f0-9]{16}) slot=([0-3]) '){throw "$role invalid rollback evidence"}
     $checkpointReports[$role]['checkpoint-wipe.txt']=@{token=$Matches[1];slot=[int]$Matches[2];proof=$proof}
    }elseif(!(Test-Path -LiteralPath (Join-Path $run 'checkpoint-wipe-refused.txt') -PathType Leaf)){
     throw "$role did not refuse rollback with a missing archive"
    }
   }
   $actual=@(Get-ChildItem -LiteralPath $runSaves -File -Recurse|Where-Object FullName -NotIn $sidecars.FullName)
   if($expected.Count -ne $actual.Count){throw "$role save count changed"}
   $changed=@()
   foreach($file in $expected){
    $relative=$file.FullName.Substring($sourceSaves.Length+1)
    $target=Join-Path $runSaves $relative
    $before=SaveSha256 $file.FullName
    $after=SaveSha256 $target
    if($before -eq $after){continue}
    # The native equip fixture can update weapon collection challenges.
    # The stock checkpoint unlock control writes these exact challenge files.
    $storyChallenge=$worldStory -and $relative -in @('Challenges','Challenges_Backup') -and
       $after -eq 'FC-65-0A-38-3A-9A-93-90-77-80-57-58-66-3E-60-CC-89-13-FA-F0-F1-9B-87-E3-58-68-63-B3-1C-43-26-C0'
    if(!($playerGear -and $relative -in @('Challenges','Challenges_Backup')) -and
       !(($checkpointRefresh -or $guestSafeSave) -and $relative -eq 'World1.sav') -and !$storyChallenge){
     throw "$role save changed at $relative"
    }
    Copy-Item -LiteralPath $file.FullName -Destination $target -Force
    if((SaveSha256 $target) -ne $before){throw "$role fixture restore failed at $relative"}
    $changed+=@(@{file=$relative;before=$before;after=$after;restored=$true})
   }
   $saveDifferences[$role]=$changed
   $playerReports[$role]=$player
   $worlds[$role]=$world
   $cores[$role]=$core
   continue
  }
  if($recoveryLab){
   $recovery=Get-Content (Join-Path $run 'native-recovery.json') -Raw|ConvertFrom-Json
   if(!$world.passed -or $world.ue4ss_loaded -or $world.wrong_thread -ne 0 -or $core.wrong_thread -ne 0 -or
      $world.metadata_found -ne 42 -or !$recovery.detached -or !$recovery.sources_alive -or
      !$recovery.final_clean -or !$recovery.same_lobby -or $recovery.first_epoch -ne 1 -or
      $recovery.second_epoch -ne 2 -or $recovery.vacant_members -ne 3 -or $recovery.new_generation -ne 1 -or
      $core.rooms_started -ne 1 -or $core.rooms_stopped -ne 1 -or $core.recoveries -ne 1 -or
      $core.auto_enrolled -ne 2 -or $core.enroll_acked -ne 6 -or $core.kill_restored -ne 2 -or
      $core.kill_restore_conflicts -ne 0 -or $core.active -ne 0 -or $core.retire_destroyed -ne 0){
    throw 'Isolated engine recovery did not pass'
   }
   @{passed=$true;syntheticSteam=$true;gamePids=$gamePids;world=$world;core=$core;recovery=$recovery}|
    ConvertTo-Json -Depth 6|Set-Content (Join-Path $runRoot 'summary.json')
   return
  }
  if($steamSmoke){
   $steam=Get-Content (Join-Path $run 'native-steam.json') -Raw|ConvertFrom-Json
   if(!$world.passed -or $world.ue4ss_loaded -or $world.wrong_thread -ne 0 -or $world.metadata_found -ne 42 -or
      !$steam.module_loaded -or !$steam.lobby_seen -or !$steam.nonce_seen -or !$steam.invalid_rejected -or
      $steam.phase -ne 5 -or $steam.failure -ne 0 -or $steam.opened_members -ne 1 -or
      $steam.opened_limit -ne 4 -or $steam.expected -ne 4 -or $steam.left -ne 1 -or
      $steam.rooms_opened -ne 2 -or $steam.rooms_left -ne 2 -or !$steam.rooms_distinct -or
      $steam.closed_join_response -ne 2){
    throw 'Native Steam private room did not complete create/leave verification'
   }
   @{passed=$true;gamePids=$gamePids;world=$world;core=$core;steam=$steam}|
    ConvertTo-Json -Depth 5|Set-Content (Join-Path $runRoot 'summary.json')
   return
  }
  if($automatic){
   $auto=Get-Content (Join-Path $run 'native-auto.json') -Raw|ConvertFrom-Json
   if(!$world.passed -or $world.role -ne $role -or $world.ue4ss_loaded -or $world.metadata_found -ne 42 -or
      $world.wrong_thread -ne 0 -or $core.wrong_thread -ne 0 -or $core.build -ne 1 -or
      $core.socket_errors -ne 0 -or $core.worker_drops -ne 0 -or $core.queue_drops -ne 0 -or
      $core.enroll_rejected -ne 0 -or $core.retire_rejected -ne 0 -or $core.factory_failed -ne 0 -or
      $core.factory_unloaded -ne 0 -or !$auto.enabled -or !$auto.initial_verified -or
      !$auto.replacement_verified -or !$auto.old_gone -or !$auto.stop_clean -or
      $auto.initial.Count -ne 4 -or $auto.replacement.generation -ne 2 -or
      $auto.replacement.actor -eq $auto.initial[0].actor -or
      @($auto.initial.actor|Sort-Object -Unique).Count -ne 4){
    throw "Automatic native world evidence failed for $role"
   }
   $worlds[$role]=$world
   $cores[$role]=$core
   $automaticReports[$role]=$auto
   continue
  }
  if(!$world.passed -or $world.role -ne $role -or $world.ue4ss_loaded -or
     $world.client_count -ne $clientCount -or $world.port -ne (49861+$index) -or
     $world.wrong_thread -ne 0 -or $world.metadata_found -ne 42 -or $core.active -ne 0 -or
     $core.build -ne 1 -or $core.thread -ne $world.game_thread -or
     $core.wrong_thread -ne 0 -or $core.socket_errors -ne 0 -or $core.worker_drops -ne 0 -or
     $core.queue_drops -ne 0 -or $core.expired -ne 0 -or $core.replaced -ne 0 -or
     $world.animation_methods -ne 6 -or $world.animation_fixture_step -ne 5 -or !$world.animation_stopped -or
     $world.catalog_profile -ne 31 -or $world.catalog_scan_slices -le 1 -or
     $core.anim_catalog_passes -ne 1 -or $core.anim_catalog_slots -le 2048 -or
     $core.anim_catalog_binds -ne 2 -or $core.anim_catalog_montages -lt 2 -or $core.anim_catalog_sequences -lt 2 -or
     $core.anim_rejected -ne 0 -or $core.anim_missing -ne 0 -or !$world.native_enrolled -or
     $core.enroll_rejected -ne 0 -or $core.retire_rejected -ne 0 -or
     !$world.first_clean -or !$world.reuse_enrolled -or !$world.reuse_verified -or !$world.second_clean -or
     !$world.old_generation_rejected -or $world.last_generation -ne 2 -or
     !$world.reuse_actor -or $world.reuse_actor -eq $world.actor -or $world.reuse_hp -ne 50){
   throw "Native world evidence failed for $role"
  }
  $worlds[$role]=$world
  $cores[$role]=$core
 }
 if(@($gamePids|Sort-Object -Unique).Count -ne $roles.Count){throw 'Expected one distinct game process per role'}
 if($players){
  if($worldLan -and (@($lanSlots.Values|Sort-Object -Unique).Count -ne $roles.Count -or $lanSlots.host -ne 0 -or
     @($lanFirstSlots.Values|Sort-Object -Unique).Count -ne $roles.Count -or $lanFirstSlots.host -ne 0 -or
     @($lanScopes.Values|Sort-Object -Unique).Count -ne 1 -or @($lanFirstScopes.Values|Sort-Object -Unique).Count -ne 1 -or
     $lanScopes.host -eq $lanFirstScopes.host)){throw 'LAN peers disagreed on slots or room scopes'}
  if($worldCutscene){
   $anchors=@(foreach($role in $roles){
    $line=Get-Content -LiteralPath (Join-Path $runRoot "$role/cine-sample.txt") -Raw
    if($line -notmatch 'playing=1 seconds=(\d+\.\d+) tick_ms=(\d+)'){throw "$role missing playback clock"}
    [double]$Matches[2]/1000-[double]$Matches[1]
   })
   $spread=($anchors|Measure-Object -Maximum).Maximum-($anchors|Measure-Object -Minimum).Minimum
   if($spread -gt .400){throw "Cutscene playback clock spread $spread seconds"}
  }
  if($hitboxLab -and @($weaponResults.Values|Sort-Object -Unique).Count -ne 1){throw 'The weapon hit did not converge on all peers'}
  if($playerCheckpoint){
   foreach($name in $checkpointReports['host'].Keys){
    $proofs=@(foreach($role in $roles){$checkpointReports[$role][$name]})
    if(@($proofs.token|Sort-Object -Unique).Count -ne 1 -or
       @($proofs.slot|Sort-Object -Unique).Count -ne $roles.Count){throw "Peers disagreed on $name"}
   }
   if($checkpointRefresh -and $checkpointReports['host']['checkpoint-base.txt'].token -eq
      $checkpointReports['host']['checkpoint-refresh.txt'].token){throw 'Refresh reused the original checkpoint token'}
  }
  @{passed=$true;roles=$roles;gamePids=$gamePids;world=$worlds;core=$cores;players=$playerReports;saveFiles=$expected.Count;saveDifferences=$saveDifferences;lan=$worldLan;lanSlots=$lanSlots;playerBody=$playerBody;playerBodyFallback=$playerBodyFallback;playerGear=$playerGear;playerBones=$playerBones;playerRescue=$playerRescue;playerMedic=$playerMedic;playerPresentation=$playerPresentation;playerMedicInterrupt=$medicInterrupt;medicReplay=[bool]$medicReplay;medicRefundReplay=$refundReplay;playerCheckpoint=$playerCheckpoint;checkpointRefresh=$checkpointRefresh;checkpointWipe=$checkpointWipe;checkpointWipeMissing=$checkpointWipeMissing;checkpoints=$checkpointReports}|
   ConvertTo-Json -Depth 8|Set-Content (Join-Path $runRoot 'summary.json')
  return
 }
 $hostWorld=$worlds.host
 $hostCore=$cores.host
 if($automatic){
  $hostAuto=$automaticReports.host
  if($hostCore.auto_enrolled -ne 5 -or $hostCore.active -ne 0 -or $hostCore.enum_calls -lt 3 -or
     $hostCore.enum_frees -ne $hostCore.enum_calls -or $hostCore.enroll_acked -ne (5*$clientCount) -or
     $hostCore.retire_automatic -ne 1 -or $hostCore.retire_acked -ne $clientCount -or
     $hostCore.kill_restored -ne 1 -or $hostCore.retire_destroyed -ne $(if($reopen){2}else{0}) -or
     $hostCore.kill_queued -ne 1 -or $hostCore.death_acks -ne $clientCount -or $hostCore.death_pending -ne 0 -or
     [uint64]$hostCore.world_epoch -le 1 -or $hostCore.world_configure_stage -ne 6){
   throw 'Automatic host discovery, death or reuse failed'
  }
  foreach($role in $roles|Where-Object {$_ -ne 'host'}){
   $core=$cores[$role]
   $auto=$automaticReports[$role]
   $extra=if($reopen -and $role -ne 'client'){1}else{0}
   if($core.factory_created -ne (5+$extra) -or $core.active -ne 0 -or $core.ownership_taken -ne (5+$extra) -or
      $core.retire_applied -ne 1 -or $core.retire_destroyed -lt (9+2*$extra) -or $core.retire_destroyed -gt (10+2*$extra) -or
      $core.enroll_confirmed -ne (5+$extra) -or $core.anim_applied -le 0 -or $core.death_applied -ne 1 -or
      $core.world_epoch -ne $hostCore.world_epoch -or $core.world_configure_stage -ne 6){
    throw "$role automatic factory or cleanup failed"
   }
   $hostSamples=@($hostAuto.initial)+@($hostAuto.replacement)
   $guestSamples=@($auto.initial)+@($auto.replacement)
   for($i=0;$i -lt $hostSamples.Count;++$i){
    $expected=$hostSamples[$i]
    $actual=$guestSamples[$i]
    if($expected.generation -ne $actual.generation -or $expected.hp -ne $actual.hp){
     throw "$role did not apply host generation or HP"
    }
    for($axis=0;$axis -lt 3;++$axis){
     if([math]::Abs($expected.position[$axis]-$actual.position[$axis]) -gt 0.02 -or
        [math]::Abs($expected.mesh[$axis]-$actual.mesh[$axis]) -gt 0.02){
      throw "$role did not apply host transform"
     }
    }
   }
  }
  if($reopen){
   foreach($role in $roles){
    $core=$cores[$role]
    $auto=$automaticReports[$role]
    $authority=$role -eq 'client'
    if($core.rooms_started -ne 2 -or $core.rooms_stopped -ne 2 -or $core.active -ne 0 -or
       !$auto.live_start_rejected -or !$auto.old_start_rejected -or !$auto.first_epoch_clean -or !$auto.second_epoch_clean -or
       $auto.reopen_authority -ne $authority -or $auto.old_offer_sent -ne $authority -or
       $auto.reopen_generation -ne 1 -or $auto.reopen_hp -ne 103.5 -or
       $auto.old_scope_dropped -ne $(if($authority){0}else{1})){
     throw "$role played-room teardown or fresh-generation verification failed"
    }
   }
   if($cores.client.auto_enrolled -ne 1 -or $cores.client.enroll_acked -ne $clientCount -or
      $cores.client.kill_restored -ne 1 -or $hostCore.factory_created -ne 1 -or $hostCore.enroll_confirmed -ne 1){
    throw 'Guest-to-host or host-to-guest role change failed'
   }
  }
  @{passed=$true;reopen=$reopen;roles=$roles;gamePids=$gamePids;clientCount=$clientCount;world=$worlds;core=$cores;automatic=$automaticReports}|
   ConvertTo-Json -Depth 7|Set-Content (Join-Path $runRoot 'summary.json')
  return
 }
 if(!$hostWorld.native_natural -or $hostWorld.native_candidates -lt 2 -or
    $hostCore.enum_calls -ne 4 -or $hostCore.enum_frees -ne $hostCore.enum_calls -or
    $hostCore.enroll_acked -ne (2*$clientCount) -or ($clientCount -eq 3 -and $hostCore.enroll_sent -lt 4) -or
    !$hostWorld.early_reuse_rejected -or !$hostWorld.natural_reuse_alive -or
    $hostCore.retire_acked -ne (2*$clientCount) -or $hostCore.retire_deferred -ne 1 -or
    $hostCore.retire_automatic -ne 0 -or $hostCore.retire_sent -lt 2 -or $hostCore.retire_destroyed -ne 0){
  throw 'Host natural enumeration or enrollment failed'
 }
 if($hostCore.captured -lt 15 -or $hostCore.tx -lt (15*$clientCount) -or !$hostWorld.second_marker_sent -or
    $hostCore.anim_captured -lt 15 -or !$hostWorld.animation_started -or
    !$hostWorld.death_published -or $hostCore.death_acks -lt $clientCount -or $hostCore.death_pending -ne 0 -or
    !$hostWorld.kill_observer_installed -or !$hostWorld.kill_slot_restored -or
    $hostCore.kill_hook -ne 0 -or $hostCore.kill_queued -ne 1 -or $hostCore.kill_ignored -ne 0 -or
    $hostCore.kill_observed -ne 1 -or $hostCore.kill_forwarded -ne 1 -or
    $hostCore.kill_restored -ne 1 -or $hostCore.kill_restore_conflicts -ne 0 -or
    ($clientCount -eq 3 -and $hostCore.death_sends -lt 2)){
  throw 'Host capture or fanout evidence failed'
 }
 foreach($role in $roles|Where-Object {$_ -ne 'host'}){
  $world=$worlds[$role]
  $core=$cores[$role]
  if($core.applied -le 0 -or $core.rx -lt 15 -or !$world.first_snapshot_seen -or
     $world.hp_after -ne $hostWorld.hp_after -or $world.hp_after -ne 45.5 -or
     $world.hp_before -eq $hostWorld.hp_before -or $core.death_applied -ne 1 -or
     !$world.offer_confirmed -or !$world.stale_offer_rejected -or $world.native_natural -or
     $world.offer_class -ne $hostWorld.offer_class -or $world.offer_mesh -ne $hostWorld.offer_mesh -or
     $core.enroll_received -ne 2 -or $core.enroll_confirmed -ne 2 -or $core.retire_applied -ne 2 -or
     $core.retire_destroyed -lt 3 -or $core.retire_destroyed -gt 4 -or
     ($clientCount -eq 3 -and $role -eq 'client3' -and $core.retire_duplicates -lt 1) -or
     !$world.display_hidden -or (!$world.death_hp_zero -and !$world.death_actor_expired) -or
     !$world.ownership_restored -or !$world.stale_release_rejected -or
     $world.ownership_fixture_step -ne 6 -or $world.ownership_before -ne $world.ownership_after -or
     $world.ownership_held -ne 3 -or $world.dilation_before -ne $world.dilation_after -or
     $world.dilation_held -ne 0 -or $core.ownership_taken -ne 3 -or
     $core.ownership_released -ne 1 -or $core.ownership_failed -ne 0 -or
     !$world.animation_first -or !$world.animation_second -or !$world.animation_advanced -or
     $world.montage_rate -ne 0.35 -or $core.anim_applied -le 0 -or $core.anim_starts -ne 1 -or $core.anim_stops -ne 1 -or
     $core.reward_applied -ne 0 -or $core.kill_hook -ne 0 -or $core.kill_queued -ne 0 -or ($clientCount -eq 3 -and $role -eq 'client3' -and $core.death_duplicates -lt 1)){
   throw "$role did not satisfy the state, ownership, animation or death checks"
  }
  for($axis=0;$axis -lt 3;++$axis){
   if([math]::Abs($world.position_after[$axis]-$hostWorld.position_after[$axis]) -gt 0.01 -or
      [math]::Abs($world.display_position[$axis]-$hostWorld.display_position[$axis]) -gt 0.01){
    throw "$role actor or mesh did not converge on its host transform"
   }
  }
  if([math]::Abs($world.position_before[0]-$world.position_after[0]) -lt 1){
   throw "$role position did not change"
  }
 }
 @{passed=$true;roles=$roles;gamePids=$gamePids;clientCount=$clientCount;world=$worlds;core=$cores}|
  ConvertTo-Json -Depth 6|Set-Content (Join-Path $runRoot 'summary.json')
}catch{
 $_|Out-String|Set-Content (Join-Path $runRoot 'runner-error.txt')
 throw
}finally{
 foreach($child in $children.Values){$child.WaitForExit()}
 foreach($pak in $stagedPaks){if(Test-Path -LiteralPath $pak){Remove-Item -LiteralPath $pak}}
 foreach($proxy in $moved){Move-Item -LiteralPath $proxy.disabled -Destination $proxy.active}
}
