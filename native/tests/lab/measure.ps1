param([string]$Scenario='bridge-1440p',[string[]]$Modes=@('native','legacy','legacy','native','native','legacy'))
$ErrorActionPreference='Stop'
function Probe([string]$role,[string]$label,[string]$code){
 $raw=(& (Join-Path $PSScriptRoot 'probe.ps1') -Role $role -Label $label -Code $code)|Out-String
 $lines=$raw.TrimEnd() -split '\r?\n',4
 if($lines.Count -ne 4 -or $lines[0] -ne 'true' -or $lines[2] -ne 'true'){throw $raw}
 return $lines[3]
}
$results=@();$gpu=@();$index=0
foreach($mode in $Modes){
 $index++;$label="$Scenario-$index-$mode"
 foreach($r in @('host','client')){Probe $r "$label-switch" ('return M1.switch("'+$mode+'")')|Out-Null}
 Start-Sleep -Seconds 3
 $before=@{}
 foreach($r in @('host','client')){$before[$r]=(Probe $r "$label-begin" 'assert(M1.native("B")==1);return "{\"native\":"..M1.native("S")..",\"legacyFrames\":"..M1.legacyFrames..",\"legacyApplied\":"..M1.legacyApplied.."}"')|ConvertFrom-Json}
 for($second=0;$second -lt 12;$second++){
  Start-Sleep -Seconds 1
  if($second%3 -eq 0){$gpu+=@{trial=$label;utc=[DateTime]::UtcNow.ToString('o');reading=(& "$env:SystemRoot\System32\nvidia-smi.exe" --query-gpu=utilization.gpu,utilization.memory,power.draw --format=csv,noheader,nounits)} }
 }
 foreach($r in @('host','client')){
  $metric=(Probe $r "$label-end" 'local s=M1.native("E");return s:sub(1,-2)..",\"legacy_frames\":"..M1.legacyFrames..",\"legacy_applied\":"..M1.legacyApplied.."}"')|ConvertFrom-Json
  $legacy=Probe $r "$label-status" 'return M1.legacyStatus()'
  if($metric.state.active -ne 24 -or $metric.state.build -ne 1 -or $metric.samples -lt 100){throw 'Invalid measurement scene'}
  $results+=@{scenario=$Scenario;trial=$index;mode=$mode;role=$r;before=$before[$r];metric=$metric;legacy=$legacy}
  Write-Output ("{0} {1}: frame p50={2:N3} p95={3:N3} ms, GT CPU/frame={4:N3} ms" -f $label,$r,$metric.frame_ms.p50,$metric.frame_ms.p95,($metric.game_thread_cpu_ms/$metric.samples))
 }
 $results|ConvertTo-Json -Depth 12|Set-Content -LiteralPath (Join-Path $PSScriptRoot "$Scenario-results.json")
 $gpu|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $PSScriptRoot "$Scenario-gpu.json")
}
