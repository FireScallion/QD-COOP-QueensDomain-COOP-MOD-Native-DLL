[CmdletBinding()]
param(
    [ValidateSet('Install','Uninstall','UninstallLegacy','UninstallAll')]
    [string]$Action = 'Install',
    [string]$GameRoot
)

$ErrorActionPreference = 'Stop'
$bin = 'QueensDomain/Binaries/Win64'
$paks = 'QueensDomain/Content/Paks'
$shipping = "$bin/QueensDomain-Win64-Shipping.exe"
$shippingHash = '79CCDC6CF9E1E85512D52D2CB1C3FFA963D400BE1555051A3523C982D181D1E6'
$legacyProxy = "$bin/dwmapi.dll"
$legacyProxyHash = '482FCCB956F6BCEC816E568E0D71862664F5DA3D027B8347390A61B33F181E1D'
$dllNames = @('xinput1_3.dll','QDNativeBootstrap.dll','QDCoopNativeM1.dll')
$pakNames = @('QDCoop-GuestTarget.pak','QDCoop-NativeBody.pak','QDCoop-NativeSpear.pak','QDCoop-PlacedItem.pak','QDCoop-Skins.pak')
$dllTargets = @($dllNames | ForEach-Object { "$bin/$_" })
$pakTargets = @($pakNames | ForEach-Object { "$paks/$_" })
$nativeTargets = $dllTargets + $pakTargets

