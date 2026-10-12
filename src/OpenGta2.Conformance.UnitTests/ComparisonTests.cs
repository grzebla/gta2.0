using OpenGta2.Conformance.Comparison;
using OpenGta2.Conformance.Scenarios;
using OpenGta2.Conformance.Tracing;
using Shouldly;
using Xunit;

namespace OpenGta2.Conformance.UnitTests;

public class ComparisonTests
{
    private static ComparisonReport Compare(Trace expected, Trace actual, ToleranceSet? tolerances = null) =>
        new TraceComparer(tolerances ?? new ToleranceSet()).Compare(expected, actual);

    private static ToleranceSet Abs(double x) => new() { Default = new PropertyTolerance { Absolute = x } };

    [Fact]
    public void Identical_traces_should_pass_with_full_statistics()
    {
        var t = TestData.Trace(TestData.At(0, TestData.Body("p", 1, 2)), TestData.At(1, TestData.Body("p", 1.5, 2)));

        var report = Compare(t, t);

        report.Passed.ShouldBeTrue();
        report.ComparedTicks.ShouldBe(2);
        report.ComparedValues.ShouldBe(4);
        report.FirstDivergence.ShouldBeNull();
        report.Statistics.Select(s => s.Property).ShouldBe(["x", "y"]);
    }

    [Fact]
    public void Values_within_absolute_tolerance_should_pass_and_report_max_distance()
    {
        var expected = TestData.Trace(TestData.At(0, TestData.Body("p", 1.0)), TestData.At(1, TestData.Body("p", 2.0)));
        var actual = TestData.Trace(TestData.At(0, TestData.Body("p", 1.05)), TestData.At(1, TestData.Body("p", 1.9)));

        var report = Compare(expected, actual, Abs(0.1 + 1e-12));

        report.Passed.ShouldBeTrue();
        var x = report.Statistics.Single(s => s.Property == "x");
        x.MaxDistance.ShouldBe(0.1, 1e-12);
        x.MaxDistanceTick.ShouldBe(1);
    }

    [Fact]
    public void First_divergence_should_be_the_earliest_tick_then_entity_then_property()
    {
        var expected = TestData.Trace(
            TestData.At(0, TestData.Body("a", 0), TestData.Body("b", 0)),
            TestData.At(1, TestData.Body("a", 1, 1), TestData.Body("b", 1, 1)),
            TestData.At(2, TestData.Body("a", 2), TestData.Body("b", 2)));
        var actual = TestData.Trace(
            TestData.At(0, TestData.Body("a", 0), TestData.Body("b", 0)),
            TestData.At(1, TestData.Body("a", 1, 1), TestData.Body("b", 1.5, 1.5)),
            TestData.At(2, TestData.Body("a", 9), TestData.Body("b", 2)));

        var report = Compare(expected, actual, Abs(0.25));

        report.Passed.ShouldBeFalse();
        report.TotalDivergences.ShouldBe(3);
        report.FirstDivergence!.Tick.ShouldBe(1);
        report.FirstDivergence.EntityId.ShouldBe("b");
        report.FirstDivergence.Property.ShouldBe("x");
        report.FirstDivergence.Expected.ShouldBe(1);
        report.FirstDivergence.Actual.ShouldBe(1.5);
        report.FirstDivergence.Distance.ShouldBe(0.5);
        report.Statistics.Single(s => s.Property == "x").FirstOutOfToleranceTick.ShouldBe(1);
    }

    [Theory]
    [InlineData(359, 1, 2)]
    [InlineData(10, 370, 0)]
    [InlineData(0, 180, 180)]
    public void Wrapped_properties_should_use_the_shortest_circular_distance(double expected, double actual, double distance)
    {
        new PropertyTolerance { WrapPeriod = 360 }.Distance(expected, actual).ShouldBe(distance, 1e-9);
    }

    [Fact]
    public void Relative_and_kind_specific_tolerances_should_apply()
    {
        var tolerances = new ToleranceSet
        {
            Properties = new Dictionary<string, PropertyTolerance> { ["x"] = new() { Relative = 0.01 } },
            Kinds = new Dictionary<string, IReadOnlyDictionary<string, PropertyTolerance>>
            {
                ["car"] = new Dictionary<string, PropertyTolerance> { ["x"] = new() { Absolute = 5 } },
            },
        };
        var expected = TestData.Trace(TestData.At(0, TestData.Body("ped", 1000), TestData.Body("car", 10, kind: "car")));
        var actual = TestData.Trace(TestData.At(0, TestData.Body("ped", 1009), TestData.Body("car", 14, kind: "car")));

        Compare(expected, actual, tolerances).Passed.ShouldBeTrue();
    }

