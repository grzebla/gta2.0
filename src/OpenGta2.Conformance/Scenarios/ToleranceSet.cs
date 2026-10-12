namespace OpenGta2.Conformance.Scenarios;

/// <summary>
/// How far a captured property may deviate. Lookup order: kind-specific property,
/// property, default. The default is exact equality.
/// </summary>
public sealed record ToleranceSet
{
    public PropertyTolerance Default { get; init; } = PropertyTolerance.Exact;

    public IReadOnlyDictionary<string, PropertyTolerance> Properties { get; init; } =
        new Dictionary<string, PropertyTolerance>();

    public IReadOnlyDictionary<string, IReadOnlyDictionary<string, PropertyTolerance>> Kinds { get; init; } =
        new Dictionary<string, IReadOnlyDictionary<string, PropertyTolerance>>();

    public PropertyTolerance For(string kind, string property)
    {
        if (Kinds.TryGetValue(kind, out var byKind) && byKind.TryGetValue(property, out var specific))
        {
            return specific;
        }

        return Properties.TryGetValue(property, out var general) ? general : Default;
    }
}

/// <summary>
/// A value matches when |expected - actual| &lt;= Absolute OR &lt;= Relative * |expected|.
/// With WrapPeriod set (e.g. 360 for degrees, 1024 for GTA-style angle units) the
/// shortest distance around the circle is used.
/// </summary>
public sealed record PropertyTolerance
{
    public static PropertyTolerance Exact { get; } = new();

    public double Absolute { get; init; }

    public double Relative { get; init; }

    public double? WrapPeriod { get; init; }

    public double Distance(double expected, double actual)
    {
        if (double.IsNaN(expected) || double.IsNaN(actual))
        {
            return double.IsNaN(expected) && double.IsNaN(actual) ? 0 : double.PositiveInfinity;
        }

        if (expected.Equals(actual))
        {
            return 0; // also covers equal infinities
        }

        var delta = Math.Abs(expected - actual);
        if (WrapPeriod is { } period && period > 0 && double.IsFinite(delta))
        {
            delta %= period;
            delta = Math.Min(delta, period - delta);
        }

        return delta;
    }

    public bool Accepts(double expected, double actual, out double distance)
    {
        distance = Distance(expected, actual);
        return distance <= Absolute || (Relative > 0 && distance <= Relative * Math.Abs(expected));
    }
}
