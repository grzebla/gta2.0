using OpenGta2.Conformance.Scenarios;

namespace OpenGta2.Conformance.Tracing;

/// <summary>State of all captured entities after <see cref="Tick"/> completed ticks (0 = initial state).</summary>
public sealed record TickSnapshot(long Tick, IReadOnlyList<EntityState> Entities);

/// <summary>
/// One observable object. Properties are numeric so they can be compared with tolerances;
/// encode enums/flags as numbers and document them in the adapter.
/// </summary>
public sealed record EntityState(string Id, string Kind, IReadOnlyDictionary<string, double> Properties)
{
    public static EntityState Create(string id, string kind, params (string Name, double Value)[] properties) =>
        new(id, kind, new SortedDictionary<string, double>(
            properties.ToDictionary(p => p.Name, p => p.Value), StringComparer.Ordinal));
}

public sealed record TraceHeader
{
    public const int CurrentSchemaVersion = 1;

    public int SchemaVersion { get; init; } = CurrentSchemaVersion;

    public required string ScenarioId { get; init; }

    public required ScenarioProvenance Provenance { get; init; }

    public required string Adapter { get; init; }

    public required string AdapterVersion { get; init; }

    public required int TickRateHz { get; init; }

    public required int Ticks { get; init; }

    public ulong Seed { get; init; }

    /// <summary>Free-form origin note (e.g. game build, host). Never compared.</summary>
    public string? Source { get; init; }
}

public sealed record Trace(TraceHeader Header, IReadOnlyList<TickSnapshot> Snapshots);
