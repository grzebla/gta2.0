using OpenGta2.Conformance.Scenarios;

namespace OpenGta2.Conformance.Simulation;

/// <summary>Pre-resolved, per-tick input. Lookups are O(1) and independent of wall-clock time.</summary>
public sealed class InputTimeline
{
    private readonly PlayerButtons[] _frames;

    public InputTimeline(int ticks, IEnumerable<InputSpan> spans)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(ticks);
        _frames = new PlayerButtons[ticks];

        foreach (var span in spans)
        {
            var flags = span.Buttons.ToFlags();
            var from = Math.Max(span.FromTick, 0);
            var to = Math.Min(span.ToTick, ticks - 1);
            for (var tick = from; tick <= to; tick++)
            {
                _frames[tick] |= flags;
            }
        }
    }

    public static InputTimeline From(ScenarioDefinition scenario) =>
        new(scenario.Simulation.Ticks, scenario.Inputs);

    public int Ticks => _frames.Length;

    /// <summary>Buttons held during <paramref name="tick"/>; nothing outside the scenario range.</summary>
    public PlayerButtons this[long tick] => tick >= 0 && tick < _frames.Length ? _frames[tick] : PlayerButtons.None;
}