    [Fact]
    public void NaN_should_only_match_NaN()
    {
        var tol = PropertyTolerance.Exact;
        tol.Accepts(double.NaN, double.NaN, out _).ShouldBeTrue();
        tol.Accepts(double.NaN, 0, out var d).ShouldBeFalse();
        d.ShouldBe(double.PositiveInfinity);
        new PropertyTolerance { Absolute = 1e9 }.Accepts(1, double.PositiveInfinity, out _).ShouldBeFalse();
    }

    [Fact]
    public void Structural_differences_should_be_reported()
    {
        var expected = new Trace(TestData.Header(ticks: 3), [
            TestData.At(0, TestData.Body("a", 0), TestData.Body("gone", 0)),
            TestData.At(1, EntityState.Create("a", "body", ("x", 1), ("y", 0))),
            TestData.At(2, TestData.Body("a", 2)),
        ]);
        var actual = new Trace(TestData.Header(ticks: 3) with { Seed = 8 }, [
            TestData.At(0, TestData.Body("a", 0), TestData.Body("new", 0)),
            TestData.At(1, EntityState.Create("a", "body", ("x", 1), ("speed", 3))),
            TestData.At(3, TestData.Body("a", 3)),
        ]);

        var kinds = Compare(expected, actual).Divergences.Select(d => d.Kind).ToList();

        kinds.ShouldContain(DivergenceKind.HeaderMismatch);
        kinds.ShouldContain(DivergenceKind.MissingEntity);
        kinds.ShouldContain(DivergenceKind.UnexpectedEntity);
        kinds.ShouldContain(DivergenceKind.MissingProperty);
        kinds.ShouldContain(DivergenceKind.UnexpectedProperty);
        kinds.ShouldContain(DivergenceKind.MissingTick);
        kinds.ShouldContain(DivergenceKind.UnexpectedTick);
        Compare(expected, actual).FirstDivergence!.Kind.ShouldBe(DivergenceKind.HeaderMismatch);
    }

    [Fact]
    public void Kind_mismatch_should_not_compare_properties()
    {
        var expected = TestData.Trace(TestData.At(0, TestData.Body("a", 0, kind: "ped")));
        var actual = TestData.Trace(TestData.At(0, TestData.Body("a", 99, kind: "car")));

        var report = Compare(expected, actual);

        report.Divergences.Single().Kind.ShouldBe(DivergenceKind.KindMismatch);
        report.ComparedValues.ShouldBe(0);
    }

    [Fact]
    public void Reported_divergences_should_be_capped_but_counted()
    {
        var snapshots = Enumerable.Range(0, 50).Select(t => TestData.At(t, TestData.Body("p", t))).ToArray();
        var shifted = Enumerable.Range(0, 50).Select(t => TestData.At(t, TestData.Body("p", t + 1))).ToArray();

        var report = new TraceComparer(new ToleranceSet(), maxReportedDivergences: 5)
            .Compare(TestData.Trace(snapshots), TestData.Trace(shifted));

        report.TotalDivergences.ShouldBe(50);
        report.Divergences.Count.ShouldBe(5);
        report.FirstDivergence!.Tick.ShouldBe(0);
    }

    [Fact]
    public void Markdown_and_json_reports_should_name_the_first_divergence()
    {
        var report = Compare(
            TestData.Trace(TestData.At(0, TestData.Body("p", 0)), TestData.At(7, TestData.Body("p", 1))),
            TestData.Trace(TestData.At(0, TestData.Body("p", 0)), TestData.At(7, TestData.Body("p", 4))));

        var md = ReportWriter.ToMarkdown(report);
        md.ShouldContain("FAIL");
        md.ShouldContain("| 7 | p | x | ValueOutOfTolerance | 1 | 4 | 3 |");
        ReportWriter.ToJson(report).ShouldContain("\"firstDivergence\"");
    }
}
