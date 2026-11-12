using System;
using System.Net;
using System.Net.Sockets;
using System.Threading;

public sealed class LanFaultRelay : IDisposable
{
    private readonly UdpClient socket;
    private readonly IPEndPoint host;
    private readonly Thread worker;
    private readonly bool cutWipe;
    private volatile bool stopping;

    public int DroppedAcks { get; private set; }
    public int ForwardedAcks { get; private set; }
    public int ForwardedRequests { get; private set; }
    public int CutWipeLaunches { get; private set; }
    public Exception Failure { get; private set; }

    public LanFaultRelay(string address, int hostPort, int relayPort, bool cutWipeLaunch)
    {
        var ip = IPAddress.Parse(address);
        host = new IPEndPoint(ip, hostPort);
        cutWipe = cutWipeLaunch;
        socket = new UdpClient(new IPEndPoint(ip, relayPort));
        worker = new Thread(Forward) { IsBackground = true };
        worker.Start();
    }

    private void Forward()
    {
        IPEndPoint guest = null;
        try
        {
            while (!stopping)
            {
                var from = new IPEndPoint(IPAddress.Any, 0);
                byte[] packet;
                try { packet = socket.Receive(ref from); }
                catch (SocketException error)
                {
                    // Winsock reports late UDP traffic to a closed game socket this way.
                    if (error.SocketErrorCode == SocketError.ConnectionReset) continue;
                    throw;
                }
                var fromHost = from.Equals(host);
                if (!fromHost)
                {
                    if (!from.Address.Equals(host.Address)) continue;
                    if (guest == null) guest = from;
                    if (!guest.Equals(from)) continue;
                }
                if (guest == null) continue;
                if (CutWipeLaunches != 0) continue;

                var gameplay = packet.Length >= 40 && BitConverter.ToUInt32(packet, 0) == 0x45344e51;
                if (cutWipe && fromHost && gameplay && packet.Length == 352 &&
                    BitConverter.ToUInt32(packet, 32) == 16 && BitConverter.ToUInt32(packet, 40) == 13)
                {
                    CutWipeLaunches++;
                    continue;
                }
                var trigger = !cutWipe && gameplay && packet.Length == 88 && BitConverter.ToUInt32(packet, 32) == 39;
                if (trigger)
                {
                    var ack = BitConverter.ToUInt32(packet, 68);
                    if (fromHost && ack == 1)
                    {
                        if (DroppedAcks == 0)
                        {
                            DroppedAcks++;
                            continue;
                        }
                        ForwardedAcks++;
                    }
                    if (!fromHost && ack == 0) ForwardedRequests++;
                }
                socket.Send(packet, packet.Length, fromHost ? guest : host);
            }
        }
        catch (Exception error)
        {
            if (!stopping) Failure = error;
        }
    }

    public void Dispose()
    {
        stopping = true;
        socket.Close();
        if (!worker.Join(2000)) throw new InvalidOperationException("UDP test relay failed to stop.");
    }
}
