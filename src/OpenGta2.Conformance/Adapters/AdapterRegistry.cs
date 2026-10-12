using OpenGta2.Conformance.Simulation;

namespace OpenGta2.Conformance.Adapters;

/// <summary>
/// Known adapters. OpenGTA2 adapters are added here once the game exposes a headless,
/// fixed-step simulation entry point (see docs/conformance/README.md).
/// </summary>
public static class AdapterRegistry
{
    private static readonly Dictionary<string, Func<ISimulationAdapter>> Factories = new(StringComparer.Ordinal)
    {
        [SyntheticKinematicsAdapter.AdapterId] = () => new SyntheticKinematicsAdapter(),
    };

    public static IReadOnlyCollection<string> Ids => Factories.Keys;

    public static ISimulationAdapter Create(string id) =>
        Factories.TryGetValue(id, out var factory)
            ? factory()
            : throw new ArgumentException($"Unknown adapter '{id}'. Known: {string.Join(", ", Factories.Keys)}.", nameof(id));
}
