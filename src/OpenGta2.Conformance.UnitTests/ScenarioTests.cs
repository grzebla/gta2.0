using OpenGta2.Conformance.Scenarios;
using Shouldly;
using Xunit;

namespace OpenGta2.Conformance.UnitTests;

public class ScenarioTests
{
    [Fact]
    public void Committed_scenarios_should_parse_and_validate()
    {
        var files = Directory.GetFiles(Path.Combine(AppContext.BaseDirectory, "scenarios"), "*.json");

        files.ShouldNotBeEmpty();
        foreach (var file in files)
        {
            var scenario = ScenarioSerializer.LoadValid(file);
            scenario.Provenance.ShouldBe(ScenarioProvenance.Synthetic, $"{file}: only synthetic scenarios exist until GTA 2 reference data is recorded");
        }
    }

    [Fact]
    public void Json_round_trip_should_preserve_the_scenario()
    {
        var original = ScenarioSerializer.LoadValid(TestData.ScenarioFile("synthetic-drive-turn-brake.json"));

        var reparsed = ScenarioSerializer.Parse(ScenarioSerializer.Serialize(original));

        ScenarioSerializer.Serialize(reparsed).ShouldBe(ScenarioSerializer.Serialize(original));
        reparsed.Inputs.Count.ShouldBe(original.Inputs.Count);
        reparsed.Tolerances.For("synthetic-body", "headingDegrees").WrapPeriod.ShouldBe(360);
    }

    [Fact]
    public void Unknown_fields_and_unknown_buttons_should_be_rejected()
    {
        Should.Throw<System.Text.Json.JsonException>(() => ScenarioSerializer.Parse(
            """{"id":"synthetic-x","provenance":"synthetic","simulation":{"tickRateHz":30,"ticks":1},"typo":1}"""));
        Should.Throw<System.Text.Json.JsonException>(() => ScenarioSerializer.Parse(
            """{"id":"synthetic-x","provenance":"synthetic","simulation":{"tickRateHz":30,"ticks":5},"inputs":[{"fromTick":0,"toTick":1,"buttons":["fly"]}]}"""));
    }

    [Fact]
    public void Synthetic_scenarios_must_be_labelled_in_their_id()
    {
        var scenario = TestData.Scenario() with { Id = "wil-drive" };

        ScenarioValidator.Validate(scenario).ShouldContain(e => e.Contains("synthetic-"));
    }

    [Theory]
    [InlineData(0, 10, 0, 1, "tickRateHz")]
    [InlineData(30, 0, 0, 1, "ticks")]
    [InlineData(30, 10, 5, 10, "inputs[0]")]
    [InlineData(30, 10, 6, 5, "inputs[0]")]
    public void Invalid_settings_should_be_reported(int rate, int ticks, int from, int to, string expected)
    {
        var scenario = TestData.Scenario() with
        {
            Simulation = new SimulationSettings { TickRateHz = rate, Ticks = ticks },
            Inputs = [TestData.Hold(from, to, PlayerButton.Forward)],
        };

        ScenarioValidator.Validate(scenario).ShouldContain(e => e.Contains(expected));
    }

    [Fact]
    public void Negative_tolerances_should_be_reported()
    {
        var scenario = TestData.Scenario() with
        {
            Tolerances = new ToleranceSet { Properties = new Dictionary<string, PropertyTolerance> { ["x"] = new() { Absolute = -1 } } },
        };

        ScenarioValidator.Validate(scenario).ShouldContain(e => e.Contains("'x'"));
    }
}
