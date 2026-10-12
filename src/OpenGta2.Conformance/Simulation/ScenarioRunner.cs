using OpenGta2.Conformance.Scenarios;
using OpenGta2.Conformance.Tracing;

namespace OpenGta2.Conformance.Simulation;

/// <summary>Runs a scenario against an adapter and produces a trace.</summary>
public static class ScenarioRunner
{
    public static Trace Run(ScenarioDefinition scenario, ISimulationAdapter adapter, string? gameDataPath = null)
    {
        var errors = ScenarioValidator.Validate(scenario);
        if (errors.Count > 0)
        {
            throw new InvalidScenarioException(scenario.Id, errors);
        }

        var clock = new FixedStepClock(scenario.Simulation.TickRateHz);
        var random = new DeterministicRandom(scenario.Simulation.Seed);
        var timeline = InputTimeline.From(scenario);
        var kinds = scenario.Capture.Kinds.Count == 0 ? null : scenario.Capture.Kinds.ToHashSet(StringComparer.Ordinal);

        adapter.Initialize(scenario, new SimulationContext(clock, random, gameDataPath));

        var snapshots = new List<TickSnapshot> { Snapshot(0, adapter, kinds) };
        for (var tick = 0; tick < scenario.Simulation.Ticks; tick++)
        {
            adapter.Step(new StepInput(tick, clock.DeltaSeconds, clock.NextDelta, timeline[tick]));
            clock.Advance();

            var completed = tick + 1;
            if (completed % scenario.Capture.EveryTicks == 0 || completed == scenario.Simulation.Ticks)
            {
                snapshots.Add(Snapshot(completed, adapter, kinds));
            }
        }

        var header = new TraceHeader
        {
            ScenarioId = scenario.Id,
            Provenance = scenario.Provenance,
            Adapter = adapter.Id,
            AdapterVersion = adapter.Version,
            TickRateHz = scenario.Simulation.TickRateHz,
            Ticks = scenario.Simulation.Ticks,
            Seed = scenario.Simulation.Seed,
        };

        return new Trace(header, snapshots);
    }

    private static TickSnapshot Snapshot(long tick, ISimulationAdapter adapter, HashSet<string>? kinds)
    {
        var entities = adapter.Capture()
            .Where(e => kinds is null || kinds.Contains(e.Kind))
            .OrderBy(e => e.Id, StringComparer.Ordinal)
            .ToList();

        var duplicate = entities.GroupBy(e => e.Id).FirstOrDefault(g => g.Count() > 1);
        if (duplicate is not null)
        {
            throw new InvalidOperationException($"Adapter '{adapter.Id}' reported entity id '{duplicate.Key}' more than once at tick {tick}.");
        }

        return new TickSnapshot(tick, entities);
    }
}
