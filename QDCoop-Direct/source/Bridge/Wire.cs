using System.Buffers.Binary;
using System.IO.Compression;
using System.Net;
using System.Security.Cryptography;
using System.Text.Json;

// Authenticated application fragments stay below an ordinary IPv4/VPN MTU.
// Reassembly is bounded and expires; incomplete snapshots never reach Lua.
sealed class Wire(byte[] key) {
 public const int DatagramLimit=1200;
 const int Header=32,Tag=16,Part=DatagramLimit-Header-Tag,MaxBody=32000;
 readonly ulong sender=BitConverter.ToUInt64(RandomNumberGenerator.GetBytes(8));
 readonly Dictionary<string,Assembly> incoming=[];
 public long Expired,Fragments;
 public IEnumerable<byte[]> Encode(Message message){
  byte[] raw=JsonSerializer.SerializeToUtf8Bytes(message),body=raw;byte compressed=0;
  using(var output=new MemoryStream()) {using(var zip=new BrotliStream(output,CompressionLevel.Fastest,true))zip.Write(raw);if(output.Length+16<raw.Length){body=output.ToArray();compressed=1;}}
  if(body.Length>MaxBody)yield break;
  int count=(body.Length+Part-1)/Part;
  for(int i=0;i<count;i++){
   int size=Math.Min(Part,body.Length-i*Part);var packet=new byte[Header+size+Tag];var h=packet.AsSpan();
   "QDC2"u8.CopyTo(h);BinaryPrimitives.WriteUInt64LittleEndian(h[4..],sender);BinaryPrimitives.WriteInt64LittleEndian(h[12..],message.Seq);
   BinaryPrimitives.WriteUInt16LittleEndian(h[20..],(ushort)i);BinaryPrimitives.WriteUInt16LittleEndian(h[22..],(ushort)count);BinaryPrimitives.WriteInt32LittleEndian(h[24..],body.Length);h[28]=compressed;
   body.AsSpan(i*Part,size).CopyTo(h[Header..]);HMACSHA256.HashData(key,h[..^Tag]).AsSpan(0,Tag).CopyTo(h[^Tag..]);yield return packet;
  }
 }
 public Message? Decode(byte[] packet,IPEndPoint endpoint,long now){
  foreach(var stale in incoming.Where(v=>now-v.Value.At>2000).Select(v=>v.Key).ToArray()){incoming.Remove(stale);Expired++;}
  var h=packet.AsSpan();if(h.Length<Header+Tag+1||h.Length>DatagramLimit||!h[..4].SequenceEqual("QDC2"u8))return null;
  if(!CryptographicOperations.FixedTimeEquals(h[^Tag..],HMACSHA256.HashData(key,h[..^Tag]).AsSpan(0,Tag)))return null;
  long sequence=BinaryPrimitives.ReadInt64LittleEndian(h[12..]);int i=BinaryPrimitives.ReadUInt16LittleEndian(h[20..]),n=BinaryPrimitives.ReadUInt16LittleEndian(h[22..]),length=BinaryPrimitives.ReadInt32LittleEndian(h[24..]);
  int bytes=h.Length-Header-Tag;if(sequence<=0||length<=0||length>MaxBody||n!=(length+Part-1)/Part||i>=n||bytes!=Math.Min(Part,length-i*Part)||h[28]>1)return null;
  string id=$"{endpoint}/{BinaryPrimitives.ReadUInt64LittleEndian(h[4..])}/{sequence}";byte[] body;
  if(n==1)body=h.Slice(Header,bytes).ToArray();else{
   Fragments++;
   if(!incoming.TryGetValue(id,out var a)){if(incoming.Count>=128){var oldest=incoming.MinBy(v=>v.Value.At);incoming.Remove(oldest.Key);Expired++;}a=new(length,n,h[28],now);incoming.Add(id,a);}
   if(a.Data.Length!=length||a.Parts.Length!=n||a.Compressed!=h[28])return null;
   if(!a.Parts[i]){h.Slice(Header,bytes).CopyTo(a.Data.AsSpan(i*Part));a.Parts[i]=true;a.Count++;}
   if(a.Count!=n)return null;body=a.Data;incoming.Remove(id);
  }
  if(h[28]==1){using var zip=new BrotliStream(new MemoryStream(body),CompressionMode.Decompress);using var output=new MemoryStream();byte[] chunk=new byte[4096];int got;while((got=zip.Read(chunk))>0){if(output.Length+got>MaxBody)return null;output.Write(chunk,0,got);}body=output.ToArray();}
  var message=JsonSerializer.Deserialize<Message>(body);return message?.Seq==sequence?message:null;
 }
 sealed class Assembly(int length,int count,byte compressed,long at){public byte[] Data=new byte[length];public bool[] Parts=new bool[count];public byte Compressed=compressed;public long At=at;public int Count;}
}
