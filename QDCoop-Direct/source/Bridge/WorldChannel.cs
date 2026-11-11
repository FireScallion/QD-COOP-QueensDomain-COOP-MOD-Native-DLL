using System.Security.Cryptography;
using System.Text;

// Independently acknowledged snapshot chunks. Never expose a partially received
// world to the game, and never let an old transfer replace a newer generation.
sealed class WorldChannel {
 const int Limit=1048576,ChunkSize=650,MaxChunks=1614;
 string txText="",txHash="",rxHash="",completedHash="";long generation=0,rxGeneration=-1;
 string[] txChunks=[],rxChunks=[];long[] sent=[];bool[] acked=[];int received=0;
 public long NextWork {get;private set;}=long.MaxValue;
 public void Reset(){txText=txHash=rxHash=completedHash="";generation=0;rxGeneration=-1;txChunks=rxChunks=[];sent=[];acked=[];received=0;NextWork=long.MaxValue;}
 public void Publish(string text){
  if(text==txText||text.Length>Limit||!text.StartsWith("QDW1|")||!text.EndsWith("\nEND\n")||text.Any(c=>c>127))return;
  txText=text;txHash=Convert.ToHexString(SHA256.HashData(Encoding.ASCII.GetBytes(text)));generation++;
  txChunks=Enumerable.Range(0,(text.Length+ChunkSize-1)/ChunkSize).Select(i=>text.Substring(i*ChunkSize,Math.Min(ChunkSize,text.Length-i*ChunkSize))).ToArray();
  sent=Enumerable.Repeat(-10000L,txChunks.Length).ToArray();acked=new bool[txChunks.Length];
  NextWork=0;
 }
 public void Tick(long now,Action<string> send){if(now<NextWork)return;int budget=8;for(int i=0;i<txChunks.Length&&budget>0;i++)if(!acked[i]&&now-sent[i]>=250){send($"{generation}|{txHash}|{i}|{txChunks.Length}|{txChunks[i]}");sent[i]=now;budget--;}NextWork=long.MaxValue;for(int i=0;i<sent.Length;i++)if(!acked[i])NextWork=Math.Min(NextWork,Math.Max(now+2,sent[i]+250));}
 public void Acknowledge(string text){var f=text.Split('|');if(f.Length==3&&long.TryParse(f[0],out var g)&&g==generation&&f[1]==txHash&&int.TryParse(f[2],out var i)&&i>=0&&i<acked.Length)acked[i]=true;}
 public void Receive(string text,long now,Action<string> ack,Func<string,bool> commit){
  var f=text.Split('|',5);if(f.Length!=5||!long.TryParse(f[0],out var g)||g<1||g<rxGeneration||f[1].Length!=64||!f[1].All(Uri.IsHexDigit)||!int.TryParse(f[2],out var i)||!int.TryParse(f[3],out var n)||n<1||n>MaxChunks||i<0||i>=n||f[4].Length>ChunkSize||f[4].Any(c=>c>127))return;
  if(g>rxGeneration){rxGeneration=g;rxHash=f[1];rxChunks=new string[n];received=0;}
  if(f[1]!=rxHash||rxChunks.Length!=n)return;
  if(rxChunks[i]==null){rxChunks[i]=f[4];received++;}
  else if(rxChunks[i]!=f[4])return;
  if(received==n&&completedHash!=rxHash){
   var data=string.Concat(rxChunks);if(data.Length>Limit||Convert.ToHexString(SHA256.HashData(Encoding.ASCII.GetBytes(data)))!=rxHash||!data.StartsWith("QDW1|")||!data.EndsWith("\nEND\n"))return;
   // The game may briefly hold the mailbox open on Windows. Do not acknowledge
   // the final chunk until atomic publication succeeds; its retry commits again.
   if(!commit(data))return;completedHash=rxHash;
  }
  ack($"{g}|{f[1]}|{i}");
 }
}
