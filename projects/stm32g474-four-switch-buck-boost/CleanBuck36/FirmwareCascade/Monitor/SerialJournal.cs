using System.Text;
using System.Text.Json;

namespace FixedSerialMonitor;

internal sealed record JournalEntry(long Sequence, DateTimeOffset At, string Kind, string Text);

internal sealed class SerialJournal : IDisposable
{
    private const int Capacity = 8192;
    private const int BeforeFault = 100;
    private const int AfterFault = 100;
    private readonly object _gate = new();
    private readonly Queue<JournalEntry> _recent = new(Capacity);
    private readonly List<JournalEntry> _faultWindow = new();
    private StreamWriter? _writer;
    private long _nextSequence = 1;
    private long? _faultSequence;
    private long? _emergencyAckSequence;
    private int _faultAfterRemaining;
    private string? _writeError;

    public string? FilePath { get; }

    public SerialJournal()
    {
        try
        {
            string directory = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "G474SerialMonitor", "Logs");
            Directory.CreateDirectory(directory);
            FilePath = Path.Combine(directory,
                $"serial-{DateTime.Now:yyyyMMdd-HHmmss}-{Guid.NewGuid():N}.jsonl");
            _writer = new StreamWriter(new FileStream(FilePath, FileMode.CreateNew,
                FileAccess.Write, FileShare.Read, 4096), new UTF8Encoding(false))
            {
                AutoFlush = true
            };
        }
        catch (Exception ex)
        {
            _writeError = ex.Message;
        }
    }

    public bool Durable
    {
        get { lock (_gate) return _writer is not null && _writeError is null; }
    }

    public long? EmergencyAckSequence
    {
        get { lock (_gate) return _emergencyAckSequence; }
    }

    public long? FaultSequence
    {
        get { lock (_gate) return _faultSequence; }
    }

    public long LatestSequence
    {
        get { lock (_gate) return _nextSequence - 1; }
    }

    public string? WriteError
    {
        get { lock (_gate) return _writeError; }
    }

    public JournalEntry Append(string line)
    {
        lock (_gate)
        {
            string kind = Classify(line);
            var entry = new JournalEntry(_nextSequence++, DateTimeOffset.UtcNow, kind, line);
            if (_writer is not null)
            {
                try { _writer.WriteLine(JsonSerializer.Serialize(entry)); }
                catch (Exception ex)
                {
                    _writeError = ex.Message;
                    _writer.Dispose();
                    _writer = null;
                }
            }

            if (line.TrimStart().StartsWith("CMD ESTOP LATCHED", StringComparison.Ordinal))
                _emergencyAckSequence = entry.Sequence;
            _recent.Enqueue(entry);
            if (_recent.Count > Capacity) _recent.Dequeue();

            if (_faultSequence is null && kind == "fault")
            {
                _faultSequence = entry.Sequence;
                _faultWindow.Clear();
                _faultWindow.AddRange(_recent.TakeLast(BeforeFault + 1));
                _faultAfterRemaining = AfterFault;
            }
            else if (_faultSequence is not null && _faultAfterRemaining > 0)
            {
                _faultWindow.Add(entry);
                _faultAfterRemaining--;
            }
            return entry;
        }
    }

    public string ReadAfter(long after, int limit)
    {
        lock (_gate)
        {
            limit = Math.Clamp(limit, 1, 1000);
            long oldest = _recent.Count > 0 ? _recent.Peek().Sequence : _nextSequence;
            return JsonSerializer.Serialize(new
            {
                ok = true,
                after,
                oldestSequence = oldest,
                latestSequence = _nextSequence - 1,
                truncated = after < oldest - 1,
                durable = _writer is not null && _writeError is null,
                filePath = FilePath,
                writeError = _writeError,
                entries = _recent.Where(x => x.Sequence > after).Take(limit).ToArray()
            });
        }
    }

    public string FaultWindow()
    {
        lock (_gate)
        {
            return JsonSerializer.Serialize(new
            {
                ok = true,
                faultLocked = _faultSequence.HasValue,
                faultSequence = _faultSequence,
                emergencyAckSequence = _emergencyAckSequence,
                postWindowComplete = _faultSequence.HasValue && _faultAfterRemaining == 0,
                durable = _writer is not null && _writeError is null,
                filePath = FilePath,
                writeError = _writeError,
                entries = _faultWindow.ToArray()
            });
        }
    }

    private static string Classify(string line)
    {
        string s = line.TrimStart();
        if (s.StartsWith("CMD ESTOP LATCHED", StringComparison.Ordinal) ||
            s.StartsWith("HOST RX OVERFLOW", StringComparison.Ordinal) ||
            s.StartsWith("FAULT_SNAPSHOT", StringComparison.Ordinal) ||
            s.StartsWith("BUCK FAULT LATCH", StringComparison.Ordinal) ||
            s.StartsWith("BUCK FAULT:", StringComparison.Ordinal) ||
            s.StartsWith("BUCK OC FAULT", StringComparison.Ordinal) ||
            s.StartsWith("BUCK OV FAULT", StringComparison.Ordinal) ||
            s.StartsWith("BUCK ADC FAULT", StringComparison.Ordinal) ||
            s.StartsWith("BUCK VOUT FB FAULT", StringComparison.Ordinal) ||
            s.StartsWith("BUCK TEMP/SENSOR FAULT", StringComparison.Ordinal) ||
            s.StartsWith("BUCK STARTUP TIMEOUT", StringComparison.Ordinal) ||
            s.StartsWith("DCM TRACE TRIGGER", StringComparison.Ordinal) ||
            s.StartsWith("CMD STOP LOCKED", StringComparison.Ordinal) ||
            (s.StartsWith("BUCK run=", StringComparison.Ordinal) &&
             s.Contains(" fault=1 ", StringComparison.Ordinal)))
            return "fault";
        if (s.StartsWith("CMD ", StringComparison.Ordinal)) return "command";
        if (s.StartsWith("REC ", StringComparison.Ordinal) ||
            s.StartsWith("TRC,", StringComparison.Ordinal) ||
            s.StartsWith("EVT,", StringComparison.Ordinal)) return "trace";
        if (s.StartsWith("VREF=", StringComparison.Ordinal)) return "telemetry";
        return "log";
    }

    public void Dispose()
    {
        lock (_gate)
        {
            _writer?.Dispose();
            _writer = null;
        }
    }
}
