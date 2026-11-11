param(
    [string]$Compiler = 'gcc',
    [string]$OutputDirectory = (Join-Path $PSScriptRoot 'dist/runtime/QDCoop-Direct')
)

$ErrorActionPreference = 'Stop'
$source = Join-Path $PSScriptRoot 'QDCoop-Direct/source'
$compilerPath = (Get-Command -Name $Compiler -CommandType Application).Source
$env:Path = (Split-Path -Parent $compilerPath) + [IO.Path]::PathSeparator + $env:Path

$nativeOutput = Join-Path $OutputDirectory 'mod/Scripts'
$bridgeOutput = Join-Path $OutputDirectory 'standalone'
[IO.Directory]::CreateDirectory($nativeOutput) | Out-Null
[IO.Directory]::CreateDirectory($bridgeOutput) | Out-Null

$modules = @(
    'Bootstrap/qdboot',
    'Steam/qdsteam',
    'AsyncIPC/qdipc',
    'Capture/qdcapture',
    'Encounter/qdscene',
    'Placement/qdplacement',
    'PoseBatch/qdpose',
    'Render/qdread'
)
foreach ($module in $modules) {
    $output = Join-Path $nativeOutput ([IO.Path]::GetFileName($module) + '.dll')
    & $compilerPath -shared -O2 -Wall -Wextra -static-libgcc -o $output (Join-Path $source ($module + '.c'))
    if ($LASTEXITCODE -ne 0) { throw "Could not build $module" }
}

& dotnet publish (Join-Path $source 'Bridge/Bridge.csproj') --configuration Release --runtime win-x64 --self-contained true --output $bridgeOutput -p:RuntimeFrameworkVersion=9.0.9
if ($LASTEXITCODE -ne 0) { throw 'Could not publish the bridge' }
