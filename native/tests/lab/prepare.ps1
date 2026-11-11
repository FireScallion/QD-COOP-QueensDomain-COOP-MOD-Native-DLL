param(
 [string]$OfficialGame='D:\steam\steamapps\common\QueensDomain',
 [Parameter(Mandatory=$true)][string]$RuntimeDirectory,
 [Parameter(Mandatory=$true)][string]$TestSave,
 [Parameter(Mandatory=$true)][string]$NativeBuildDirectory,
 [Parameter(Mandatory=$true)][string]$Preview2Scripts,
 [string]$TestGameSettings
)
$ErrorActionPreference='Stop'
$relative='QueensDomain\Binaries\Win64\QueensDomain-Win64-Shipping.exe'
$expected='79CCDC6CF9E1E85512D52D2CB1C3FFA963D400BE1555051A3523C982D181D1E6'
if((Get-FileHash -LiteralPath (Join-Path $OfficialGame $relative)).Hash -ne $expected){throw 'Official build changed; revalidate adapter before testing'}
if((Get-FileHash -LiteralPath (Join-Path $RuntimeDirectory 'ue4ss\UE4SS.dll')).Hash -ne 'AD0396A530F9EFF2BA1DE7E27E6129328AFF975211E29CCFCF29DA3C8F765021'){throw 'Pinned UE4SS runtime required'}
foreach($helper in @('qdcapture.dll','qdplacement.dll')){if(!(Test-Path -LiteralPath (Join-Path $Preview2Scripts $helper))){throw 'Preview2Scripts must contain the built preview2 helper DLLs as well as Lua sources'}}
foreach($role in @('host','client')){if(Test-Path -LiteralPath (Join-Path $PSScriptRoot $role)){throw 'Fresh lab required; preserve previous results in another directory'}}
function CopyGame([string]$from,[string]$to){
 [IO.Directory]::CreateDirectory($to)|Out-Null
 foreach($f in Get-ChildItem -LiteralPath $from){
  if($f.Name -in @('ue4ss','Saved','QDCoop-Direct','UnrealEngine','dwmapi.dll','xinput1_3.dll') -or $f.Name -like 'QDCoop-*.pak' -or $f.Extension -in @('.log','.dmp','.mdmp')){continue}
  $dest=Join-Path $to $f.Name
  if($f.PSIsContainer){CopyGame $f.FullName $dest}else{Copy-Item -LiteralPath $f.FullName -Destination $dest}
 }
}
foreach($directory in @('bin','legacy')){[IO.Directory]::CreateDirectory((Join-Path $PSScriptRoot $directory))|Out-Null}
foreach($name in @('QDCoopNativeM1.dll','QDEngineTickBridge.dll')){Copy-Item -LiteralPath (Join-Path $NativeBuildDirectory $name) -Destination (Join-Path $PSScriptRoot 'bin')}
foreach($f in Get-ChildItem -LiteralPath $Preview2Scripts -File){if($f.Extension -in @('.lua','.dll')){Copy-Item -LiteralPath $f.FullName -Destination (Join-Path $PSScriptRoot 'legacy')}}
foreach($role in @('host','client')){
 $lab=Join-Path $PSScriptRoot $role;$game=Join-Path $lab 'game';$profile=Join-Path $lab 'profile'
 foreach($part in @('Engine','QueensDomain')){CopyGame (Join-Path $OfficialGame $part) (Join-Path $game $part)}
 $bin=Join-Path $game 'QueensDomain\Binaries\Win64'
 Copy-Item -LiteralPath (Join-Path $RuntimeDirectory 'ue4ss') -Destination $bin -Recurse
 Copy-Item -LiteralPath (Join-Path $RuntimeDirectory 'dwmapi.dll') -Destination $bin
 Copy-Item -LiteralPath (Join-Path $NativeBuildDirectory 'QDEngineTickBridge.dll') -Destination $bin
 $settings=Join-Path $bin 'ue4ss\UE4SS-settings.ini'
 [IO.File]::WriteAllText($settings,([IO.File]::ReadAllText($settings) -replace 'UseCache = 1','UseCache = 0'))
 $mods=Join-Path $bin 'ue4ss\Mods';$scripts=Join-Path $mods 'QDCoopProbe\Scripts'
 [IO.Directory]::CreateDirectory($scripts)|Out-Null
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'probe-main.lua') -Destination (Join-Path $scripts 'main.lua')
 [IO.File]::WriteAllText((Join-Path $scripts 'config.lua'),('return {Root=[==['+$lab.Replace('\','/')+']==]}'))
 [IO.File]::WriteAllText((Join-Path $mods 'mods.txt'),"QDCoopProbe : 1`r`n")
 $saves=Join-Path $profile 'Saved\SaveGames';[IO.Directory]::CreateDirectory($saves)|Out-Null
 Copy-Item -LiteralPath $TestSave -Destination (Join-Path $saves 'World1.sav')
 if($TestGameSettings){$config=Join-Path $profile 'Saved\Config\WindowsNoEditor';[IO.Directory]::CreateDirectory($config)|Out-Null;Copy-Item -LiteralPath $TestGameSettings -Destination (Join-Path $config 'GameUserSettings.ini')}
 [IO.File]::WriteAllText((Join-Path $game 'steam_appid.txt'),'1799840')
 if((Get-FileHash -LiteralPath (Join-Path $game $relative)).Hash -ne $expected){throw 'Copy fingerprint mismatch'}
}
Write-Output 'Prepared fresh isolated copies. Start using station-run.ps1 -Script ./run-private.ps1.'
