namespace OpenGta2.Conformance.Simulation;

/// <summary>
/// Simulation clock that only advances by whole ticks. Elapsed time is derived from the
/// tick count (never accumulated), so it does not drift and is identical on every machine.
/// </summary>
public sealed class FixedStepClock
{
    public FixedStepClock(int tickRateHz)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(tickRateHz, 1);
        TickRateHz = tickRateHz;
        DeltaSeconds = 1.0 / tickRateHz;
    }

    public int TickRateHz { get; }

    public double DeltaSeconds { get; }

    /// <summary>Number of completed ticks.</summary>
    public long Tick { get; private set; }

    public double ElapsedSeconds => (double)Tick / TickRateHz;

    /// <summary>Elapsed time as <see cref="TimeSpan"/>, rounded down to whole 100 ns units.</summary>
    public TimeSpan Elapsed => TimeSpan.FromTicks(Tick * TimeSpan.TicksPerSecond / TickRateHz);

    /// <summary>Duration of the tick that is about to run, matching <see cref="Elapsed"/> boundaries exactly.</summary>
    public TimeSpan NextDelta =>
        TimeSpan.FromTicks((Tick + 1) * TimeSpan.TicksPerSecond / TickRateHz - Tick * TimeSpan.TicksPerSecond / TickRateHz);

    public void Advance() => Tick++;
}
