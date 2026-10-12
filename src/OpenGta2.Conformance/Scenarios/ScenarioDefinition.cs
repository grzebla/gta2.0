namespace OpenGta2.Conformance.Scenarios;

/// <summary>
/// A conformance scenario: what to load, how long to simulate, which input to replay,
/// what to capture and how strictly two traces of it must agree.
/// </summary>
public sealed record ScenarioDefinition
{
    public const int CurrentSchemaVersion = 1;

    public int SchemaVersion { get; init; } = CurrentSchemaVersion;

    public required string Id { get; init; }

    public string? Description { get; init; }

    /// <summary>Where the expected data comes from. Synthetic data must never be presented as GTA 2 behaviour.</summary>
    public required ScenarioProvenance Provenance { get; init; }

    public required SimulationSettings Simulation { get; init; }

    public ScenarioWorld? World { get; init; }

    public IReadOnlyList<InputSpan> Inputs { get; init; } = [];

    public CaptureSettings Capture { get; init; } = new();

    public ToleranceSet Tolerances { get; init; } = new();
}

public enum ScenarioProvenance
{
    /// <summary>Invented data for testing the framework itself. Says nothing about GTA 2.</summary>
    Synthetic,

    /// <summary>Recorded from the original GTA 2 executable (e.g. on the EC2 graphics host).</summary>
    OriginalGta2,

    /// <summary>Recorded from OpenGTA2 (regression baseline, not a statement about the original).</summary>
    OpenGta2,
}

public sealed record SimulationSettings
{
    /// <summary>Fixed simulation rate. Every tick advances time by exactly 1 / TickRateHz seconds.</summary>
    public required int TickRateHz { get; init; }

    public required int Ticks { get; init; }

    /// <summary>Seed for <see cref="Simulation.DeterministicRandom"/>; adapters must not use any other randomness.</summary>
    public ulong Seed { get; init; }
}

public sealed record ScenarioWorld
{
    /// <summary>Map file relative to the game directory, e.g. "data/wil.gmp". Never committed to the repository.</summary>
    public string? Map { get; init; }

    public string? Style { get; init; }

    public string? Script { get; init; }
}

public sealed record CaptureSettings
{
    /// <summary>Capture a snapshot after every N ticks (the initial state at tick 0 is always captured).</summary>
    public int EveryTicks { get; init; } = 1;

    /// <summary>Restrict captured entities to these kinds; empty = everything the adapter reports.</summary>
    public IReadOnlyList<string> Kinds { get; init; } = [];
}
