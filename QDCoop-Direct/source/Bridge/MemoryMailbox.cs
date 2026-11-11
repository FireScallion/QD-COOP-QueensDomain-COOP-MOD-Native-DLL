using System.Runtime.InteropServices;
using System.Text;

// Same helper and layout as the game worker. Empty name is the explicit file
// fallback for rollback/protocol tests; configured memory failure is surfaced.
sealed class MemoryMailbox : IDisposable
{
    readonly IntPtr handle;
    readonly byte[] buffer = new byte[2 * 1024 * 1024];
    readonly ulong[] revisions = new ulong[11], written = new ulong[11];
    readonly string?[] values = new string?[11];
    static readonly string[] Names = ["out.state", "in.state", "out.visual", "in.visual", "out.enemy", "in.enemy", "out.roster", "in.roster", "out.world", "in.world", "status.txt"];
    readonly byte[] eventBuffer = new byte[65536];
    public long CosmeticReads { get; private set; }
    public long CosmeticWrites { get; private set; }
    public IntPtr Ready => Enabled ? qdmail_ready(handle, 0) : IntPtr.Zero;
    public uint DirtyMask => Enabled ? qdmail_changed(handle,revisions,revisions.Length) : uint.MaxValue;
    public bool HasEvents(int side)=>Enabled&&qdmail_pending(handle,side)!=0;
    public bool Enabled => handle != IntPtr.Zero;
    public long Reads { get; private set; }
    public long Writes { get; private set; }
    public MemoryMailbox(string? name)
    {
        if (!string.IsNullOrEmpty(name))
        {
            handle = qdmail_open(name);
            if (handle == IntPtr.Zero) throw new IOException("Cannot open configured QDCoop shared memory mailbox");
        }
    }
    public bool Handles(string name) => Enabled && Array.IndexOf(Names, name) >= 0;
    public bool Write(string name, string value)
    {
        int id = Array.IndexOf(Names, name);
        if (!Enabled || id < 0 || (value.Length > buffer.Length / 3 && Encoding.UTF8.GetByteCount(value) > buffer.Length)) return false;
        int n = Encoding.UTF8.GetBytes(value, buffer);
        bool ok = qdmail_write(handle, id, buffer, n) == 1;
        if (ok) Writes++;
        return ok;
    }
    public bool DiscardOutgoing(string name)
    {
        // Only the game writes outbound slots. Bridge startup discards its old
        // read baseline locally instead of racing the game's publisher.
        int id = Array.IndexOf(Names, name);
        if (!Enabled || id < 0 || (id & 1) != 0) return false;
        Read(name); values[id] = null; written[id] = 0;
        return true;
    }
    public string? Read(string name)
    {
        int id = Array.IndexOf(Names, name);
        if (!Enabled || id < 0) return null;
        int result = qdmail_read(handle, id, buffer, buffer.Length, ref revisions[id], out int n, out ulong at);
        if (result == 1) { values[id] = Encoding.UTF8.GetString(buffer, 0, n); written[id] = at; Reads++; }
        return values[id];
    }
    public bool Fresh(string name)
    {
        int id = Array.IndexOf(Names, name);
        return id >= 0 && written[id] > 0 && (ulong)Environment.TickCount64 - written[id] < 3000;
    }
    public ulong EventRevision(int id) => Enabled ? qdmail_event(handle, id, 0) : 0;
    public void NotifyEvents(int id) { if (Enabled) qdmail_event(handle, id, 1); }
    public static bool IsCosmetic(string value) {
        if(!(value.StartsWith("SND|")||value.StartsWith("VFX|")))return false;
        int at=0;for(int i=0;i<3;i++){at=value.IndexOf('|',at);if(at<0)return false;at++;}
        int end=value.IndexOf('|',at);if(end<0)end=value.TrimEnd('\n','\r').Length;
        return !value.AsSpan(at,end-at).SequenceEqual("R");
    }
    public bool PushEvent(int side,string value) {
        if(!Enabled||Encoding.UTF8.GetByteCount(value)>4096)return false;
        int n=Encoding.UTF8.GetBytes(value,eventBuffer);bool ok=qdmail_push(handle,side,eventBuffer,n)==1;if(ok)CosmeticWrites++;return ok;
    }
    public string[] DrainEvents(int side,int count) {
        if(!Enabled||count<=0)return [];int n=qdmail_drain(handle,side,eventBuffer,eventBuffer.Length,count);
        if(n==0)return [];var result=Encoding.UTF8.GetString(eventBuffer,0,n).Split('\n',StringSplitOptions.RemoveEmptyEntries);CosmeticReads+=result.Length;return result;
    }
    public void DiscardEvents(int side){if(Enabled)qdmail_discard_events(handle,side);}
    public void Dispose() { if (Enabled) qdmail_close(handle); }
    [DllImport("qdipc.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode, ExactSpelling = true)]
    static extern IntPtr qdmail_open(string name);
    [DllImport("qdipc.dll", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    static extern void qdmail_close(IntPtr handle);
    [DllImport("qdipc.dll", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    static extern int qdmail_write(IntPtr handle, int id, byte[] value, int length);
    [DllImport("qdipc.dll", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    static extern int qdmail_read(IntPtr handle, int id, [Out] byte[] value, int capacity, ref ulong revision, out int length, out ulong written);
    [DllImport("qdipc.dll", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    static extern ulong qdmail_event(IntPtr handle, int id, int increment);
    [DllImport("qdipc.dll", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    static extern IntPtr qdmail_ready(IntPtr handle,int side);
    [DllImport("qdipc.dll", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    static extern int qdmail_push(IntPtr handle,int side,byte[] value,int length);
    [DllImport("qdipc.dll", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    static extern int qdmail_drain(IntPtr handle,int side,[Out] byte[] value,int capacity,int count);
    [DllImport("qdipc.dll", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    static extern void qdmail_discard_events(IntPtr handle,int side);
    [DllImport("qdipc.dll", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    static extern uint qdmail_changed(IntPtr handle,ulong[] revisions,int count);
    [DllImport("qdipc.dll", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    static extern int qdmail_pending(IntPtr handle,int side);
}
