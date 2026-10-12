using OpenGta2.Conformance.Scenarios;
using OpenGta2.Conformance.Tracing;

namespace OpenGta2.Conformance.Simulation;

/// <summary>
/// Bridge between the conformance runner and a simulation (OpenGTA2, a synthetic model,
/// or a recorder attached to the original game). Implementations must be deterministic:
/// same scenario + same inputs => bit-identical captures.
/// </summary>
public interface ISimulationAdapter : IDisposable
{
    /// <summary>Stable identifier written into trace headers, e.g. "synthetic-kinematics".</summary>
    string Id { get; }

    /// <summary>Version of the adapter's behaviour; bump it when captured semantics change.</summary>
    string Version { get; }

    void Initialize(ScenarioDefinition scenario, SimulationContext context);

    /// <summary>Advance exactly one fixed tick with the given input.</summary>
    void Step(in StepInput input);

    /// <summary>Current state of every observable entity.</summary>
    IReadOnlyList<EntityState> Capture();
}

public sealed record SimulationContext(FixedStepClock Clock, DeterministicRandom Random, string? GameDataPath);

public readonly record struct StepInput(long Tick, double DeltaSeconds, TimeSpan Delta, PlayerButtons Buttons);
