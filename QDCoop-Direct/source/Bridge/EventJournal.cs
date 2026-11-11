using System.Text;

// Bounded incremental reads: old event history must not be reread at 500 Hz.
sealed class EventJournal(string path, Func<ulong>? notification = null)
{
    public long Offset { get; private set; }
    ulong consumedRevision;
    public bool HasPending => notification == null || notification() != consumedRevision;
    public IEnumerable<string> Read(int lines, int bytes = 65536)
    {
        if (lines <= 0) yield break;
        ulong revision = notification?.Invoke() ?? 0;
        if (notification != null && revision == consumedRevision) yield break;
        using var f = new FileStream(path, FileMode.Open, FileAccess.Read,
            FileShare.ReadWrite | FileShare.Delete);
        if (f.Length < Offset) Offset = 0;
        f.Position = Offset;
        var data = new byte[Math.Min(bytes, (int)Math.Min(int.MaxValue, f.Length - Offset))];
        int length = f.Read(data, 0, data.Length), begin = 0, count = 0;
        for (int i = 0; i < length && count < lines; ++i)
        {
            if (data[i] != (byte)'\n') continue;
            var line = Encoding.UTF8.GetString(data, begin, i - begin).TrimEnd('\r');
            Offset += i - begin + 1; begin = i + 1; count++;
            yield return line;
        }
        if (Offset >= f.Length) consumedRevision = revision;
    }
    public void SkipExisting() {
        ulong revision = notification?.Invoke() ?? 0;
        if (notification != null && revision == consumedRevision) return;
        Offset = new FileInfo(path).Length;
        consumedRevision = revision;
    }
}
