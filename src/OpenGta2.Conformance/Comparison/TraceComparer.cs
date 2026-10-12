using OpenGta2.Conformance.Scenarios;
using OpenGta2.Conformance.Tracing;

namespace OpenGta2.Conformance.Comparison;

public sealed class TraceComparer(ToleranceSet tolerances, int maxReportedDivergences = 200)
{
    public ComparisonReport Compare(Trace expected, Trace actual, string expectedSource = "expected", string actualSource = "actual")
    {
        var divergences = new List<Divergence>();
        var stats = new Dictionary<(string Kind, string Property), StatsBuilder>();
        long comparedValues = 0, comparedTicks = 0;

        CompareHeaders(expected.Header, actual.Header, divergences);

        var actualByTick = actual.Snapshots.ToDictionary(s => s.Tick);
        var expectedTicks = new HashSet<long>();

        foreach (var exp in expected.Snapshots)
        {
            expectedTicks.Add(exp.Tick);
            if (!actualByTick.TryGetValue(exp.Tick, out var act))
            {
                divergences.Add(new Divergence { Kind = DivergenceKind.MissingTick, Tick = exp.Tick });
                continue;
            }

            comparedTicks++;
            comparedValues += CompareSnapshot(exp, act, divergences, stats);
        }

        foreach (var act in actual.Snapshots.Where(s => !expectedTicks.Contains(s.Tick)))
        {
            divergences.Add(new Divergence { Kind = DivergenceKind.UnexpectedTick, Tick = act.Tick });
        }

        var ordered = divergences
            .OrderBy(d => d.Tick)
            .ThenBy(d => d.EntityId, StringComparer.Ordinal)
            .ThenBy(d => d.Property, StringComparer.Ordinal)
            .ThenBy(d => d.Kind)
            .ToList();

        return new ComparisonReport
        {
            ExpectedSource = expectedSource,
            ActualSource = actualSource,
            ScenarioId = expected.Header.ScenarioId,
            ComparedTicks = comparedTicks,
            ComparedValues = comparedValues,
            TotalDivergences = ordered.Count,
            FirstDivergence = ordered.FirstOrDefault(),
            Divergences = ordered.Take(maxReportedDivergences).ToList(),
            Statistics = stats
                .OrderBy(kv => kv.Key.Kind, StringComparer.Ordinal)
                .ThenBy(kv => kv.Key.Property, StringComparer.Ordinal)
                .Select(kv => kv.Value.Build(kv.Key.Kind, kv.Key.Property))
                .ToList(),
        };
    }

    private static void CompareHeaders(TraceHeader e, TraceHeader a, List<Divergence> divergences)
    {
        void Check(string field, object expected, object actual)
        {
            if (!Equals(expected, actual))
            {
                divergences.Add(new Divergence
                {
                    Kind = DivergenceKind.HeaderMismatch,
                    Property = field,
                    Detail = $"expected '{expected}', actual '{actual}'",
                });
            }
        }

        Check("scenarioId", e.ScenarioId, a.ScenarioId);
        Check("tickRateHz", e.TickRateHz, a.TickRateHz);
        Check("ticks", e.Ticks, a.Ticks);
        Check("seed", e.Seed, a.Seed);
    }

    private long CompareSnapshot(TickSnapshot exp, TickSnapshot act, List<Divergence> divergences,
        Dictionary<(string, string), StatsBuilder> stats)
    {
        long compared = 0;
        var actualById = act.Entities.ToDictionary(e => e.Id, StringComparer.Ordinal);
        var expectedIds = new HashSet<string>(StringComparer.Ordinal);

        foreach (var e in exp.Entities)
        {
            expectedIds.Add(e.Id);
            if (!actualById.TryGetValue(e.Id, out var a))
            {
                divergences.Add(new Divergence { Kind = DivergenceKind.MissingEntity, Tick = exp.Tick, EntityId = e.Id, EntityKind = e.Kind });
                continue;
            }

            if (!string.Equals(e.Kind, a.Kind, StringComparison.Ordinal))
            {
                divergences.Add(new Divergence
                {
                    Kind = DivergenceKind.KindMismatch, Tick = exp.Tick, EntityId = e.Id, EntityKind = e.Kind,
                    Detail = $"expected kind '{e.Kind}', actual '{a.Kind}'",
                });
                continue;
            }

            foreach (var (property, expectedValue) in e.Properties)
            {
                if (!a.Properties.TryGetValue(property, out var actualValue))
                {
                    divergences.Add(new Divergence
                    {
                        Kind = DivergenceKind.MissingProperty, Tick = exp.Tick, EntityId = e.Id, EntityKind = e.Kind,
                        Property = property, Expected = expectedValue,
                    });
                    continue;
                }

                var tolerance = tolerances.For(e.Kind, property);
                var accepted = tolerance.Accepts(expectedValue, actualValue, out var distance);
                compared++;

                if (!stats.TryGetValue((e.Kind, property), out var builder))
                {
                    stats[(e.Kind, property)] = builder = new StatsBuilder(tolerance);
                }

                builder.Add(exp.Tick, distance, accepted);

                if (!accepted)
                {
                    divergences.Add(new Divergence
                    {
                        Kind = DivergenceKind.ValueOutOfTolerance, Tick = exp.Tick, EntityId = e.Id, EntityKind = e.Kind,
                        Property = property, Expected = expectedValue, Actual = actualValue, Distance = distance,
                        AbsoluteTolerance = tolerance.Absolute, RelativeTolerance = tolerance.Relative,
                    });
                }
            }

            foreach (var property in a.Properties.Keys.Where(p => !e.Properties.ContainsKey(p)))
            {
                divergences.Add(new Divergence
                {
                    Kind = DivergenceKind.UnexpectedProperty, Tick = exp.Tick, EntityId = e.Id, EntityKind = e.Kind,
                    Property = property, Actual = a.Properties[property],
                });
            }
        }

        foreach (var a in act.Entities.Where(a => !expectedIds.Contains(a.Id)))
        {
            divergences.Add(new Divergence { Kind = DivergenceKind.UnexpectedEntity, Tick = exp.Tick, EntityId = a.Id, EntityKind = a.Kind });
        }

        return compared;
    }

    private sealed class StatsBuilder(PropertyTolerance tolerance)
    {
        private long _compared, _outOfTolerance;
        private double _maxDistance;
        private long? _maxTick, _firstOutTick;

        public void Add(long tick, double distance, bool accepted)
        {
            _compared++;
            if (distance > _maxDistance || _maxTick is null && distance > 0)
            {
                _maxDistance = distance;
                _maxTick = tick;
            }

            if (!accepted)
            {
                _outOfTolerance++;
                _firstOutTick ??= tick;
            }
        }

        public PropertyStatistics Build(string kind, string property) => new()
        {
            Kind = kind,
            Property = property,
            Compared = _compared,
            OutOfTolerance = _outOfTolerance,
            MaxDistance = _maxDistance,
            MaxDistanceTick = _maxTick,
            FirstOutOfToleranceTick = _firstOutTick,
            AbsoluteTolerance = tolerance.Absolute,
            RelativeTolerance = tolerance.Relative,
        };
    }
}
