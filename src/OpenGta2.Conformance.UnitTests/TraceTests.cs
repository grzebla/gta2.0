using OpenGta2.Conformance.Tracing;
using Shouldly;
using Xunit;

namespace OpenGta2.Conformance.UnitTests;

public class TraceTests
{
    [Fact]
    public void Round_trip_should_preserve_every_double_bit_for_bit()
    {
        double[] values = [0.1 + 0.2, Math.PI, -0.0, double.Epsilon, 1e308, double.NaN, double.PositiveInfinity];
        var entity = new EntityState("e", "k", values.Select((v, i) => ($"p{i}", v)).ToDictionary(p => p.Item1, p => p.v));
        var trace = TestData.Trace(TestData.At(0, entity));

        var text = TraceSerializer.WriteToString(trace);
        var read = TraceSerializer.Read(new StringReader(text));

        var props = read.Snapshots[0].Entities[0].Properties;
        for (var i = 0; i < values.Length; i++)
        {
            BitConverter.DoubleToInt64Bits(props[$"p{i}"]).ShouldBe(BitConverter.DoubleToInt64Bits(values[i]));
        }
        TraceSerializer.WriteToString(read).ShouldBe(text);
    }

    [Fact]
    public void Output_should_be_canonical_regardless_of_entity_and_property_order()
    {
        var a = TestData.Trace(TestData.At(0,
            new EntityState("b", "k", new Dictionary<string, double> { ["z"] = 1, ["a"] = 2 }),
            new EntityState("a", "k", new Dictionary<string, double> { ["m"] = 3 })));
        var b = TestData.Trace(TestData.At(0,
            new EntityState("a", "k", new Dictionary<string, double> { ["m"] = 3 }),
            new EntityState("b", "k", new Dictionary<string, double> { ["a"] = 2, ["z"] = 1 })));

        TraceSerializer.WriteToString(a).ShouldBe(TraceSerializer.WriteToString(b));
        TraceSerializer.WriteToString(a).Split('\n')[0].ShouldStartWith("{\"header\":");
    }

    [Fact]
    public void Reader_should_reject_non_increasing_ticks_and_unknown_schema()
    {
        var text = TraceSerializer.WriteToString(TestData.Trace(TestData.At(0), TestData.At(1)));
        var lines = text.Split('\n');

        Should.Throw<InvalidDataException>(() => TraceSerializer.Read(new StringReader($"{lines[0]}\n{lines[2]}\n{lines[1]}\n")));
        Should.Throw<InvalidDataException>(() => TraceSerializer.Read(new StringReader(lines[0].Replace("\"schemaVersion\":1", "\"schemaVersion\":9"))));
    }
}
