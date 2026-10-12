using System.Text.RegularExpressions;

namespace OpenGta2.Conformance.Scenarios;

public static partial class ScenarioValidator
{
    public const int MaxTicks = 1_000_000;

    [GeneratedRegex("^[a-z0-9][a-z0-9-]{0,63}$")]
    private static partial Regex IdPattern();

    public static IReadOnlyList<string> Validate(ScenarioDefinition scenario)
    {
        var errors = new List<string>();

        if (scenario.SchemaVersion != ScenarioDefinition.CurrentSchemaVersion)
        {
            errors.Add($"schemaVersion must be {ScenarioDefinition.CurrentSchemaVersion}, got {scenario.SchemaVersion}.");
        }

        var id = scenario.Id ?? "";
        if (!IdPattern().IsMatch(id))
        {
            errors.Add("id must match ^[a-z0-9][a-z0-9-]{0,63}$.");
        }
        else if (scenario.Provenance == ScenarioProvenance.Synthetic && !id.StartsWith("synthetic-", StringComparison.Ordinal))
        {
            errors.Add("synthetic scenarios must have an id starting with 'synthetic-' so they cannot be mistaken for GTA 2 reference data.");
        }

        var sim = scenario.Simulation;
        if (sim is null)
        {
            errors.Add("simulation is required.");
            return errors;
        }

        if (sim.TickRateHz is < 1 or > 1000)
        {
            errors.Add("simulation.tickRateHz must be within 1..1000.");
        }

        if (sim.Ticks is < 1 or > MaxTicks)
        {
            errors.Add($"simulation.ticks must be within 1..{MaxTicks}.");
        }

        for (var i = 0; i < scenario.Inputs.Count; i++)
        {
            var span = scenario.Inputs[i];
            if (span.FromTick < 0 || span.ToTick < span.FromTick || span.ToTick >= sim.Ticks)
            {
                errors.Add($"inputs[{i}] must satisfy 0 <= fromTick <= toTick < simulation.ticks.");
            }

            if (span.Buttons.Count == 0)
            {
                errors.Add($"inputs[{i}].buttons must not be empty.");
            }
        }

        if (scenario.Capture.EveryTicks < 1)
        {
            errors.Add("capture.everyTicks must be >= 1.");
        }

        foreach (var (name, tolerance) in EnumerateTolerances(scenario.Tolerances))
        {
            if (tolerance.Absolute < 0 || tolerance.Relative < 0 || tolerance.WrapPeriod is <= 0)
            {
                errors.Add($"tolerance '{name}' must use non-negative absolute/relative and a positive wrapPeriod.");
            }
        }

        return errors;
    }

    private static IEnumerable<(string, PropertyTolerance)> EnumerateTolerances(ToleranceSet set)
    {
        yield return ("default", set.Default);
        foreach (var (property, t) in set.Properties)
        {
            yield return (property, t);
        }

        foreach (var (kind, properties) in set.Kinds)
        {
            foreach (var (property, t) in properties)
            {
                yield return ($"{kind}.{property}", t);
            }
        }
    }
}
