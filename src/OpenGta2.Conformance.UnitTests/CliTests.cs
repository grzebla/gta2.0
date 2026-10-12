using OpenGta2.Conformance.Cli;
using Shouldly;
using Xunit;

namespace OpenGta2.Conformance.UnitTests;

public class CliTests : IDisposable
{
    private readonly string _dir = Directory.CreateTempSubdirectory("opengta2-conformance-").FullName;

    public void Dispose() => Directory.Delete(_dir, recursive: true);

    private static (int Code, string Out, string Err) Cli(params string[] args)
    {
        var stdout = new StringWriter();
        var stderr = new StringWriter();
        var code = ConformanceCli.Run(args, stdout, stderr);
        return (code, stdout.ToString(), stderr.ToString());
    }

    [Fact]
    public void Run_then_compare_with_itself_should_pass()
    {
        var scenario = TestData.ScenarioFile("synthetic-drive-turn-brake.json");
        var a = Path.Combine(_dir, "a.jsonl");
        var b = Path.Combine(_dir, "b.jsonl");

        Cli("validate", scenario).Code.ShouldBe(ConformanceCli.Ok);
        Cli("run", scenario, "--adapter", "synthetic-kinematics", "--out", a).Code.ShouldBe(ConformanceCli.Ok);
        Cli("run", scenario, "--adapter", "synthetic-kinematics", "--out", b).Code.ShouldBe(ConformanceCli.Ok);

        var json = Path.Combine(_dir, "r.json");
        var result = Cli("compare", a, b, "--scenario", scenario, "--json", json);

        result.Code.ShouldBe(ConformanceCli.Ok, result.Err);
        result.Out.ShouldContain("PASS");
        File.ReadAllText(json).ShouldContain("\"passed\": true");
    }

    [Fact]
    public void Compare_should_exit_1_on_divergence()
    {
        var scenario = TestData.ScenarioFile("synthetic-drive-turn-brake.json");
        var a = Path.Combine(_dir, "a.jsonl");
        Cli("run", scenario, "--adapter", "synthetic-kinematics", "--out", a);
        var tampered = Path.Combine(_dir, "t.jsonl");
        var lines = File.ReadAllLines(a);
        lines[5] = lines[5].Replace("\"speed\":", "\"speed\":1");
        File.WriteAllLines(tampered, lines);

        var result = Cli("compare", a, tampered, "--scenario", scenario);

        result.Code.ShouldBe(ConformanceCli.Diverged);
        result.Out.ShouldContain("First divergence");
    }

    [Fact]
    public void Bad_input_should_exit_2_with_a_message()
    {
        Cli().Code.ShouldBe(ConformanceCli.UsageError);
        Cli("run", TestData.ScenarioFile("synthetic-drive-turn-brake.json"), "--adapter", "nope", "--out", "x").Err.ShouldContain("Unknown adapter");
        Cli("validate", Path.Combine(_dir, "missing.json")).Code.ShouldBe(ConformanceCli.UsageError);
    }
}
