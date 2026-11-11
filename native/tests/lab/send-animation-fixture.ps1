param([int]$Seconds=30,[uint64]$World=1,[uint32]$Generation=1)
$ErrorActionPreference='Stop'
$udp=[Net.Sockets.UdpClient]::new([Net.IPEndPoint]::new([Net.IPAddress]::Loopback,48761))
try {
 $until=[DateTime]::UtcNow.AddSeconds($Seconds);[uint64]$sequence=0
 while([DateTime]::UtcNow -lt $until){
  $sequence++;$stream=[IO.MemoryStream]::new();$w=[IO.BinaryWriter]::new($stream)
  $w.Write([uint32]0x45344e51);$w.Write([uint32]1)
  $w.Write([uint64]3819373611);$w.Write([uint64]1729814417);$w.Write($World)
  $w.Write([uint32]1);$w.Write([uint32]124)
  $w.Write([uint32]1);$w.Write($Generation);$w.Write($sequence)
  $w.Write([double]([Diagnostics.Stopwatch]::GetTimestamp()/[double][Diagnostics.Stopwatch]::Frequency))
  $values=@(37,1,0,1,0,1,16901,-7152,61,0,123,0,0,0,0,16901,-7152,61,0,0,0,1,1,1,1)
  foreach($value in $values){$w.Write([single]$value)}
  $w.Flush();$bytes=$stream.ToArray();if($bytes.Length -ne 164){throw 'Fixture packet size'}
  [void]$udp.Send($bytes,$bytes.Length,'127.0.0.1',48762)
  $anim=[IO.MemoryStream]::new();$aw=[IO.BinaryWriter]::new($anim)
  [byte[]]$header=$bytes[0..39];$header[32]=3;$header[36]=56;$aw.Write($header)
  $aw.Write([uint32]1);$aw.Write($Generation);$aw.Write($sequence)
  $aw.Write([double]([Diagnostics.Stopwatch]::GetTimestamp()/[double][Diagnostics.Stopwatch]::Frequency))
  $aw.Write([uint32]13);$aw.Write([uint32]0);$aw.Write([uint64]1);$aw.Write([uint32]21)
  $aw.Write([single]0);$aw.Write([single]1);$aw.Write([single]37.5);$aw.Flush()
  $ab=$anim.ToArray();if($ab.Length -ne 96){throw 'Animation packet size'}
  [void]$udp.Send($ab,$ab.Length,'127.0.0.1',48762);$aw.Dispose();$anim.Dispose()
  $w.Dispose();$stream.Dispose();Start-Sleep -Milliseconds 50
 }
 Write-Output "Sent $sequence scalar fixtures"
}finally{$udp.Dispose()}
