param(
    [Parameter(Mandatory=$true)][string]$Compiler,
    [string]$OutputDirectory = (Join-Path $PSScriptRoot 'dist\native')
)
$ErrorActionPreference = 'Stop'
$Compiler = (Resolve-Path -LiteralPath $Compiler).Path
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
$savedPath = $env:PATH
try {
    $env:PATH = [IO.Path]::GetDirectoryName($Compiler) + ';' + $savedPath
    $native = Join-Path $PSScriptRoot 'native'
    $flags = @('-std=c++20','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-fno-exceptions','-fno-rtti','-static','-static-libgcc','-static-libstdc++','-I',(Join-Path $native 'include'))
    $core = @((Join-Path $native 'src\session.cpp'),(Join-Path $native 'src\transactions.cpp'))
    & $Compiler @flags @core (Join-Path $native 'src\api.cpp') '-shared' '-Wl,--no-undefined' '-Wl,--exclude-all-symbols' '-o' (Join-Path $OutputDirectory 'QDCoopNative.dll')
    if ($LASTEXITCODE -ne 0) { throw 'Native DLL compilation failed' }
    & $Compiler @flags @core (Join-Path $native 'tests\core_tests.cpp') '-o' (Join-Path $OutputDirectory 'QDNativeCoreTests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Core test compilation failed' }
    & $Compiler @flags (Join-Path $native 'tests\dll_tests.cpp') '-o' (Join-Path $OutputDirectory 'QDNativeDllTests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'DLL harness compilation failed' }
    $objdump = Join-Path ([IO.Path]::GetDirectoryName($Compiler)) 'objdump.exe'
    $pe = & $objdump '-p' (Join-Path $OutputDirectory 'QDCoopNative.dll')
    if ($LASTEXITCODE -ne 0) { throw 'DLL dependency inspection failed' }
    $imports = @($pe | Select-String 'DLL Name:' | ForEach-Object { ($_.Line -split 'DLL Name:')[1].Trim() })
    if ($imports.Count -eq 0 -or @($imports | Where-Object { $_ -notin @('KERNEL32.dll','msvcrt.dll') }).Count -ne 0) {
        throw ('Unexpected DLL dependency: ' + ($imports -join ', '))
    }
    Write-Output ('DLL imports: ' + ($imports -join ', '))
    # Prevent compiler-bin PATH entries from masking missing runtime DLLs.
    $env:PATH = (Join-Path $env:SystemRoot 'System32') + ';' + $env:SystemRoot
    & (Join-Path $OutputDirectory 'QDNativeCoreTests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Core tests failed' }
    & (Join-Path $OutputDirectory 'QDNativeDllTests.exe') (Join-Path $OutputDirectory 'QDCoopNative.dll')
    if ($LASTEXITCODE -ne 0) { throw 'DLL tests failed' }
    Write-Output ('Built and verified native foundation: ' + $OutputDirectory)
} finally { $env:PATH = $savedPath }