function Resolve-GameRoot([string]$Selection) {
    if ($Selection) { return (Resolve-Path -LiteralPath $Selection).ProviderPath }
    foreach ($candidate in @($PSScriptRoot,(Split-Path $PSScriptRoot -Parent),(Get-Location).ProviderPath)) {
        if (Test-Path -LiteralPath (Join-Path $candidate $shipping) -PathType Leaf) {
            return (Resolve-Path -LiteralPath $candidate).ProviderPath
        }
    }
    $steam = @(
        (Get-ItemProperty -LiteralPath 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
        (Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam' -ErrorAction SilentlyContinue).InstallPath
    ) | Where-Object { $_ } | Select-Object -Unique
    $roots = @(foreach ($homePath in $steam) {
        $libraries = @($homePath)
        $vdf = Join-Path $homePath 'steamapps/libraryfolders.vdf'
        if (Test-Path -LiteralPath $vdf -PathType Leaf) {
            foreach ($match in [regex]::Matches([IO.File]::ReadAllText($vdf),'(?mi)^\s*"path"\s*"([^"]+)"')) {
                $libraries += $match.Groups[1].Value.Replace('\\','\')
            }
        }
        foreach ($library in $libraries) {
            $apps = Join-Path $library 'steamapps'
            $acf = Join-Path $apps 'appmanifest_1799840.acf'
            if (!(Test-Path -LiteralPath $acf -PathType Leaf)) { continue }
            $match = [regex]::Match([IO.File]::ReadAllText($acf),'(?mi)^\s*"installdir"\s*"([^"\\/]+)"')
            if (!$match.Success) { continue }
            $candidate = Join-Path $apps ('common/' + $match.Groups[1].Value)
            if (Test-Path -LiteralPath (Join-Path $candidate $shipping) -PathType Leaf) {
                (Resolve-Path -LiteralPath $candidate).ProviderPath
            }
        }
    })
    $roots = @($roots | Select-Object -Unique)
    if ($roots.Count -eq 1) { return $roots[0] }
    $selection = (Read-Host 'Enter the Steam QueensDomain folder containing QueensDomain.exe').Trim().Trim('"')
    if (!$selection) { throw 'No game folder selected.' }
    return (Resolve-Path -LiteralPath $selection).ProviderPath
}

function Safe-Path([string]$Base,[string]$Relative) {
    if (!$Relative -or [IO.Path]::IsPathRooted($Relative) -or $Relative.Contains(':')) {
        throw "Invalid setup path: $Relative"
    }
    $path = [IO.Path]::GetFullPath((Join-Path $Base $Relative))
    if (!$path.StartsWith($Base + '\',[StringComparison]::OrdinalIgnoreCase)) { throw "Setup path escapes its folder: $path" }
    for ($part = $path; $part.Length -ge $Base.Length; $part = Split-Path $part -Parent) {
        if ((Test-Path -LiteralPath $part) -and ((Get-Item -LiteralPath $part -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Linked setup path: $part"
        }
        if ($part -eq $Base) { break }
    }
    return $path
}

function File-Hash([string]$Path) {
    if (!(Test-Path -LiteralPath $Path)) { return '' }
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Expected a file: $Path" }
    $file = [IO.File]::OpenRead($Path)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($sha.ComputeHash($file)).Replace('-','') }
    finally { $file.Dispose(); $sha.Dispose() }
}

function Is-Hash([string]$Hash) { return !$Hash -or $Hash -match '^[0-9A-Fa-f]{64}$' }

function Commit-File([string]$Temp,[string]$Path) {
    if (Test-Path -LiteralPath $Path) {
        [IO.File]::Replace($Temp,$Path,[System.Management.Automation.Language.NullString]::Value)
    }
    else { [IO.File]::Move($Temp,$Path) }
}

function Write-State([string]$Path,$Value) {
    $temp = $Path + '.' + [guid]::NewGuid().ToString('N') + '.tmp'
    $json = ConvertTo-Json -InputObject $Value -Depth 7
    [IO.File]::WriteAllText($temp,$json,[Text.UTF8Encoding]::new($false))
    try { Commit-File $temp $Path }
    finally { if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp } }
}

function Store-Path([string]$Hash) {
    if (!$Hash -or !(Is-Hash $Hash)) { throw 'Invalid file backup hash.' }
    return Safe-Path $stateDir "files/$Hash"
}

function Store-File([string]$Source,[string]$Hash) {
    $target = Store-Path $Hash
    $have = File-Hash $target
    if ($have -eq $Hash) { return }
    if ($have) { throw "Setup backup has changed: $target" }
    $temp = $target + '.' + [guid]::NewGuid().ToString('N') + '.tmp'
    try {
        [IO.File]::Copy($Source,$temp)
        if ((File-Hash $temp) -ne $Hash) { throw "Source changed while staging: $Source" }
        Commit-File $temp $target
    } finally { if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp } }
}

. (Join-Path $PSScriptRoot 'QDCoop-Legacy.ps1')

function Valid-Target([string]$Path) {
    return $Path -in $nativeTargets -or (Legacy-Target $Path)
}

function Add-Change([string]$Path,[string]$Before,[string]$After) {
    if ($Before -eq $After) { return }
    if (!(Valid-Target $Path) -or !(Is-Hash $Before) -or !(Is-Hash $After) -or $changes.Contains($Path)) {
        throw "Invalid or repeated setup change: $Path"
    }
    $changes.Add($Path,[pscustomobject]@{Path=$Path;Before=$Before;After=$After})
}

function Verify-State($State) {
    if (!$State) { return }
    if ($State.Version -ne 1 -or !$State.Files) { throw 'Unsupported native installation record.' }
    $seen = @{}
    foreach ($row in $State.Files) {
        if ($row.Path -notin ($nativeTargets + $legacyProxy) -or $seen.ContainsKey($row.Path) -or
            !(Is-Hash $row.Original) -or !(Is-Hash $row.Installed) -or ($row.Path -ne $legacyProxy -and !$row.Installed)) {
            throw 'Invalid native installation record.'
        }
        if ($row.Original -and (File-Hash (Store-Path $row.Original)) -ne $row.Original) {
            throw "Original file backup is missing or changed: $($row.Path)"
        }
        $seen[$row.Path] = $true
    }
}

function Complete-Transaction {
    if (!(Test-Path -LiteralPath $pending)) { return }
    $work = Get-Content -LiteralPath $pending -Raw | ConvertFrom-Json
    if ($work.Version -ne 1) { throw 'Unsupported pending setup record.' }
    Verify-State $work.State
    $steps = @(foreach ($row in $work.Changes) {
        if (!(Valid-Target $row.Path) -or !(Is-Hash $row.Before) -or !(Is-Hash $row.After)) { throw 'Invalid pending file change.' }
        $path = Safe-Path $game $row.Path
        $current = File-Hash $path
        if ($current -ne $row.Before -and $current -ne $row.After) { throw "File changed during setup: $path" }
        if ($row.After -and (File-Hash (Store-Path $row.After)) -ne $row.After) { throw "Pending file backup changed: $path" }
        if ($current -ne $row.After) { [pscustomobject]@{Path=$path;After=$row.After} }
    })
    foreach ($step in $steps) {
        if (!$step.After) { Remove-Item -LiteralPath $step.Path -Force; continue }
        $null = [IO.Directory]::CreateDirectory((Split-Path $step.Path -Parent))
        $temp = $step.Path + '.' + [guid]::NewGuid().ToString('N') + '.qdtmp'
        try {
            [IO.File]::Copy((Store-Path $step.After),$temp)
            Commit-File $temp $step.Path
        } finally { if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp } }
    }
    if ($work.State) { Write-State $record $work.State }
    elseif (Test-Path -LiteralPath $record) { Remove-Item -LiteralPath $record }
    Remove-Item -LiteralPath $pending
}

function Install-Native($State) {
    $payload = Join-Path $PSScriptRoot 'payload'
    $package = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'payload.json') -Raw | ConvertFrom-Json
    if ($package.Version -ne 1 -or $package.Files.Count -ne $nativeTargets.Count) {
        throw 'Incomplete or unsupported native payload manifest.'
    }
    $old = @{}
    if ($State) { foreach ($row in $State.Files) { $old[$row.Path] = $row } }
    $proxy = File-Hash (Safe-Path $game $legacyProxy)
    if ($proxy -and $proxy -ne $legacyProxyHash) { throw 'An unfamiliar dwmapi.dll is installed. The native loader cannot run with it.' }
    if ($proxy -and $old.ContainsKey($legacyProxy) -and $proxy -ne $old[$legacyProxy].Original) {
        throw 'The suspended legacy loader was changed.'
    }
    $legacyPresent = $proxy -or $old.ContainsKey($legacyProxy) -or
        (Test-Path -LiteralPath (Safe-Path $game "$bin/ue4ss/Mods/QDCoop/enabled.txt"))
    $legacy = $null
    $rows = @(foreach ($path in $nativeTargets) {
        $item = @($package.Files | Where-Object Path -eq $path)
        if ($item.Count -ne 1 -or !$item[0].Hash -or !(Is-Hash $item[0].Hash)) { throw "Invalid payload entry: $path" }
        $hash = $item[0].Hash
        $source = Safe-Path $payload ($path.Substring('QueensDomain/'.Length))
        $destination = Safe-Path $game $path
        $current = File-Hash $destination
        $previous = $old[$path]
        if ($current -and $current -ne $hash -and $current -ne $previous.Installed) {
            $knownLegacyPak = $false
            if ($legacyPresent -and $path -in $pakTargets) {
                if ($null -eq $legacy) { $legacy = Read-LegacyManifest }
                $knownLegacyPak = [bool]($legacy[$path] | Where-Object InstalledHash -eq $current)
            }
            if (!$knownLegacyPak) { throw "Existing file belongs to another installation or was changed: $destination" }
        }
        $original = ''
        if ($previous) { $original = $previous.Original }
        elseif ($current -and $path -in $pakTargets -and $legacyPresent) {
            Store-File $destination $current
            $original = $current
        }
        if ((File-Hash $source) -ne $hash) { throw "Payload missing or changed: $source" }
        Store-File $source $hash
        Add-Change $path $current $hash
        [pscustomobject]@{Path=$path;Installed=$hash;Original=$original}
    })
    if ($proxy) {
        Store-File (Safe-Path $game $legacyProxy) $proxy
        Add-Change $legacyProxy $proxy ''
        $rows += [pscustomobject]@{Path=$legacyProxy;Installed='';Original=$proxy}
    } elseif ($old.ContainsKey($legacyProxy)) { $rows += $old[$legacyProxy] }
    return [pscustomobject]@{Version=1;Files=$rows}
}

function Uninstall-Native($State) {
    if (!$State) { Write-Host 'No managed native installation found.'; return }
    foreach ($row in $State.Files) {
        $path = Safe-Path $game $row.Path
        $current = File-Hash $path
        if ($current -and $current -ne $row.Installed -and $current -ne $row.Original) { throw "File changed since installation: $path" }
        Add-Change $row.Path $current $row.Original
    }
}

function Commit-Changes($State) {
    Write-State $pending ([pscustomobject]@{Version=1;State=$State;Changes=@($changes.Values)})
    Complete-Transaction
}

function Remove-EmptyDirectory([string]$Path) {
    if ([IO.Directory]::Exists($Path) -and !(Get-ChildItem -LiteralPath $Path -Force | Select-Object -First 1)) {
        try { [IO.Directory]::Delete($Path) }
        catch [IO.IOException] { Write-Verbose "Directory is in use: $Path" }
    }
}

function Remove-UnusedStoreFiles($State) {
    $keep = @{}
    if ($State) {
        foreach ($row in $State.Files) {
            if ($row.Original) { $keep[$row.Original] = $true }
        }
    }
    $store = Safe-Path $stateDir 'files'
    foreach ($file in (Get-ChildItem -LiteralPath $store -File -Force)) {
        if ($file.Name -match '^[0-9A-Fa-f]{64}$' -and !$keep.ContainsKey($file.Name) -and
            !($file.Attributes -band [IO.FileAttributes]::ReparsePoint) -and (File-Hash $file.FullName) -eq $file.Name) {
            Remove-Item -LiteralPath $file.FullName -Force
        }
    }
    Remove-EmptyDirectory $store
}

function Remove-ExtractedNativePackage {
    $manifestPath = Safe-Path $game 'payload.json'
    if (!(Test-Path -LiteralPath $manifestPath -PathType Leaf)) { return }
    try { $package = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json }
    catch { return }
    if ($package.Version -ne 1 -or $package.Files.Count -ne $nativeTargets.Count) { return }
    $items = @(foreach ($target in $nativeTargets) {
        $match = @($package.Files | Where-Object Path -eq $target)
        if ($match.Count -ne 1 -or !$match[0].Hash -or !(Is-Hash $match[0].Hash)) { return }
        [pscustomobject]@{Path=('payload/' + $target.Substring('QueensDomain/'.Length));Hash=$match[0].Hash}
    })
    $packageTools = @('Install-Native.cmd','Uninstall-Native.cmd','Uninstall-UE4SS-QDCoop.cmd',
        'Uninstall.cmd','QDCoop-Setup.ps1','QDCoop-Legacy.ps1','INSTALL.zh-CN.md')
    foreach ($item in $package.Tools) {
        if ($item.Path -in $packageTools -and $item.Hash -and (Is-Hash $item.Hash)) { $items += $item }
    }
    # M123 packages predate the manifest entries for their setup tools.
    if ((File-Hash $manifestPath) -eq '1A2795CE5A3F37641E7AC0AEFBA67E80EE5916B4A3684FE08A24B45929EDA4C4') {
        $oldGuide = (-join [char[]](0x5b89,0x88c5,0x8bf4,0x660e)) + '.md'
        $oldTools = @{
            $oldGuide = '98C8AC7ECEEEA96391AB9F7E72315BA64F107B6059B6376CD3A522DB920FEE77'
            'Install-Native.cmd' = '7F331E640933A1553658366CE097955211703884E4D3217C016CBA12C7607C29'
            'QDCoop-Legacy.ps1' = '5C05ED25BE433B3A4835D90F018A5B83651AEC1158189F92140EE0C8A3596866'
            'QDCoop-Setup.ps1' = '862F2B48B1991776ED8750D5033470EE4C25D61620440C2A133ADA199F7177FF'
            'Uninstall-Native.cmd' = 'D44D7C2847ACE1171AEA2F6C54B3250A2D108715BBB6D8751E94871E28E5CB8B'
            'Uninstall-UE4SS-QDCoop.cmd' = '20B9F4A7FA05D3C6DC2A868AAEC5493D838FD8504BB487A758F23B94C57F5240'
        }
        $items += @($oldTools.GetEnumerator() | ForEach-Object {
            [pscustomobject]@{Path=$_.Key;Hash=$_.Value}
        })
    }
    $kept = $false
    foreach ($item in $items) {
        $path = Safe-Path $game $item.Path
        $hash = File-Hash $path
        if (!$hash) { continue }
        if ($hash -eq $item.Hash) { Remove-Item -LiteralPath $path -Force }
        else { $kept = $true; Write-Warning "Changed package file kept: $path" }
    }
    foreach ($path in @('payload/Binaries/Win64','payload/Binaries','payload/Content/Paks','payload/Content','payload')) {
        Remove-EmptyDirectory (Safe-Path $game $path)
    }
    if (!$kept) { Remove-Item -LiteralPath $manifestPath -Force }
}

$game = Resolve-GameRoot $GameRoot
$exe = Safe-Path $game $shipping
if (!(Test-Path -LiteralPath $exe -PathType Leaf)) { throw 'The selected folder is not the QueensDomain game root.' }
if ($Action -eq 'Install' -and (File-Hash $exe) -ne $shippingHash) {
    throw 'The selected folder does not contain supported Steam Build 26496428.'
}
foreach ($process in @(Get-Process QueensDomain-Win64-Shipping -ErrorAction SilentlyContinue)) {
    if ($process.Path -eq $exe) { throw 'Close the game before changing this installation.' }
}
Write-Host "Game folder: $game"
$stateDir = Safe-Path $game 'QDCoop-Native-Install'
$null = [IO.Directory]::CreateDirectory((Safe-Path $game 'QDCoop-Native-Install/files'))
$record = Safe-Path $stateDir 'install.json'
$pending = Safe-Path $stateDir 'pending.json'
$lock = [IO.FileStream]::new((Safe-Path $stateDir 'setup.lock'),[IO.FileMode]::OpenOrCreate,
    [IO.FileAccess]::ReadWrite,[IO.FileShare]::None,4096,[IO.FileOptions]::DeleteOnClose)
try {
    Complete-Transaction
    $state = if (Test-Path -LiteralPath $record) { Get-Content -LiteralPath $record -Raw | ConvertFrom-Json }
    Verify-State $state
    $changes = [ordered]@{}
    $nextState = $null
    switch ($Action) {
        Install { $nextState = Install-Native $state }
        Uninstall { Uninstall-Native $state }
        UninstallAll {
            Uninstall-Native $state
            Commit-Changes $null
            $changes.Clear()
            Uninstall-Legacy
        }
        UninstallLegacy {
            $nativePresent = $null -ne $state -or
                (File-Hash (Safe-Path $game "$bin/QDNativeBootstrap.dll")) -or
                (File-Hash (Safe-Path $game "$bin/QDCoopNativeM1.dll"))
            Uninstall-Legacy $state $nativePresent
            $nextState = $state
        }
    }
    Commit-Changes $nextState
    Remove-UnusedStoreFiles $nextState
    if ($Action -in 'UninstallAll','UninstallLegacy') {
        Remove-EmptyDirectory (Safe-Path $game "$bin/ue4ss/Mods/QDCoop/Scripts")
        Remove-EmptyDirectory (Safe-Path $game "$bin/ue4ss/Mods/QDCoop")
    }
    switch ($Action) {
        Install {
            Write-Host 'QD COOP M133 installed. Start QueensDomain from Steam.'
            if ($nextState.Files.Path -contains $legacyProxy) { Write-Host 'The UE4SS loader is suspended. Uninstall-Native.cmd restores it.' }
        }
        Uninstall { Write-Host 'Native setup removal complete.' }
        UninstallLegacy {
            Write-Host 'UE4SS QD COOP removal complete.'
            if ($nativePresent) { Write-Host 'The native installation and its shared game assets were kept.' }
        }
        UninstallAll {
            Remove-ExtractedNativePackage
            Write-Host 'QD COOP removal complete. Saves, save backups, and UE4SS were kept.'
        }
    }
} finally { $lock.Dispose() }
Remove-EmptyDirectory $stateDir
