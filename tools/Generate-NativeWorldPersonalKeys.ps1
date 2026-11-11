$ErrorActionPreference='Stop'
$source=Join-Path $PSScriptRoot '../QDCoop-Direct/mod/Scripts'
$target=Join-Path $PSScriptRoot '../native/include/qdcoop/world_personal_keys.inc'
$names=[regex]::Matches((Get-Content -LiteralPath (Join-Path $source 'personal_progress_names.lua') -Raw),'\["([A-Za-z0-9_]+)"\]=true')
$guidLines=[regex]::Matches((Get-Content -LiteralPath (Join-Path $source 'personal_pickup_guids.lua') -Raw),'\["(-?\d+),(-?\d+),(-?\d+),(-?\d+)"\]=true')
if($names.Count -lt 20 -or $guidLines.Count -lt 1000){throw 'Unexpected preview3 personal key tables'}
$guids=@(foreach($match in $guidLines){
 [pscustomobject]@{a=[int]$match.Groups[1].Value;b=[int]$match.Groups[2].Value;c=[int]$match.Groups[3].Value;d=[int]$match.Groups[4].Value}
}) | Sort-Object a,b,c,d -Unique
if($guids.Count -ne $guidLines.Count){throw 'Duplicate pickup GUID'}
@(
 '// Generated from preview3 personal_progress_names.lua and personal_pickup_guids.lua.'
 'inline constexpr std::string_view personal_names[]{'
 foreach($match in $names){'    "'+$match.Groups[1].Value+'",'}
 '};'
 'inline constexpr Guid personal_guids[]{'
 foreach($guid in $guids){"    {$($guid.a),$($guid.b),$($guid.c),$($guid.d)},"}
 '};'
) | Set-Content -LiteralPath $target -Encoding utf8NoBOM
"Generated $($names.Count) names and $($guids.Count) GUIDs"
