param(
    [Parameter(Mandatory)][string]$RuntimeZip,
    [Parameter(Mandatory)][string]$OutputZip
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$source = (Resolve-Path -LiteralPath $RuntimeZip).ProviderPath
$destination = if ([IO.Path]::IsPathRooted($OutputZip)) { [IO.Path]::GetFullPath($OutputZip) }
    else { [IO.Path]::GetFullPath((Join-Path (Get-Location).ProviderPath $OutputZip)) }
$runtimeHash = 'B0DEF2F88996D9CED71898C9CB3A999FD104EDD99C05C7358FDFF5DBBAB0A5B9'
if ((Get-FileHash -LiteralPath $source).Hash -ne $runtimeHash) { throw 'Expected the verified M131 runtime ZIP.' }
if (Test-Path -LiteralPath $destination) { throw "Output already exists: $destination" }

$tempParent = (Resolve-Path -LiteralPath ([IO.Path]::GetTempPath())).ProviderPath
$scratch = [IO.Directory]::CreateDirectory((Join-Path $tempParent ('qdcoop-setup-' + [guid]::NewGuid().ToString('N')))).FullName
try {
    $runtime = Join-Path $scratch 'runtime'
    [IO.Compression.ZipFile]::ExtractToDirectory($source,$runtime)
    $setup = Join-Path $scratch 'QDCoopNative-Setup'
    $payload = Join-Path $setup 'payload'
    $null = [IO.Directory]::CreateDirectory($payload)
    foreach ($name in 'Binaries','Content') {
        Copy-Item -LiteralPath (Join-Path $runtime $name) -Destination $payload -Recurse
    }
    foreach ($name in 'Install-Native.cmd','Uninstall-Native.cmd','Uninstall-UE4SS-QDCoop.cmd','Uninstall.cmd',
        'QDCoop-Setup.ps1','QDCoop-Legacy.ps1') {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $setup
    }
    $guide = Get-Item -LiteralPath (Join-Path $PSScriptRoot 'INSTALL.zh-CN.md')
    if (!$guide) { throw 'Installation guide is missing.' }
    Copy-Item -LiteralPath $guide.FullName -Destination $setup
    $files = @(Get-ChildItem -LiteralPath $payload -Recurse -File | Sort-Object FullName | ForEach-Object {
        $relative = $_.FullName.Substring($payload.Length + 1).Replace('\','/')
        [pscustomobject]@{Path="QueensDomain/$relative";Hash=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
    if ($files.Count -ne 8) { throw 'Unexpected native payload size.' }
    $tools = @(Get-ChildItem -LiteralPath $setup -File | Sort-Object Name | ForEach-Object {
        [pscustomobject]@{Path=$_.Name;Hash=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
    $manifest = ConvertTo-Json -InputObject ([pscustomobject]@{Version=1;Files=$files;Tools=$tools}) -Depth 4
    [IO.File]::WriteAllText((Join-Path $setup 'payload.json'),$manifest,[Text.UTF8Encoding]::new($false))
    Compress-Archive -LiteralPath $setup -DestinationPath $destination -CompressionLevel Optimal
    Get-FileHash -LiteralPath $destination
} finally {
    $checked = (Resolve-Path -LiteralPath $scratch).ProviderPath
    if ((Split-Path $checked -Parent) -eq $tempParent -and (Split-Path $checked -Leaf) -match '^qdcoop-setup-[0-9a-f]{32}$' -and
        !((Get-Item -LiteralPath $checked).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        Remove-Item -LiteralPath $checked -Recurse -Force
    }
}
