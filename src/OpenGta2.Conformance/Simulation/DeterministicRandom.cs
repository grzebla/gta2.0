namespace OpenGta2.Conformance.Simulation;

/// <summary>
/// SplitMix64. Platform-independent and fully specified, unlike <see cref="Random"/> whose
/// algorithm may change between .NET versions. Adapters must draw all randomness from here.
/// </summary>
public sealed class DeterministicRandom(ulong seed)
{
    private ulong _state = seed;

    public ulong NextUInt64()
    {
        var z = _state += 0x9E3779B97F4A7C15UL;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9UL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBUL;
        return z ^ (z >> 31);
    }

    /// <summary>Uniform integer in [0, maxExclusive) without modulo bias.</summary>
    public int NextInt(int maxExclusive)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(maxExclusive, 1);
        var bound = (ulong)maxExclusive;
        var threshold = (0UL - bound) % bound;
        while (true)
        {
            var r = NextUInt64();
            if (r >= threshold)
            {
                return (int)(r % bound);
            }
        }
    }

    /// <summary>Uniform double in [0, 1) using the top 53 bits.</summary>
    public double NextDouble() => (NextUInt64() >> 11) * (1.0 / (1UL << 53));
}
