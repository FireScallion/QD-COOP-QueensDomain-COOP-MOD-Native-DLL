param([switch]$Uninstall)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$bin=Join-Path $root 'QueensDomain/Binaries/Win64'
$record=Join-Path $PSScriptRoot 'InstallBackup/manifest.json'
$gameExe=Join-Path $bin 'QueensDomain-Win64-Shipping.exe'
foreach($process in @(Get-Process -Name 'QueensDomain-Win64-Shipping' -ErrorAction SilentlyContinue)){
 if($process.Path -and $process.Path.Equals($gameExe,[StringComparison]::OrdinalIgnoreCase)){throw 'Close this game before installing or uninstalling QD COOP.'}
}
function WriteUtf8($p,$s){[IO.File]::WriteAllText($p,$s,[Text.UTF8Encoding]::new($false))}
function SafeTarget($relative){
 $p=[IO.Path]::GetFullPath((Join-Path $root $relative))
 if(!$p.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Invalid install destination'}
 $cursor=$p
 while($cursor -and $cursor.Length -ge $root.Length){if((Test-Path -LiteralPath $cursor) -and ((Get-Item -LiteralPath $cursor).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw "Linked install destination: $cursor"};$cursor=Split-Path $cursor}
 return $p
}
function Read-InstallRows($item,[int]$depth=0){
 if($depth -gt 32){throw 'Install record is nested too deeply.'}
 if($item -is [array]){
  foreach($child in $item){Read-InstallRows $child ($depth+1)}
 }elseif($item -is [string]){
  if($item -match '^@\{Path=(?<file>[^;]+); Backup=(?<backup>[^;]*); InstalledHash=(?<hash>[a-fA-F0-9]{64})\}$'){
   [pscustomobject]@{Path=$Matches.file;Backup=$Matches.backup;InstalledHash=$Matches.hash}
  }
 }elseif($item -and $item.Path -is [string] -and $item.InstalledHash -match '^[a-fA-F0-9]{64}$'){$item}
}
if($Uninstall){
 if(!(Test-Path -LiteralPath $record)){throw 'No installation backup exists'}
 $data=Get-Content -LiteralPath $record -Raw|ConvertFrom-Json
 $rows=@(Read-InstallRows $data)
 $mods='QueensDomain/Binaries/Win64/ue4ss/Mods'
 $list=SafeTarget "$mods/mods.txt"
 if(Test-Path -LiteralPath $list -PathType Leaf){
  $reader=[IO.StreamReader]::new($list,[Text.UTF8Encoding]::new($false),$true)
  try{$old=$reader.ReadToEnd();$encoding=$reader.CurrentEncoding}finally{$reader.Dispose()}
  $clean=[regex]::Replace($old,'(?im)^[ \t]*QDCoop[ \t]*:[^\r\n]*(?:\r\n|\n|$)','')
  if($old -cne $clean){[IO.File]::WriteAllText($list,$clean,$encoding)}
 }
 $marker=SafeTarget "$mods/QDCoop/enabled.txt"
 if(Test-Path -LiteralPath $marker -PathType Leaf){Remove-Item -LiteralPath $marker}
 foreach($group in ($rows|Group-Object {$_.Path.Replace('\','/')})){
  $relative=$group.Name
  if($relative -notmatch ('^'+[regex]::Escape("$mods/QDCoop/Scripts/")+'[a-zA-Z0-9_-][a-zA-Z0-9_.-]*$') -and
     $relative -notmatch '^QueensDomain/Content/Paks/QDCoop-[a-zA-Z0-9_-]+\.pak$'){continue}
  $dest=SafeTarget $relative
  if(Test-Path -LiteralPath $dest){
   $hash=(Get-FileHash -LiteralPath $dest).Hash
   if($hash -notin $group.Group.InstalledHash){Write-Warning "Changed since installation; kept: $dest";continue}
   $original=$group.Group[0].Backup
   if($original){
    $backup=$original.Replace('\','/')
    if($backup -notmatch '^InstallBackup/files/[a-fA-F0-9]{32}$'){throw 'Invalid install backup path'}
    $originalFile=SafeTarget "QDCoop-Direct/$backup"
    if((Get-FileHash -LiteralPath $originalFile).Hash -in $group.Group.InstalledHash){Remove-Item -LiteralPath $dest}
    else{Copy-Item -LiteralPath $originalFile -Destination $dest -Force}
   }else{Remove-Item -LiteralPath $dest}
  }
 }
 Write-Host 'QD COOP removed. Saves, other mods, and backup files were kept.';exit
}
if(!(Test-Path -LiteralPath (Join-Path $bin 'QueensDomain-Win64-Shipping.exe'))){throw 'Extract beside the original game launcher, Engine and QueensDomain folders, then run Install.cmd.'}
# Do not replace a running game's DLLs or someone else's proxy loader.
$proxy=Join-Path $bin 'dwmapi.dll';$bundled=Join-Path $PSScriptRoot 'runtime/dwmapi.dll'
if((Test-Path -LiteralPath $proxy) -and (Get-FileHash -LiteralPath $proxy).Hash -ne (Get-FileHash -LiteralPath $bundled).Hash){throw 'An unfamiliar dwmapi.dll loader is installed. Nothing was replaced; merge loaders manually.'}
$records=[Collections.Generic.List[object]]::new()
if(Test-Path -LiteralPath $record){
 $data=Get-Content -LiteralPath $record -Raw|ConvertFrom-Json
 foreach($group in ((Read-InstallRows $data)|Group-Object {$_.Path.Replace('\','/')})){$records.Add($group.Group[0])}
}
$backupRoot=Join-Path $PSScriptRoot 'InstallBackup/files';[IO.Directory]::CreateDirectory($backupRoot)|Out-Null
function Deploy($source,$relative){
 $dest=SafeTarget $relative
 $row=$records|Where-Object Path -eq $relative|Select-Object -First 1
 if(!$row){
  $backup=''
  if(Test-Path -LiteralPath $dest){$backup='InstallBackup/files/'+[Guid]::NewGuid().ToString('N');Copy-Item -LiteralPath $dest -Destination (Join-Path $PSScriptRoot $backup)}
  $row=[pscustomobject]@{Path=$relative;Backup=$backup;InstalledHash=''};$records.Add($row)
 }
 $row.InstalledHash=(Get-FileHash -LiteralPath $source).Hash
 WriteUtf8 $record (ConvertTo-Json -InputObject @($records.ToArray()) -Depth 4)
 [IO.Directory]::CreateDirectory((Split-Path $dest))|Out-Null
 Copy-Item -LiteralPath $source -Destination $dest -Force
}
foreach($f in Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'runtime') -Recurse -File){
 $rel=$f.FullName.Substring((Join-Path $PSScriptRoot 'runtime').Length+1)
 if($rel -eq 'ue4ss\UE4SS-settings.ini'){continue}
 Deploy $f.FullName ('QueensDomain/Binaries/Win64/'+$rel)
}
foreach($f in Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'mod/Scripts') -File){Deploy $f.FullName ('QueensDomain/Binaries/Win64/ue4ss/Mods/QDCoop/Scripts/'+$f.Name)}
foreach($f in Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'assets') -Filter '*.pak'){Deploy $f.FullName ('QueensDomain/Content/Paks/'+$f.Name)}
$temp=Join-Path $PSScriptRoot 'InstallBackup/prepared.txt'
$settings=Join-Path $bin 'ue4ss/UE4SS-settings.ini'
if(Test-Path -LiteralPath $settings){$ini=[IO.File]::ReadAllText($settings)}else{$ini=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'runtime/ue4ss/UE4SS-settings.ini'))}
# Merge required entries within their sections; preserve unrelated settings.
foreach($section in @('General','Hooks')){
 $changes=if($section -eq 'General'){@{DefaultExecuteInGameThreadMethod='EngineTick';EnableHotReloadSystem='0';EnableAutoReloadingLuaMods='0';bUseUObjectArrayCache='true';bForceGUObjectArrayForIteration='false'}}else{@{HookEngineTick='1';HookProcessInternal='1';HookProcessLocalScriptFunction='1';HookBeginPlay='1';HookEndPlay='1';HookLoadMap='1';HookUObjectProcessEvent='1'}}
 $pattern='(?ms)^\['+[regex]::Escape($section)+'\][^\r\n]*\r?\n(?<body>.*?)(?=^\[|\z)'
 $match=[regex]::Match($ini,$pattern)
 $body=if($match.Success){$match.Groups['body'].Value}else{''}
 foreach($k in $changes.Keys){$line=$k+' = '+$changes[$k];$keyPattern='(?m)^'+[regex]::Escape($k)+'\s*=.*$';if([regex]::IsMatch($body,$keyPattern)){$body=[regex]::Replace($body,$keyPattern,$line)}else{$body+="`r`n"+$line+"`r`n"}}
 $replacement='['+$section+"]`r`n"+$body
 if($match.Success){$ini=$ini.Substring(0,$match.Index)+$replacement+$ini.Substring($match.Index+$match.Length)}else{$ini+="`r`n"+$replacement}
}
WriteUtf8 $temp $ini;Deploy $temp 'QueensDomain/Binaries/Win64/ue4ss/UE4SS-settings.ini'
$list=Join-Path $bin 'ue4ss/Mods/mods.txt'
$text=if(Test-Path -LiteralPath $list){[IO.File]::ReadAllText($list)}else{''}
if($text -match '(?m)^\s*QDCoop\s*:'){$text=[regex]::Replace($text,'(?m)^\s*QDCoop\s*:.*$','QDCoop : 1')}else{$text+="`r`nQDCoop : 1`r`n"}
WriteUtf8 $temp $text;Deploy $temp 'QueensDomain/Binaries/Win64/ue4ss/Mods/mods.txt'
WriteUtf8 $temp '';Deploy $temp 'QueensDomain/Binaries/Win64/ue4ss/Mods/QDCoop/enabled.txt'
Write-Host 'QD COOP 1.18.0-preview3 installed. Start the ORIGINAL game launcher or Steam. Clear any old Steam QDCoopLauncher launch option. Original saves are used; a backup is made at startup.'
