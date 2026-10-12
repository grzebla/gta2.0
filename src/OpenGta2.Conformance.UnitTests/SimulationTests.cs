using OpenGta2.Conformance.Adapters;
using OpenGta2.Conformance.Scenarios;
using OpenGta2.Conformance.Simulation;
using OpenGta2.Conformance.Tracing;
using Shouldly;
using Xunit;

namespace OpenGta2.Conformance.UnitTests;

public class SimulationTests
{
    [Fact]
    public void Timeline_should_combine_overlapping_spans_and_ignore_out_of_range_ticks()
    {
        var timeline = new InputTimeline(10,
        [
            TestData.Hold(2, 5, PlayerButton.Forward),
            TestData.Hold(4, 7, PlayerButton.Left, PlayerButton.Attack),
        ]);

        timeline[1].ShouldBe(PlayerButtons.None);
        timeline[2].ShouldBe(PlayerButtons.Forward);
        timeline[4].ShouldBe(PlayerButtons.Forward | PlayerButtons.Left | PlayerButtons.Attack);
        timeline[7].ShouldBe(PlayerButtons.Left | PlayerButtons.Attack);
        timeline[8].ShouldBe(PlayerButtons.None);
        timeline[-1].ShouldBe(PlayerButtons.None);
        timeline[10].ShouldBe(PlayerButtons.None);
    }

    [Fact]
    public void Clock_should_derive_time_from_ticks_without_drift()
    {
        var clock = new FixedStepClock(60);
        for (var i = 0; i < 216_000; i++) // one hour at 60 Hz
        {
            clock.Advance();
        }

        clock.ElapsedSeconds.ShouldBe(3600.0);
        clock.Elapsed.ShouldBe(TimeSpan.FromHours(1));
    }

    [Fact]
    public void Clock_deltas_should_sum_exactly_to_elapsed_time()
    {
        var clock = new FixedStepClock(30); // 1/30 s is not a whole number of 100 ns units
        var sum = TimeSpan.Zero;
        for (var i = 0; i < 90; i++)
        {
            sum += clock.NextDelta;
            clock.Advance();
        }

        sum.ShouldBe(clock.Elapsed);
        clock.Elapsed.ShouldBe(TimeSpan.FromSeconds(3));
    }

    [Fact]
    public void Random_should_match_the_SplitMix64_reference_sequence()
    {
        // Reference outputs of SplitMix64 for seed 0 (Steele, Lea, Flood 2014; also used by xoshiro seeding).
        var random = new DeterministicRandom(0);

        random.NextUInt64().ShouldBe(0xE220A8397B1DCDAFUL);
        random.NextUInt64().ShouldBe(0x6E789E6AA1B965F4UL);
        random.NextUInt64().ShouldBe(0x06C45D188009454FUL);
    }

    [Fact]
    public void Random_should_be_reproducible_per_seed_and_stay_in_range()
    {
        var a = new DeterministicRandom(42);
        var b = new DeterministicRandom(42);
        var c = new DeterministicRandom(43);

        var seqA = Enumerable.Range(0, 1000).Select(_ => a.NextInt(10)).ToArray();
        var seqB = Enumerable.Range(0, 1000).Select(_ => b.NextInt(10)).ToArray();
        var seqC = Enumerable.Range(0, 1000).Select(_ => c.NextInt(10)).ToArray();

        seqA.ShouldBe(seqB);
        seqA.ShouldNotBe(seqC);
        seqA.ShouldAllBe(v => v >= 0 && v < 10);
        Enumerable.Range(0, 1000).Select(_ => a.NextDouble()).ShouldAllBe(d => d >= 0 && d < 1);
    }

    [Fact]
    public void Runner_should_capture_initial_state_and_every_nth_tick_plus_the_last()
    {
        var scenario = TestData.Scenario(ticks: 10) with { Capture = new CaptureSettings { EveryTicks = 4 } };

        var trace = ScenarioRunner.Run(scenario, new SyntheticKinematicsAdapter());

        trace.Snapshots.Select(s => s.Tick).ShouldBe([0L, 4L, 8L, 10L]);
        trace.Header.Adapter.ShouldBe(SyntheticKinematicsAdapter.AdapterId);
    }

    [Fact]
    public void Runner_should_pass_tick_index_fixed_delta_and_replayed_buttons_to_the_adapter()
    {
        var scenario = TestData.Scenario(5, TestData.Hold(1, 2, PlayerButton.Jump));
        var probe = new RecordingAdapter();

        ScenarioRunner.Run(scenario, probe);

        probe.Steps.Select(s => s.Tick).ShouldBe([0L, 1L, 2L, 3L, 4L]);
        probe.Steps.ShouldAllBe(s => s.DeltaSeconds == 1.0 / 30);
        probe.Steps.Select(s => s.Buttons).ShouldBe([PlayerButtons.None, PlayerButtons.Jump, PlayerButtons.Jump, PlayerButtons.None, PlayerButtons.None]);
    }

    [Fact]
    public void Runner_should_filter_kinds_and_reject_duplicate_entity_ids()
    {
        var scenario = TestData.Scenario(1) with { Capture = new CaptureSettings { Kinds = ["synthetic-walker"] } };

        var trace = ScenarioRunner.Run(scenario, new SyntheticKinematicsAdapter());
        trace.Snapshots.ShouldAllBe(s => s.Entities.All(e => e.Kind == "synthetic-walker"));

        Should.Throw<InvalidOperationException>(() => ScenarioRunner.Run(TestData.Scenario(1), new RecordingAdapter { Duplicate = true }));
    }

    [Fact]
    public void Same_scenario_should_produce_byte_identical_traces()
    {
        var scenario = ScenarioSerializer.LoadValid(TestData.ScenarioFile("synthetic-drive-turn-brake.json"));

        var first = TraceSerializer.WriteToString(ScenarioRunner.Run(scenario, new SyntheticKinematicsAdapter()));
        var second = TraceSerializer.WriteToString(ScenarioRunner.Run(scenario, new SyntheticKinematicsAdapter()));

        second.ShouldBe(first);
    }

    [Fact]
    public void Synthetic_adapter_should_refuse_non_synthetic_scenarios()
    {
        var scenario = TestData.Scenario() with { Id = "wil-drive", Provenance = ScenarioProvenance.OriginalGta2 };

        Should.Throw<InvalidOperationException>(() => new SyntheticKinematicsAdapter()
            .Initialize(scenario, new SimulationContext(new FixedStepClock(30), new DeterministicRandom(0), null)));
    }

    private sealed class RecordingAdapter : ISimulationAdapter
    {
        public List<StepInput> Steps { get; } = [];
        public bool Duplicate { get; init; }
        public string Id => "recording";
        public string Version => "1";
        public void Initialize(ScenarioDefinition scenario, SimulationContext context) { }
        public void Step(in StepInput input) => Steps.Add(input);
        public IReadOnlyList<EntityState> Capture() => Duplicate
            ? [TestData.Body("a", 0), TestData.Body("a", 1)]
            : [TestData.Body("a", Steps.Count)];
        public void Dispose() { }
    }
}
