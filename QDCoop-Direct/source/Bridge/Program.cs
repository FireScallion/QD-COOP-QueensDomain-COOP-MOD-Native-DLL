using System.Net;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

// Prototype transport only. Unreal objects stay on the game thread in the Lua adapter.
// Payloads are bounded data records, never executable code. One host and one guest.
if(args.Length==2 && args[0]=="--steam-service") return SteamService.Run(args[1]);
if(args.Length != 1) { Console.Error.WriteLine("Usage: QDCoopBridge <config.json>"); return 2; }
var cfg=JsonSerializer.Deserialize<Config>(File.ReadAllText(args[0])) ?? throw new Exception("Missing config");
long nextOwnerCheck=0;
long nextSteamReleaseCheck=0;string lastSteamRelease="";
bool OwnerAlive() {
 if(cfg.OwnerPid==0)return true; // Standalone protocol tests have no launcher.
 if(Environment.TickCount64<nextOwnerCheck)return true;
 nextOwnerCheck=Environment.TickCount64+250;
 try {using var p=System.Diagnostics.Process.GetProcessById(cfg.OwnerPid);return !p.HasExited&&p.StartTime.ToUniversalTime().Ticks==cfg.OwnerStartTicks;}catch{return false;}
}
if(!OwnerAlive())return 0;
if(cfg.Role is not ("host" or "client") || cfg.Key.Length<8 || cfg.Port is <1024 or >65535) throw new Exception("Invalid configuration");
Directory.CreateDirectory(cfg.Directory);
using var mutex=new Mutex(true,"Local\\QDCoopBridge_"+Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(Path.GetFullPath(cfg.Directory))))[..24],out var created);
if(!created) throw new Exception("Bridge already running for this profile");
using var mailbox=new MemoryMailbox(cfg.IpcChannel);
mailbox.DiscardEvents(0);
var key=Encoding.UTF8.GetBytes(cfg.Key);var wire=new Wire(key);
var identity=Guid.NewGuid().ToString("N");
var session=cfg.Role=="host"?Guid.NewGuid().ToString("N"):"";
using var udp=new UdpClient(new IPEndPoint(IPAddress.Any,cfg.Role=="host"?cfg.Port:0));
udp.Client.ReceiveBufferSize=4*1024*1024;udp.Client.SendBufferSize=1024*1024;
// Windows reports an ICMP port-unreachable as WSAECONNRESET on an unconnected
// UDP socket. A peer starting or restarting must not terminate this process.
if(OperatingSystem.IsWindows()) udp.Client.IOControl(unchecked((int)0x9800000C),new byte[]{0,0,0,0},null);
using var wake=new NetworkWake(udp.Client,mailbox.Ready);
var host=cfg.Role=="client"?new IPEndPoint(IPAddress.Parse(cfg.Host),cfg.Port):null;
IPEndPoint? peer=null;
string peerId="";
string lastStatePayload="";
string lastVisualPayload="";
string? parsedEnemyText=null;
(string Key,string Text)[] parsedEnemy=[];long enemyParses=0;
long lastVisualSeq=-1,lastVisualAt=-10000;
var clock=System.Diagnostics.Stopwatch.StartNew();
long seq=0,lastSeen=-10000,lastHello=-10000,lastHeartbeat=0,lastState=0,lastStatus=-10000,lastStateSeq=-1,lastPose=-10000;
var pending=new Dictionary<long,Pending>();
var outgoing=new EventJournal(Path.Combine(cfg.Directory,"out.events"),mailbox.Enabled?()=>mailbox.EventRevision(0):null);
long nextEvents=0,nextStopCheck=0,nextEnemyHeartbeat=0,loops=0;
var enemyRx=new Dictionary<string,(long Seq,long At,string Text)>();
var enemyTx=new EnemySendCache();bool enemyDirty=false;
long lastRosterRead=-1000,lastRosterSeq=-1;
var cosmetic=new Dictionary<long,Pending>();double rtt=150;long lastPingSeq=0,lastPingAt=0;
long lastEnemyRead=-1000,lastEnemyFlush=-1000;
var worldChannel=new WorldChannel();
long lastWorldRead=-1000;
string? EnemyKey(string s){if(s.Length>6900||s.Contains('\n')||s.Contains('\r'))return null;var f=s.Split('|',6);return f.Length==6&&f[0]=="N"&&f[1].Length<=80&&int.TryParse(f[3],out var id)&&id>0&&id<=100000?f[1]+":"+id:null;}
var received=new HashSet<long>();
var receivedOrder=new Queue<long>();
long rx=0,tx=0,retries=0,rejected=0,duplicate=0;
bool running=true;
Console.CancelKeyPress+=(_,e)=>{e.Cancel=true;running=false;};
foreach(var f in new[]{"out.events","in.events","out.state","in.state","out.visual","in.visual","out.enemy","in.enemy","in.world","in.roster"}) Atomic(f,"");
void Log(string message) { var l=$"{DateTime.UtcNow:O} {message}";Console.WriteLine(l);File.AppendAllText(Path.Combine(cfg.Directory,"bridge.log"),l+"\n"); }
bool Atomic(string name,string text) { if(mailbox.Handles(name)){bool ok=name.StartsWith("out.")?mailbox.DiscardOutgoing(name):mailbox.Write(name,text);if(name!="status.txt"||!ok)return ok;}var p=Path.Combine(cfg.Directory,name);try {File.WriteAllText(p+".tmp",text,new UTF8Encoding(false));File.Move(p+".tmp",p,true);if(name=="out.events")mailbox.NotifyEvents(0);if(name=="in.events")mailbox.NotifyEvents(1);return true;}catch(Exception e) when(e is IOException or UnauthorizedAccessException){return false;} }
void Send(Message m,IPEndPoint dest) {
 foreach(var packet in wire.Encode(m))try {udp.Send(packet,dest);tx++;} catch(SocketException){rejected++;}
}
Message Make(string kind,string payload="",long ack=0)=>new(1,cfg.Build,identity,session,++seq,kind,payload,ack);
void Disconnect(string why) { if(peer!=null)Log("DISCONNECTED "+why);peer=null;peerId="";lastStatePayload="";lastVisualPayload="";parsedEnemyText=null;parsedEnemy=[];lastVisualSeq=-1;lastVisualAt=-10000;pending.Clear();cosmetic.Clear();lastRosterSeq=-1;received.Clear();receivedOrder.Clear();enemyRx.Clear();enemyTx.Clear();worldChannel.Reset();Atomic("in.world","");Atomic("in.roster","");Atomic("in.enemy","");lastStateSeq=-1;Atomic("in.state","");Atomic("in.visual","");if(cfg.Role=="client")session=""; }
void Accept(Message m,IPEndPoint from) {
 if(m.Version!=1||m.Build!=cfg.Build||m.Id==identity||m.Id is null||m.Id.Length!=32||!m.Id.All(Uri.IsHexDigit)||m.Payload is null||m.Payload.Length>7000||m.Seq<=0||m.Session is null){rejected++;return;}
 var now=clock.ElapsedMilliseconds;
 if(cfg.Role=="host"&&m.Kind=="HELLO") {
  if(peer!=null&&(peerId!=m.Id||!peer.Equals(from))&&now-lastSeen<20000){rejected++;return;}
  if(peerId!=m.Id){Disconnect("new guest");peer=from;peerId=m.Id;Log("CONNECTED guest "+from);}
  lastSeen=now;Send(Make("WELCOME"),from);return;
 }
 if(cfg.Role=="client"&&m.Kind=="WELCOME"&&from.Equals(host)) {
  if(session!=m.Session||peerId!=m.Id){Disconnect("new host session");session=m.Session;peer=from;peerId=m.Id;Log("CONNECTED host "+from);}
  lastSeen=now;return;
 }
 if(peer==null||!peer.Equals(from)||peerId!=m.Id||m.Session!=session){rejected++;return;}
 lastSeen=now;
 switch(m.Kind) {
  case "GOODBYE":Disconnect("peer left room");break;
  case "PING":Send(Make("PONG","",m.Seq),from);break;
  case "PONG":if(m.Ack==lastPingSeq)rtt=rtt*.8+Math.Min(2000,now-lastPingAt)*.2;break;
  case "ROSTER":if(cfg.Role=="client"&&m.Seq>lastRosterSeq&&m.Payload.StartsWith("QDER1|")&&m.Payload.EndsWith("\nEND\n")){lastRosterSeq=m.Seq;Atomic("in.roster",m.Payload);}break;
  case "WORLD":if(cfg.Role=="client")worldChannel.Receive(m.Payload,now,ack=>Send(Make("WORLDACK",ack),from),data=>Atomic("in.world",data));break;
  case "WORLDACK":if(cfg.Role=="host")worldChannel.Acknowledge(m.Payload);break;
  case "ENEMY":if(cfg.Role=="client"&&EnemyKey(m.Payload) is string ek&&(!enemyRx.TryGetValue(ek,out var old)||m.Seq>old.Seq)&&(enemyRx.ContainsKey(ek)||enemyRx.Count<256)){enemyRx[ek]=(m.Seq,now,m.Payload);enemyDirty=true;}break;
  case "STATE":if(m.Seq>lastStateSeq&&m.Payload.StartsWith("QDCP1\n")&&m.Payload.EndsWith("\nEND\n")){lastStateSeq=m.Seq;lastPose=now;Atomic("in.state",m.Payload);}break;
  case "VISUAL":if(m.Seq>lastVisualSeq&&m.Payload.StartsWith("QDVF1\n")&&m.Payload.EndsWith("\nEND\n")){lastVisualSeq=m.Seq;lastVisualAt=now;Atomic("in.visual",m.Payload);}break;
  case "ACK":pending.Remove(m.Ack);cosmetic.Remove(m.Ack);break;
  case "EVENT":
   if(m.Payload.Length>1000||m.Payload.Contains('\n')||m.Payload.Contains('\r')){rejected++;return;}
   if(!received.Contains(m.Seq)) {
    // Persist to the local mailbox before acknowledging delivery.
    if(mailbox.Enabled&&MemoryMailbox.IsCosmetic(m.Payload)){if(!mailbox.PushEvent(1,m.Id+"-"+m.Seq+"|"+m.Payload+"\n"))return;}else{
     File.AppendAllText(Path.Combine(cfg.Directory,"in.events"),m.Id+"-"+m.Seq+"|"+m.Payload+"\n",new UTF8Encoding(false));mailbox.NotifyEvents(1);
    }
    received.Add(m.Seq);receivedOrder.Enqueue(m.Seq);
    while(receivedOrder.Count>4096)received.Remove(receivedOrder.Dequeue());
   } else duplicate++;
   Send(Make("ACK","",m.Seq),from);break;
 }
}
bool Fresh(string name)=>mailbox.Handles(name)?mailbox.Fresh(name):DateTime.UtcNow-File.GetLastWriteTimeUtc(Path.Combine(cfg.Directory,name))<TimeSpan.FromSeconds(3);
string? ReadShared(string name) { if(mailbox.Handles(name))return mailbox.Read(name);try{using var f=new FileStream(Path.Combine(cfg.Directory,name),FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete);using var r=new StreamReader(f);return r.ReadToEnd();}catch(Exception e) when(e is IOException or UnauthorizedAccessException){return null;} }
Log("START "+cfg.Role+" local="+udp.Client.LocalEndPoint+" ipc="+(mailbox.Enabled?"memory":"file"));
double RetryDelay(Pending p)=>Math.Clamp(rtt*1.6+40,120,1200)*Math.Min(4,1+p.Attempts*.35);
long RetryAt(){long at=long.MaxValue;foreach(var p in pending.Values)at=Math.Min(at,p.SentAt+(long)Math.Ceiling(RetryDelay(p)));foreach(var p in cosmetic.Values)at=Math.Min(at,Math.Min(p.Created+1201,p.SentAt+(long)Math.Ceiling(RetryDelay(p))));return at;}
while(running&&OwnerAlive()) {
 loops++;var now=clock.ElapsedMilliseconds;
 if(now>=nextStopCheck){if(File.Exists(Path.Combine(cfg.Directory,"stop.bridge")))break;nextStopCheck=now+250;}
 uint dirty=mailbox.DirtyMask;
 for(int i=0;i<128&&udp.Available>0;i++) {
  IPEndPoint from=new(IPAddress.Any,0);
  try {
   var m=wire.Decode(udp.Receive(ref from),from,now);if(m!=null){rx++;Accept(m,from);}
  }catch(Exception e) when(e is JsonException or FormatException or ArgumentException or IOException or UnauthorizedAccessException or SocketException or InvalidDataException){rejected++;}
 }
 if(peer!=null&&now-lastSeen>20000)Disconnect("timeout");
 if(cfg.Role=="host"&&now>=nextSteamReleaseCheck){
  nextSteamReleaseCheck=now+250;
  try{
   string controlPath=Path.Combine(cfg.Directory,"steam-peer-left.txt");
   string control=File.Exists(controlPath)?File.ReadAllText(controlPath):"";
   if(control!=lastSteamRelease){lastSteamRelease=control;var f=control.Split('|');
    if(f.Length==3&&f[0]==session&&f[1]==peerId&&peer!=null&&IPAddress.IsLoopback(peer.Address)&&int.TryParse(f[2],out int port)&&peer.Port==port)Disconnect("Steam guest left lobby");
   }
  }catch(Exception e)when(e is IOException or UnauthorizedAccessException){}
 }
 foreach(var k in enemyRx.Where(x=>now-x.Value.At>3000).Select(x=>x.Key).ToArray()){enemyRx.Remove(k);enemyDirty=true;}
 if(enemyDirty&&now-lastEnemyFlush>=50){Atomic("in.enemy",string.Join('\n',enemyRx.Values.Select(x=>x.Text))+"\n");enemyDirty=false;lastEnemyFlush=now;}
 if(cfg.Role=="client"&&peer==null&&now-lastHello>=500){Send(Make("HELLO"),host!);lastHello=now;}
 if(peer!=null) {
  try {
  if(cfg.Role=="host"){
   if(now-lastWorldRead>=100&&(dirty&(1u<<8))!=0){lastWorldRead=now;var data=ReadShared("out.world");if(data!=null)worldChannel.Publish(data);}
   worldChannel.Tick(now,payload=>Send(Make("WORLD",payload),peer));
  }
  if(now-lastHeartbeat>=1000){var ping=Make("PING");lastPingSeq=ping.Seq;lastPingAt=now;Send(ping,peer);lastHeartbeat=now;}
  if(cfg.Role=="host"&&now-lastRosterRead>=500){lastRosterRead=now;var roster=ReadShared("out.roster");if(roster!=null&&roster.Length<=7000&&roster.StartsWith("QDER1|")&&Fresh("out.roster"))Send(Make("ROSTER",roster),peer);}
  if(cfg.Role=="host"&&now-lastEnemyRead>=25&&((dirty&(1u<<4))!=0||now>=nextEnemyHeartbeat)){lastEnemyRead=now;var data=ReadShared("out.enemy");nextEnemyHeartbeat=now+500;if(data!=null&&data.Length<=450000&&Fresh("out.enemy")){if(data!=parsedEnemyText){parsedEnemyText=data;var batch=new List<(string,string)>();foreach(var line in data.Split('\n').Take(64)){var k=EnemyKey(line);if(k!=null)batch.Add((k,line));}parsedEnemy=batch.ToArray();enemyParses++;}foreach(var e in parsedEnemy){if(enemyTx.ShouldSend(e.Key,e.Text,now))Send(Make("ENEMY",e.Text),peer);nextEnemyHeartbeat=Math.Min(nextEnemyHeartbeat,enemyTx.Due(e.Key));}enemyTx.Trim(now);}}
  // Forward each fresh game sample promptly. Do not impose another 50 ms
  // sampling clock or retransmit an unchanged pose under a new sequence.
  if(now-lastState>=10&&(dirty&5)!=0){var state=ReadShared("out.state");if(state!=null&&state!=lastStatePayload&&Fresh("out.state")&&state.Length<=7000&&state.StartsWith("QDCP1\n")&&state.EndsWith("\nEND\n")){Send(Make("STATE",state),peer);lastStatePayload=state;}
   var visual=ReadShared("out.visual");if(visual!=null&&visual!=lastVisualPayload&&Fresh("out.visual")&&visual.Length<=7000&&visual.StartsWith("QDVF1\n")&&visual.EndsWith("\nEND\n")){Send(Make("VISUAL",visual),peer);lastVisualPayload=visual;}lastState=now;}
  if(now>=nextEvents&&(outgoing.HasPending||mailbox.HasEvents(0))){nextEvents=now+10;
   try {foreach(var line in outgoing.Read(pending.Count<512?32:0).Concat(mailbox.DrainEvents(0,32))) {
    if(line.Length==0||line.Length>1000)continue;
    var m=Make("EVENT",line);bool visual=MemoryMailbox.IsCosmetic(line);
    if(visual){if(cosmetic.Count>=128){var oldest=cosmetic.MinBy(v=>v.Value.Created);cosmetic.Remove(oldest.Key);}cosmetic[m.Seq]=new(m,now,now,0);}else pending.Add(m.Seq,new(m,now,now,0));Send(m,peer);
   }}catch(Exception e) when(e is IOException or UnauthorizedAccessException){ /* Retry the journal on the next poll. */ }
  }
  int retryBudget=8;
  if(now>=RetryAt())foreach(var queue in new[]{pending,cosmetic})foreach(var pair in queue.OrderBy(v=>v.Value.SentAt).ToArray()) {
   if(ReferenceEquals(queue,cosmetic)&&now-pair.Value.Created>1200){queue.Remove(pair.Key);continue;}
   double delay=RetryDelay(pair.Value);
   if(retryBudget>0&&now-pair.Value.SentAt>=delay){Send(pair.Value.Message,peer);queue[pair.Key]=pair.Value with{SentAt=now,Attempts=pair.Value.Attempts+1};retries++;retryBudget--;}
  }
  }catch(Exception e) when(e is IOException or UnauthorizedAccessException){rejected++;}
 } else { // Never replay attacks generated while disconnected into a later session.
  try {outgoing.SkipExisting();mailbox.DiscardEvents(0);}catch(Exception e) when(e is IOException or UnauthorizedAccessException){}
 }
 if(now-lastStatus>=250){if(now-lastPose>3000)Atomic("in.state","");if(now-lastVisualAt>3000)Atomic("in.visual","");Atomic("status.txt",$"{(peer!=null?"connected":"waiting")}|{session}|{peerId}|{rx}|{tx}|{pending.Count}|{retries}|{rejected}|{duplicate}|{DateTimeOffset.UtcNow.ToUnixTimeSeconds()}\n");Atomic("transport-status.txt",$"rttMs={rtt:F1} critical={pending.Count} cosmetic={cosmetic.Count} fragments={wire.Fragments} expiredFragments={wire.Expired} maxDatagram={Wire.DatagramLimit} ipc={(mailbox.Enabled?"memory":"file")} memoryReads={mailbox.Reads} memoryWrites={mailbox.Writes} cosmeticReads={mailbox.CosmeticReads} cosmeticWrites={mailbox.CosmeticWrites} loops={loops} waits={wake.Waits} enemyParses={enemyParses}\n");lastStatus=now;}
 // Wait for newly published local data, socket readiness, or the earliest real
 // deadline. Game callbacks never enter this wait. Retry/heartbeat deadlines
 // remain independent of whether another snapshot happens to arrive.
 long due=Math.Min(nextStopCheck,lastStatus+250);
 if(peer==null){if(cfg.Role=="client")due=Math.Min(due,lastHello+500);}
 else {
  dirty=mailbox.DirtyMask;
  due=Math.Min(due,Math.Min(lastHeartbeat+1000,Math.Min(lastSeen+20001,RetryAt())));
  if((dirty&5)!=0)due=Math.Min(due,lastState+10);
  if(outgoing.HasPending||mailbox.HasEvents(0))due=Math.Min(due,nextEvents);
  if(cfg.Role=="host"){
   due=Math.Min(due,Math.Min(lastRosterRead+500,worldChannel.NextWork));
   if((dirty&(1u<<8))!=0)due=Math.Min(due,lastWorldRead+100);
   due=Math.Min(due,(dirty&(1u<<4))!=0?lastEnemyRead+25:Math.Max(lastEnemyRead+25,nextEnemyHeartbeat));
  }
  if(enemyDirty)due=Math.Min(due,lastEnemyFlush+50);
  foreach(var e in enemyRx.Values)due=Math.Min(due,e.At+3001);
 }
 wake.Wait((int)Math.Clamp(due-clock.ElapsedMilliseconds,0,250));
}
if(peer!=null){var goodbye=Make("GOODBYE");for(int i=0;i<3;i++)Send(goodbye,peer);}
Atomic("status.txt","stopped||||||||\n");Log("STOP");return 0;

record Config(string Role,string Directory,int Port,string Host,string Key,string Build,int OwnerPid=0,long OwnerStartTicks=0,string? IpcChannel=null);
record Message(int Version,string Build,string Id,string Session,long Seq,string Kind,string Payload,long Ack);
record Pending(Message Message,long SentAt,long Created,int Attempts);

