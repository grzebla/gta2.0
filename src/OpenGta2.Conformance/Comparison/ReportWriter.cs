using System.Globalization;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace OpenGta2.Conformance.Comparison;

public static class ReportWriter
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        WriteIndented = true,
        NumberHandling = JsonNumberHandling.AllowNamedFloatingPointLiterals,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
        Converters = { new JsonStringEnumConverter(JsonNamingPolicy.CamelCase) },
    };

    public static string ToJson(ComparisonReport report) => JsonSerializer.Serialize(report, JsonOptions);

    public static string ToMarkdown(ComparisonReport report)
    {
        var inv = CultureInfo.InvariantCulture;
        var sb = new StringBuilder();
        sb.Append(inv, $"### Conformance: `{report.ScenarioId}` — {(report.Passed ? "PASS" : "FAIL")}\n\n");
        sb.Append(inv, $"- expected: `{report.ExpectedSource}`\n- actual: `{report.ActualSource}`\n");
        sb.Append(inv, $"- compared ticks: {report.ComparedTicks}, values: {report.ComparedValues}, divergences: {report.TotalDivergences}\n\n");

        if (report.FirstDivergence is { } first)
        {
            sb.Append("**First divergence**\n\n");
            sb.Append(inv, $"| tick | entity | property | kind | expected | actual | distance | tolerance |\n|---|---|---|---|---|---|---|---|\n");
            sb.Append(Row(first)).Append("\n\n");
        }

        if (report.Statistics.Count > 0)
        {
            sb.Append("**Per-property statistics**\n\n| kind | property | compared | out of tol. | max distance (tick) | first out-of-tol. tick | abs / rel tol. |\n|---|---|---|---|---|---|---|\n");
            foreach (var s in report.Statistics)
            {
                sb.Append(inv, $"| {s.Kind} | {s.Property} | {s.Compared} | {s.OutOfTolerance} | {F(s.MaxDistance)} ({s.MaxDistanceTick?.ToString(inv) ?? "-"}) | {s.FirstOutOfToleranceTick?.ToString(inv) ?? "-"} | {F(s.AbsoluteTolerance)} / {F(s.RelativeTolerance)} |\n");
            }
            sb.Append('\n');
        }

        if (report.Divergences.Count > 1)
        {
            sb.Append(inv, $"**Divergences** (showing {report.Divergences.Count} of {report.TotalDivergences})\n\n| tick | entity | property | kind | expected | actual | distance | tolerance |\n|---|---|---|---|---|---|---|---|\n");
            foreach (var d in report.Divergences)
            {
                sb.Append(Row(d)).Append('\n');
            }
        }

        return sb.ToString();
    }

    private static string Row(Divergence d) =>
        string.Create(CultureInfo.InvariantCulture,
            $"| {(d.Tick < 0 ? "header" : d.Tick.ToString(CultureInfo.InvariantCulture))} | {d.EntityId ?? "-"} | {d.Property ?? "-"} | {d.Kind} | {F(d.Expected)} | {F(d.Actual)} | {F(d.Distance)} | {(d.AbsoluteTolerance is null ? d.Detail ?? "-" : $"abs {F(d.AbsoluteTolerance)}, rel {F(d.RelativeTolerance)}")} |");

    private static string F(double? value) => value is null ? "-" : value.Value.ToString("R", CultureInfo.InvariantCulture);
}
