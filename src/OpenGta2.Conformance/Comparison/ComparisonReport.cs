namespace OpenGta2.Conformance.Comparison;

public enum DivergenceKind
{
    HeaderMismatch,
    MissingTick,
    UnexpectedTick,
    MissingEntity,
    UnexpectedEntity,
    KindMismatch,
    MissingProperty,
    UnexpectedProperty,
    ValueOutOfTolerance,
}

public sealed record Divergence
{
    public required DivergenceKind Kind { get; init; }

    /// <summary>Tick of the snapshot; -1 for header-level differences.</summary>
    public long Tick { get; init; } = -1;

    public string? EntityId { get; init; }

    public string? EntityKind { get; init; }

    public string? Property { get; init; }

    public double? Expected { get; init; }

    public double? Actual { get; init; }

    public double? Distance { get; init; }

    public double? AbsoluteTolerance { get; init; }

    public double? RelativeTolerance { get; init; }

    public string? Detail { get; init; }
}

/// <summary>Per (kind, property) statistics over every compared value.</summary>
public sealed record PropertyStatistics
{
    public required string Kind { get; init; }

    public required string Property { get; init; }

    public long Compared { get; init; }

    public long OutOfTolerance { get; init; }

    public double MaxDistance { get; init; }

    public long? MaxDistanceTick { get; init; }

    public long? FirstOutOfToleranceTick { get; init; }

    public double AbsoluteTolerance { get; init; }

    public double RelativeTolerance { get; init; }
}

public sealed record ComparisonReport
{
    public required string ExpectedSource { get; init; }

    public required string ActualSource { get; init; }

    public required string ScenarioId { get; init; }

    public bool Passed => TotalDivergences == 0;

    public long ComparedTicks { get; init; }

    public long ComparedValues { get; init; }

    public long TotalDivergences { get; init; }

    /// <summary>Earliest divergence (header issues first, then lowest tick, entity id, property).</summary>
    public Divergence? FirstDivergence { get; init; }

    /// <summary>Divergences in deterministic order, truncated to the configured limit.</summary>
    public IReadOnlyList<Divergence> Divergences { get; init; } = [];

    public IReadOnlyList<PropertyStatistics> Statistics { get; init; } = [];
}
