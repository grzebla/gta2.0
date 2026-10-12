using OpenGta2.Conformance.Scenarios;
using OpenGta2.Conformance.Tracing;

namespace OpenGta2.Conformance.UnitTests;

internal static class TestData
{
    public static ScenarioDefinition Scenario(int ticks = 10, params InputSpan[] inputs) => new()
    {
        Id = "synthetic-unit",
        Provenance = ScenarioProvenance.Synthetic,
        Simulation = new SimulationSettings { TickRateHz = 30, Ticks = ticks, Seed = 7 },
        Inputs = inputs,
    };

    public static InputSpan Hold(int from, int to, params PlayerButton[] buttons) =>
        new() { FromTick = from, ToTick = to, Buttons = buttons };

    public static Trace Trace(params TickSnapshot[] snapshots) => new(Header(), snapshots);

    public static TraceHeader Header(string scenario = "synthetic-unit", int ticks = 2) => new()
    {
        ScenarioId = scenario,
        Provenance = ScenarioProvenance.Synthetic,
        Adapter = "test",
        AdapterVersion = "1",
        TickRateHz = 30,
        Ticks = ticks,
        Seed = 7,
    };

    public static TickSnapshot At(long tick, params EntityState[] entities) => new(tick, entities);

    public static EntityState Body(string id, double x, double y = 0, string kind = "body") =>
        EntityState.Create(id, kind, ("x", x), ("y", y));

    public static string ScenarioFile(string name) => Path.Combine(AppContext.BaseDirectory, "scenarios", name);
}
