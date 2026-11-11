using System.Net.Sockets;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

// Waits only on the standalone bridge thread. Winsock owns the level-triggered
// socket event; WSAEnumNetworkEvents clears it atomically with the event record.
sealed class NetworkWake : IDisposable {
    readonly Socket socket;
    readonly IntPtr network;
    readonly EventWaitHandle? incoming, outgoing;
    readonly WaitHandle[] handles=[];
    readonly bool enabled;
    public long Waits {get;private set;}
    public NetworkWake(Socket socket,IntPtr memory) {
        this.socket=socket;if(memory==IntPtr.Zero)return;
        network=WSACreateEvent();if(network==IntPtr.Zero)throw new SocketException();
        if(WSAEventSelect(socket.Handle,network,1)!=0){WSACloseEvent(network);throw new SocketException();}
        incoming=new(false,EventResetMode.AutoReset);incoming.SafeWaitHandle=new SafeWaitHandle(network,false);
        outgoing=new(false,EventResetMode.AutoReset);outgoing.SafeWaitHandle=new SafeWaitHandle(memory,false);
        handles=[outgoing,incoming];enabled=true;
    }
    [StructLayout(LayoutKind.Sequential)] struct Events {public int Mask;public int E0,E1,E2,E3,E4,E5,E6,E7,E8,E9;}
    public void Consume(){if(enabled&&WSAEnumNetworkEvents(socket.Handle,network,out _)!=0)throw new SocketException();}
    public void Wait(int timeout){Waits++;if(enabled){Consume();if(socket.Available==0)WaitHandle.WaitAny(handles,timeout);}else Thread.Sleep(2);}
    public void Dispose(){if(!enabled)return;WSAEventSelect(socket.Handle,IntPtr.Zero,0);incoming!.Dispose();outgoing!.Dispose();WSACloseEvent(network);}
    [DllImport("ws2_32.dll",SetLastError=true)]static extern IntPtr WSACreateEvent();
    [DllImport("ws2_32.dll",SetLastError=true)]static extern bool WSACloseEvent(IntPtr value);
    [DllImport("ws2_32.dll",SetLastError=true)]static extern int WSAEventSelect(IntPtr socket,IntPtr signal,int events);
    [DllImport("ws2_32.dll",SetLastError=true)]static extern int WSAEnumNetworkEvents(IntPtr socket,IntPtr signal,out Events events);
}
