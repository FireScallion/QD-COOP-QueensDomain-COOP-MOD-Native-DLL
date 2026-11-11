param([Parameter(Mandatory=$true)][ValidateSet('host','client')][string]$Role,[Parameter(Mandatory=$true)][string]$Code,[string]$Label='probe')
$ErrorActionPreference='Stop'
$lab=Join-Path $PSScriptRoot $Role
$id=[Guid]::NewGuid().ToString('N')
$body="local ok,result=pcall(function() "+$Code+" end); return '"+$id+"\n'..tostring(ok)..'\n'..tostring(result)"
[IO.File]::WriteAllText((Join-Path $lab 'request.lua'),$body)
$deadline=[DateTime]::UtcNow.AddSeconds(12)
do {
 Start-Sleep -Milliseconds 100
 $path=Join-Path $lab 'response.txt'
 if(Test-Path -LiteralPath $path){$text=[IO.File]::ReadAllText($path);if($text.Contains($id)){
  $history=Join-Path $lab 'evidence';[IO.Directory]::CreateDirectory($history)|Out-Null
  [IO.File]::WriteAllText((Join-Path $history ($Label+'-'+$id+'.txt')),$text)
  Write-Output $text;exit 0
 }}
}while([DateTime]::UtcNow -lt $deadline)
throw 'Probe timed out; do not treat an older response as current'
