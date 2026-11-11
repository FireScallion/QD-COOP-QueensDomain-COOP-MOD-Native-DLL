// out.enemy is a rotating, partial batch, not the authoritative live roster.
// Keep unchanged entries across batches; resend a heartbeat for packet loss.
internal sealed class EnemySendCache
{
    private readonly Dictionary<string, (string Text, long Sent, long Seen)> entries = new();
    private long nextTrim;
    public int Count => entries.Count;
    public long Due(string key)=>entries.TryGetValue(key,out var e)?e.Sent+500:0;
    public bool ShouldSend(string key, string text, long now)
    {
        bool found = entries.TryGetValue(key, out var previous);
        bool send = !found || previous.Text != text || now - previous.Sent >= 500;
        if (!found && entries.Count >= 256)
            entries.Remove(entries.MinBy(x => x.Value.Seen).Key);
        entries[key] = (text, send ? now : previous.Sent, now);
        return send;
    }
    public void Trim(long now)
    {
        if (now < nextTrim) return;
        nextTrim = now + 500;
        foreach (var key in entries.Where(x => now - x.Value.Seen > 5000).Select(x => x.Key).ToArray())
            entries.Remove(key);
    }
    public void Clear() { entries.Clear(); nextTrim = 0; }
}
