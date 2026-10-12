using OpenGta2.Conformance.Adapters;
using OpenGta2.Conformance.Comparison;
using OpenGta2.Conformance.Scenarios;
using OpenGta2.Conformance.Simulation;
using OpenGta2.Conformance.Tracing;

namespace OpenGta2.Conformance.Cli;

/// <summary>
/// opengta2-conformance validate &lt;scenario.json&gt;
/// opengta2-conformance run &lt;scenario.json&gt; --adapter &lt;id&gt; --out &lt;trace.jsonl&gt; [--game-data &lt;dir&gt;]
/// opengta2-conformance compare &lt;expected.jsonl&gt; &lt;actual.jsonl&gt; --scenario &lt;scenario.json&gt; [--json f] [--markdown f]
/// Exit codes: 0 = ok / traces match, 1 = traces diverge, 2 = usage or input error.
/// </summary>
internal static class ConformanceCli
{
    public const int Ok = 0, Diverged = 1, UsageError = 2;

    public static int Run(string[] args, TextWriter stdout, TextWriter stderr)
    {
        try
        {
            if (args.Length == 0)
            {
                return Usage(stderr);
            }

            var options = Options.Parse(args[1..]);
            return args[0] switch
            {
                "validate" => Validate(options, stdout),
                "run" => RunScenario(options, stdout),
                "compare" => CompareTraces(options, stdout),
                _ => Usage(stderr),
            };
        }
        catch (Exception ex) when (ex is ArgumentException or InvalidScenarioException or InvalidDataException
                                       or IOException or System.Text.Json.JsonException or InvalidOperationException)
        {
            stderr.WriteLine($"error: {ex.Message}");
            return UsageError;
        }
    }

    private static int Validate(Options o, TextWriter stdout)
    {
        var scenario = ScenarioSerializer.LoadValid(o.Positional(0, "scenario"));
        stdout.WriteLine($"ok: {scenario.Id} ({scenario.Provenance}, {scenario.Simulation.Ticks} ticks @ {scenario.Simulation.TickRateHz} Hz)");
        return Ok;
    }

    private static int RunScenario(Options o, TextWriter stdout)
    {
        var scenario = ScenarioSerializer.LoadValid(o.Positional(0, "scenario"));
        using var adapter = AdapterRegistry.Create(o.Required("--adapter"));
        var trace = ScenarioRunner.Run(scenario, adapter, o.Optional("--game-data"));
        var output = o.Required("--out");
        TraceSerializer.WriteFile(trace, output);
        stdout.WriteLine($"wrote {trace.Snapshots.Count} snapshots to {output}");
        return Ok;
    }

    private static int CompareTraces(Options o, TextWriter stdout)
    {
        var expectedPath = o.Positional(0, "expected trace");
        var actualPath = o.Positional(1, "actual trace");
        var scenario = ScenarioSerializer.LoadValid(o.Required("--scenario"));

        var report = new TraceComparer(scenario.Tolerances)
            .Compare(TraceSerializer.ReadFile(expectedPath), TraceSerializer.ReadFile(actualPath), expectedPath, actualPath);

        var markdown = ReportWriter.ToMarkdown(report);
        if (o.Optional("--json") is { } json)
        {
            File.WriteAllText(json, ReportWriter.ToJson(report));
        }

        if (o.Optional("--markdown") is { } md)
        {
            File.WriteAllText(md, markdown);
        }

        stdout.Write(markdown);
        return report.Passed ? Ok : Diverged;
    }

    private static int Usage(TextWriter stderr)
    {
        stderr.WriteLine("usage:");
        stderr.WriteLine("  opengta2-conformance validate <scenario.json>");
        stderr.WriteLine($"  opengta2-conformance run <scenario.json> --adapter <{string.Join("|", AdapterRegistry.Ids)}> --out <trace.jsonl> [--game-data <dir>]");
        stderr.WriteLine("  opengta2-conformance compare <expected.jsonl> <actual.jsonl> --scenario <scenario.json> [--json <file>] [--markdown <file>]");
        return UsageError;
    }

    private sealed class Options
    {
        private readonly List<string> _positional = [];
        private readonly Dictionary<string, string> _named = new(StringComparer.Ordinal);

        public static Options Parse(string[] args)
        {
            var o = new Options();
            for (var i = 0; i < args.Length; i++)
            {
                if (args[i].StartsWith("--", StringComparison.Ordinal))
                {
                    if (i + 1 >= args.Length)
                    {
                        throw new ArgumentException($"{args[i]} needs a value.");
                    }

                    o._named[args[i]] = args[++i];
                }
                else
                {
                    o._positional.Add(args[i]);
                }
            }

            return o;
        }

        public string Positional(int index, string name) =>
            index < _positional.Count ? _positional[index] : throw new ArgumentException($"missing <{name}> argument.");

        public string Required(string name) =>
            _named.TryGetValue(name, out var value) ? value : throw new ArgumentException($"missing {name}.");

        public string? Optional(string name) => _named.GetValueOrDefault(name);
    }
}
