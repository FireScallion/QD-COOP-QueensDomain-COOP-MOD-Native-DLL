param([string]$Script)
$ErrorActionPreference='Stop'
Add-Type -Path (Join-Path $PSScriptRoot 'Station.cs')
$code=[QuietStation]::Run((Resolve-Path $Script).Path,(Get-Location).Path)
if($code -ne 0){throw "Noninteractive child exited: $code"}
