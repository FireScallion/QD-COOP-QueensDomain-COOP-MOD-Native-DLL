$ErrorActionPreference='Stop'
Add-Type -Path (Join-Path $PSScriptRoot 'Station.cs')
if([QuietStation]::StationName() -eq 'WinSta0'){throw 'Interactive desktop is forbidden for this test'}
$owned=@()
try {
 foreach($role in @('host','client')){
  $lab=Join-Path $PSScriptRoot $role;$game=Join-Path $lab 'game';$profile=Join-Path $lab 'profile'
  if(Test-Path -LiteralPath (Join-Path $lab 'heartbeat.txt')){throw 'Archive the previous run metadata before restarting this lab'}
  $psi=[Diagnostics.ProcessStartInfo]::new();$psi.FileName=Join-Path $game 'QueensDomain\Binaries\Win64\QueensDomain-Win64-Shipping.exe';$psi.WorkingDirectory=$game
  $psi.UseShellExecute=$false;$psi.CreateNoWindow=$true
  $psi.EnvironmentVariables['LOCALAPPDATA']=Join-Path $profile 'AppData\Local';$psi.EnvironmentVariables['APPDATA']=Join-Path $profile 'AppData\Roaming';$psi.EnvironmentVariables['USERPROFILE']=$profile
  $psi.EnvironmentVariables['SteamAppId']='1799840';$psi.EnvironmentVariables['SteamGameId']='1799840'
  $renderer=if($role -eq 'host'){'-NullRHI'}else{'-d3d12 -RenderOffscreen'}
  $fps=if($role -eq 'host'){'30'}else{'0'}
  $psi.Arguments='-UserDir="'+$profile.Replace('\','/')+'" -NoSplash -NoMoviePlayer '+$renderer+' -unattended -nosound -windowed -ResX=1280 -ResY=720 -DefaultViewportMouseCaptureMode=NoCapture -ExecCmds="r.VSync 0,t.MaxFPS '+$fps+'"'
  $p=[Diagnostics.Process]::Start($psi);$owned+=@{p=$p;lab=$lab;start=$p.StartTime.ToUniversalTime().ToString('o')}
  @{pid=$p.Id;startedUtc=$p.StartTime.ToUniversalTime().ToString('o');station=[QuietStation]::StationName();exe=$psi.FileName;arguments=$psi.Arguments;profile=$profile}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $lab 'process.json')
  $boot=[DateTime]::UtcNow.AddSeconds(90)
  while(!(Test-Path -LiteralPath (Join-Path $lab 'heartbeat.txt'))){
   if($p.HasExited -or [DateTime]::UtcNow -gt $boot){throw "$role did not reach isolated probe"}
   Start-Sleep -Milliseconds 500
  }
 }
 $deadline=[DateTime]::UtcNow.AddMinutes(45)
 while([DateTime]::UtcNow -lt $deadline -and !(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'stop.flag'))){
  if(@($owned|Where-Object{!$_.p.HasExited}).Count -eq 0){break}
  Start-Sleep -Milliseconds 250
 }
} finally {
 foreach($item in $owned){$p=$item.p;if(!$p.HasExited){$p.Kill();$p.WaitForExit()};@{pid=$p.Id;startedUtc=$item.start;exitCode=$p.ExitCode;endedUtc=[DateTime]::UtcNow.ToString('o')}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $item.lab 'exit.json')}
}
