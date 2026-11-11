function Legacy-Target([string]$Path) {
    if ($Path -in $pakTargets -or $Path -in @(
        $legacyProxy,"$bin/ue4ss/LICENSE","$bin/ue4ss/UE4SS.dll","$bin/ue4ss/UE4SS-settings.ini",
        "$bin/ue4ss/Mods/mods.txt","$bin/ue4ss/Mods/QDCoop/enabled.txt",
        "$bin/ue4ss/Mods/shared/Types.lua","$bin/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua"
    )) { return $true }
    return $Path -match ('^' + [regex]::Escape("$bin/ue4ss/Mods/QDCoop/Scripts/") + '[a-zA-Z0-9_-][a-zA-Z0-9_.-]*$')
}

function Read-LegacyRows($Item,[int]$Depth=0) {
    if ($Depth -gt 32) { throw 'Legacy installation record is nested too deeply.' }
    if ($Item -is [array]) {
        foreach ($child in $Item) { Read-LegacyRows $child ($Depth + 1) }
        return
    }
    if ($Item -is [string]) {
        # Old Windows PowerShell installs serialized deeply nested entries as text.
        if ($Item -match '^@\{Path=(?<file>[^;]+); Backup=(?<backup>[^;]*); InstalledHash=(?<hash>[0-9a-fA-F]{64})\}$') {
            [pscustomobject]@{Path=$Matches.file;Backup=$Matches.backup;InstalledHash=$Matches.hash}
        }
    } elseif ($Item -and $Item.Path -is [string]) { $Item }
}

function Read-LegacyManifest {
    $manifest = Safe-Path $game 'QDCoop-Direct/InstallBackup/manifest.json'
    $grouped = @{}
    if (!(Test-Path -LiteralPath $manifest -PathType Leaf)) { return $grouped }
    $data = Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
    foreach ($row in (Read-LegacyRows $data)) {
        $path = $row.Path.Replace('\','/')
        if (!(Legacy-Target $path) -or !$row.InstalledHash -or !(Is-Hash $row.InstalledHash)) { continue }
        $backup = [string]$row.Backup
        if ($backup -and $backup.Replace('\','/') -notmatch '^InstallBackup/files/[0-9a-fA-F]{32}$') {
            throw "Invalid legacy backup path for $path"
        }
        if (!$grouped.ContainsKey($path)) { $grouped[$path] = [Collections.Generic.List[object]]::new() }
        $grouped[$path].Add($row)
    }
    return $grouped
}

function Read-ModList([string]$Path) {
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { return }
    $reader = [IO.StreamReader]::new($Path,[Text.UTF8Encoding]::new($false),$true)
    try { [pscustomobject]@{Text=$reader.ReadToEnd();Encoding=$reader.CurrentEncoding} }
    finally { $reader.Dispose() }
}

function Write-CleanModList([string]$Path,[string]$Before,$List) {
    $text = [regex]::Replace($List.Text,'(?im)^[ \t]*QDCoop[ \t]*:[^\r\n]*(?:\r\n|\n|$)','')
    $file = Safe-Path $stateDir ('mods.' + [guid]::NewGuid().ToString('N') + '.tmp')
    try {
        [IO.File]::WriteAllText($file,$text,$List.Encoding)
        $hash = File-Hash $file
        Store-File $file $hash
        Add-Change $Path $Before $hash
    } finally { if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file } }
}

function Archive-LegacyScript([string]$Source,[string]$Hash) {
    $archive = Safe-Path $game 'QDCoop-Direct/InstallBackup/removed'
    $null = [IO.Directory]::CreateDirectory($archive)
    $target = Safe-Path $archive ((Split-Path $Source -Leaf) + '.' + $Hash)
    if (!(Test-Path -LiteralPath $target)) {
        $temp = Safe-Path $archive ([guid]::NewGuid().ToString('N') + '.tmp')
        try {
            [IO.File]::Copy($Source,$temp)
            if ((File-Hash $temp) -ne $Hash) { throw "Script changed during backup: $Source" }
            [IO.File]::Move($temp,$target)
        } finally { if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp } }
    }
    if ((File-Hash $target) -ne $Hash) { throw "Edited script backup differs: $target" }
    Write-Host "Saved edited QD COOP script in $target"
}

function Legacy-RestoreHash([string]$Path,$History) {
    if (!$History[0].Backup) { return '' }
    $backup = Safe-Path $game ('QDCoop-Direct/' + $History[0].Backup.Replace('\','/'))
    $hash = File-Hash $backup
    if (!$hash) { throw "Original legacy backup missing: $Path" }
    if ($hash -in $History.InstalledHash) { return '' }
    Store-File $backup $hash
    return $hash
}

function Uninstall-Legacy($NativeState,[bool]$NativePresent) {
    $legacy = Read-LegacyManifest
    $mods = "$bin/ue4ss/Mods"
    $listPath = "$mods/mods.txt"
    $marker = "$mods/QDCoop/enabled.txt"
    if (!$legacy.Count) {
        $scripts = Safe-Path $game "$mods/QDCoop/Scripts"
        if ((Test-Path -LiteralPath (Safe-Path $game $marker)) -or
            (Get-ChildItem -LiteralPath $scripts -File -ErrorAction SilentlyContinue | Select-Object -First 1)) {
            throw 'UE4SS QD COOP files exist, but there is no usable installation record.'
        }
        return
    }
    $list = Read-ModList (Safe-Path $game $listPath)
    $orderedPaths = @($marker,$listPath) + @($legacy.Keys | Where-Object { $_ -notin @($listPath,$marker) } | Sort-Object)
    foreach ($path in $orderedPaths) {
        $own = $path -in $pakTargets -or $path.StartsWith("$mods/QDCoop/",[StringComparison]::OrdinalIgnoreCase)
        if (!$own -and $path -ne $listPath) { continue }
        $history = $legacy[$path]
        if ($NativePresent -and $path -in $pakTargets) {
            # Keep the live PAK. Rebase native rollback past the removed legacy copy.
            $row = $NativeState.Files | Where-Object Path -eq $path | Select-Object -First 1
            if ($row.Original -and $row.Original -in $history.InstalledHash) {
                $row.Original = Legacy-RestoreHash $path $history
            }
            continue
        }
        $destination = Safe-Path $game $path
        $current = File-Hash $destination
        if (!$current) { continue }
        if ($path -eq $marker) { Add-Change $path $current ''; continue }
        $installed = $history | Where-Object InstalledHash -eq $current | Select-Object -First 1
        if ($path -eq $listPath) {
            if ($list -and $list.Text -match '(?im)^[ \t]*QDCoop[ \t]*:') { Write-CleanModList $path $current $list }
            continue
        }
        if (!$installed) {
            if ($history -and $path.StartsWith("$mods/QDCoop/Scripts/",[StringComparison]::OrdinalIgnoreCase)) {
                Archive-LegacyScript $destination $current
                Add-Change $path $current ''
                continue
            }
            if ($history) { Write-Warning "Changed since installation; kept: $destination" }
            continue
        }
        $restore = Legacy-RestoreHash $path $history
        Add-Change $path $current $restore
    }
    Write-Host 'UE4SS and its shared files were kept.'
}
