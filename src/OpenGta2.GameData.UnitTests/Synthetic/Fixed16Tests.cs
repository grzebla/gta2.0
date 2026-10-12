using OpenGta2.GameData.Map;
using Shouldly;
using Xunit;

namespace OpenGta2.Data.UnitTests.Synthetic;

[Trait("Category", "Synthetic")]
public class Fixed16Tests
{
    [Theory]
    [InlineData((short)0, 0f)]
    [InlineData((short)128, 1f)]
    [InlineData((short)64, 0.5f)]
    [InlineData((short)-128, -1f)]
    [InlineData((short)-1, -1f / 128)]
    [InlineData(short.MaxValue, short.MaxValue / 128f)]
    public void Conversion_should_scale_by_one_128th(short raw, float expected)
    {
        float value = new Fixed16 { _data = raw };

        value.ShouldBe(expected);
    }

    [Fact]
    public void Conversion_should_be_odd_symmetric_for_every_raw_value()
    {
        for (var raw = short.MinValue + 1; raw <= short.MaxValue; raw++)
        {
            float positive = new Fixed16 { _data = (short)raw };
            float negative = new Fixed16 { _data = (short)-raw };
            negative.ShouldBe(-positive);
        }
    }
}
