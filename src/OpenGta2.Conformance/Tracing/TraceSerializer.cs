using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace OpenGta2.Conformance.Tracing;

/// <summary>
/// JSON Lines: line 1 = {"header":{...}}, then one {"tick":N,"entities":[...]} per snapshot.
/// Output is byte-stable (sorted properties, ordinal entity order, round-trippable doubles,
/// no timestamps) so identical simulations produce identical files on every platform.
/// </summary>
public static class TraceSerializer
{
    private static readonly JsonSerializerOptions Options = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        DictionaryKeyPolicy = null,
        WriteIndented = false,
        NumberHandling = JsonNumberHandling.AllowNamedFloatingPointLiterals,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
        Converters = { new JsonStringEnumConverter(JsonNamingPolicy.CamelCase, allowIntegerValues: false) },
    };

    private sealed record HeaderLine(TraceHeader Header);

    private sealed record SnapshotLine(long Tick, IReadOnlyList<EntityLine> Entities);

    private sealed record EntityLine(string Id, string Kind, SortedDictionary<string, double> Properties);

    public static void Write(Trace trace, TextWriter writer)
    {
        writer.Write(JsonSerializer.Serialize(new HeaderLine(trace.Header), Options));
        writer.Write('\n');
        foreach (var snapshot in trace.Snapshots)
        {
            var entities = snapshot.Entities
                .OrderBy(e => e.Id, StringComparer.Ordinal)
                .Select(e => new EntityLine(e.Id, e.Kind, new SortedDictionary<string, double>(
                    e.Properties.ToDictionary(p => p.Key, p => p.Value), StringComparer.Ordinal)))
                .ToList();
            writer.Write(JsonSerializer.Serialize(new SnapshotLine(snapshot.Tick, entities), Options));
            writer.Write('\n');
        }
    }

    public static string WriteToString(Trace trace)
    {
        using var writer = new StringWriter { NewLine = "\n" };
        Write(trace, writer);
        return writer.ToString();
    }

    public static void WriteFile(Trace trace, string path)
    {
        using var writer = new StreamWriter(path, append: false, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
        Write(trace, writer);
    }

    public static Trace Read(TextReader reader)
    {
        var headerJson = reader.ReadLine() ?? throw new InvalidDataException("Trace is empty.");
        var header = JsonSerializer.Deserialize<HeaderLine>(headerJson, Options)?.Header
                     ?? throw new InvalidDataException("First trace line must be {\"header\":{...}}.");
        if (header.SchemaVersion != TraceHeader.CurrentSchemaVersion)
        {
            throw new InvalidDataException($"Unsupported trace schemaVersion {header.SchemaVersion}.");
        }

        var snapshots = new List<TickSnapshot>();
        var lineNumber = 1;
        while (reader.ReadLine() is { } line)
        {
            lineNumber++;
            if (line.Length == 0)
            {
                continue;
            }

            var snapshot = JsonSerializer.Deserialize<SnapshotLine>(line, Options)
                           ?? throw new InvalidDataException($"Line {lineNumber} is not a snapshot.");
            if (snapshots.Count > 0 && snapshot.Tick <= snapshots[^1].Tick)
            {
                throw new InvalidDataException($"Line {lineNumber}: ticks must be strictly increasing.");
            }

            snapshots.Add(new TickSnapshot(snapshot.Tick,
                snapshot.Entities.Select(e => new EntityState(e.Id, e.Kind, e.Properties)).ToList()));
        }

        return new Trace(header, snapshots);
    }

    public static Trace ReadFile(string path)
    {
        using var reader = new StreamReader(path, Encoding.UTF8);
        return Read(reader);
    }
}
