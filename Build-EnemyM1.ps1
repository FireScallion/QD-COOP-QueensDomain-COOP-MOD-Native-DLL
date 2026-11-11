param(
 [Parameter(Mandatory=$true)][string]$Compiler,
 [string]$VcVars64,
 [string]$OutputDirectory=(Join-Path $PSScriptRoot 'dist\enemy-m1')
)
$ErrorActionPreference='Stop'
$Compiler=(Resolve-Path -LiteralPath $Compiler).Path

$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
$src=Join-Path $PSScriptRoot 'native'
foreach($path in @($Compiler,$OutputDirectory,$src)){
 if($path -match '["&|<>%\r\n]'){throw 'Unsupported shell metacharacter in build path'}
}
[IO.Directory]::CreateDirectory($OutputDirectory)|Out-Null
$saved=$env:PATH
try {
 $env:PATH=[IO.Path]::GetDirectoryName($Compiler)+';'+$saved
 $flags=@('-std=c++20','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-fno-exceptions','-fno-rtti','-static','-static-libgcc','-static-libstdc++','-I',(Join-Path $src 'include'))
 $core=@((Join-Path $src 'src\session.cpp'),(Join-Path $src 'src\transactions.cpp'))
 & $Compiler @flags @core (Join-Path $src 'src\api.cpp') (Join-Path $src 'src\enemy_stream.cpp') (Join-Path $src 'adapters\ue4ss\enemy_adapter.cpp') -shared '-Wl,--no-undefined' '-Wl,--exclude-all-symbols' -lws2_32 -lbcrypt -o (Join-Path $OutputDirectory 'QDCoopNativeM1.dll')
 if($LASTEXITCODE -ne 0){throw 'M1 core build failed'}
 & $Compiler @flags (Join-Path $src 'src\enemy_stream.cpp') (Join-Path $src 'tests\enemy_stream_tests.cpp') -o (Join-Path $OutputDirectory 'QDEnemyStreamTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Enemy test build failed'}
 & $Compiler @flags (Join-Path $src 'tests\dll_tests.cpp') -o (Join-Path $OutputDirectory 'QDNativeDllTests.exe')
 if($LASTEXITCODE -ne 0){throw 'DLL test build failed'}
 & $Compiler @flags @core (Join-Path $src 'tests\core_tests.cpp') -o (Join-Path $OutputDirectory 'QDNativeCoreTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Core test build failed'}
 foreach($pair in @(@('name_pool_tests.cpp','QDNamePoolTests.exe'),@('reflection_tests.cpp','QDReflectionTests.exe'),@('weak_object_tests.cpp','QDWeakObjectTests.exe'),@('animation_tests.cpp','QDAnimationTests.exe'),@('death_tests.cpp','QDDeathTests.exe'),@('reward_tests.cpp','QDRewardTests.exe'),@('object_count_map_tests.cpp','QDObjectCountMapTests.exe'),@('reward_worker_tests.cpp','QDRewardWorkerTests.exe'))){
  & $Compiler @flags (Join-Path $src ('tests\'+$pair[0])) -o (Join-Path $OutputDirectory $pair[1])
  if($LASTEXITCODE -ne 0){throw ($pair[0]+' build failed')}
 }
 & $Compiler @flags -municode (Join-Path $src 'tests\reward_journal_tests.cpp') -lbcrypt -o (Join-Path $OutputDirectory 'QDRewardJournalTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Reward journal test build failed'}
 & $Compiler @flags -municode (Join-Path $src 'tests\rescue_ledger_tests.cpp') -lbcrypt -o (Join-Path $OutputDirectory 'QDRescueLedgerTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Rescue ledger test build failed'}
 & $Compiler @flags (Join-Path $src 'tests\world_progress_tests.cpp') -o (Join-Path $OutputDirectory 'QDWorldProgressTests.exe')
 if($LASTEXITCODE -ne 0){throw 'World progress test build failed'}
 & $Compiler @flags (Join-Path $src 'tests\enemy_damage_tests.cpp') -o (Join-Path $OutputDirectory 'QDEnemyDamageTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Enemy damage test build failed'}
 & $Compiler @flags (Join-Path $src 'tests\threat_policy_tests.cpp') -o (Join-Path $OutputDirectory 'QDThreatPolicyTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Threat policy test build failed'}
 & $Compiler @flags (Join-Path $src 'adapters\windows\native_tick_bridge.cpp') -shared '-Wl,--no-undefined' '-Wl,--exclude-all-symbols' -lbcrypt -o (Join-Path $OutputDirectory 'QDEngineTickBridge.dll')
 if($LASTEXITCODE -ne 0){throw 'Native tick bridge build failed'}
 & $Compiler @flags (Join-Path $src 'adapters\windows\native_bootstrap.cpp') (Join-Path $src 'adapters\windows\native_tick_bridge.cpp') -shared '-Wl,--no-undefined' '-Wl,--exclude-all-symbols' -lbcrypt -lshell32 -o (Join-Path $OutputDirectory 'QDNativeBootstrap.dll')
 if($LASTEXITCODE -ne 0){throw 'Native bootstrap build failed'}
 & $Compiler @flags (Join-Path $src 'adapters\windows\xinput_proxy.cpp') (Join-Path $src 'adapters\windows\xinput_proxy.def') -shared '-Wl,--no-undefined' '-Wl,--exclude-all-symbols' -o (Join-Path $OutputDirectory 'xinput1_3.dll')
 if($LASTEXITCODE -ne 0){throw 'Native loader build failed'}
 & $Compiler @flags (Join-Path $src 'tests\native_room_launch_tests.cpp') -o (Join-Path $OutputDirectory 'QDNativeRoomLaunchTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Native room launch test build failed'}
 & (Join-Path $OutputDirectory 'QDNativeRoomLaunchTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Native room launch tests failed'}
 & $Compiler @flags (Join-Path $src 'tests\projectile_stream_tests.cpp') -o (Join-Path $OutputDirectory 'QDProjectileStreamTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Projectile stream test build failed'}
 & (Join-Path $OutputDirectory 'QDProjectileStreamTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Projectile stream tests failed'}
 & $Compiler @flags (Join-Path $src 'tests\sound_stream_tests.cpp') -o (Join-Path $OutputDirectory 'QDSoundStreamTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Sound stream test build failed'}
 & (Join-Path $OutputDirectory 'QDSoundStreamTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Sound stream tests failed'}
 & $Compiler @flags (Join-Path $src 'tests\placed_item_claim_tests.cpp') -o (Join-Path $OutputDirectory 'QDPlacedItemClaimTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Placed item claim test build failed'}
 & (Join-Path $OutputDirectory 'QDPlacedItemClaimTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Placed item claim tests failed'}
 & $Compiler @flags (Join-Path $src 'tests\body_material_tests.cpp') -o (Join-Path $OutputDirectory 'QDBodyMaterialTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Body material test build failed'}
 & (Join-Path $OutputDirectory 'QDBodyMaterialTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Body material tests failed'}
 & $Compiler @flags (Join-Path $src 'tests\visual_mount_tests.cpp') -o (Join-Path $OutputDirectory 'QDVisualMountTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Visual mount test build failed'}
 & (Join-Path $OutputDirectory 'QDVisualMountTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Visual mount tests failed'}
 & $Compiler @flags (Join-Path $src 'tests\boss_hud_tests.cpp') -o (Join-Path $OutputDirectory 'QDBossHudTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Boss HUD test build failed'}
 & (Join-Path $OutputDirectory 'QDBossHudTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Boss HUD tests failed'}
 & $Compiler @flags @core (Join-Path $src 'tests\steam_recovery_tests.cpp') -lbcrypt -o (Join-Path $OutputDirectory 'QDSteamRecoveryTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Steam recovery test build failed'}
 & (Join-Path $OutputDirectory 'QDSteamRecoveryTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Steam recovery tests failed'}
 & $Compiler @flags -municode (Join-Path $src 'tests\native_tick_bridge_tests.cpp') -o (Join-Path $OutputDirectory 'QDNativeTickBridgeTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Native bridge test build failed'}
 & (Join-Path $OutputDirectory 'QDNativeTickBridgeTests.exe') (Join-Path $OutputDirectory 'QDEngineTickBridge.dll')
 if($LASTEXITCODE -ne 0){throw 'Native bridge rejection tests failed'}
 $objdump=Join-Path ([IO.Path]::GetDirectoryName($Compiler)) 'objdump.exe'
 foreach($dll in @('QDCoopNativeM1.dll','QDEngineTickBridge.dll','QDNativeBootstrap.dll','xinput1_3.dll')){
  $pe=& $objdump -p (Join-Path $OutputDirectory $dll)
  if($LASTEXITCODE -ne 0){throw 'PE inspection failed'}
  $imports=@($pe|Select-String 'DLL Name:'|ForEach-Object{($_.Line -split 'DLL Name:')[1].Trim()})
  if(!$imports.Count -or @($imports|Where-Object{$_ -notin @('KERNEL32.dll','msvcrt.dll','bcrypt.dll','WS2_32.dll','SHELL32.dll')}).Count){throw "Unexpected runtime import: $imports"}
  Write-Output "$dll imports: $($imports -join ', ')"
 }
 $env:PATH="$env:SystemRoot\System32;$env:SystemRoot"
 foreach($test in @('QDNamePoolTests.exe','QDReflectionTests.exe','QDWeakObjectTests.exe','QDEnemyStreamTests.exe','QDNativeCoreTests.exe','QDAnimationTests.exe','QDDeathTests.exe','QDRewardTests.exe','QDObjectCountMapTests.exe','QDRewardJournalTests.exe','QDRewardWorkerTests.exe')){
  & (Join-Path $OutputDirectory $test);if($LASTEXITCODE -ne 0){throw "$test failed"}
 }
 & (Join-Path $OutputDirectory 'QDNativeDllTests.exe') (Join-Path $OutputDirectory 'QDCoopNativeM1.dll')
 if($LASTEXITCODE -ne 0){throw 'DLL ABI tests failed'}
 & (Join-Path $OutputDirectory 'QDRescueLedgerTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Rescue ledger tests failed'}
 & (Join-Path $OutputDirectory 'QDWorldProgressTests.exe')
 if($LASTEXITCODE -ne 0){throw 'World progress tests failed'}
 & (Join-Path $OutputDirectory 'QDEnemyDamageTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Enemy damage tests failed'}
 & (Join-Path $OutputDirectory 'QDThreatPolicyTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Threat policy tests failed'}
 Write-Output 'M1 built and verified. The Lua lab and native bootstrap use separate entry points.'
} finally {$env:PATH=$saved}
