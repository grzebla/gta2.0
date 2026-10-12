using OpenGta2.GameData.Scripts;
using Shouldly;
using Xunit;

namespace OpenGta2.Data.UnitTests.Synthetic;

[Trait("Category", "Synthetic")]
public class ScriptParserSyntheticTests
{
    // Layout documented in ScriptParser.Parse: 6000 x u16 pointers, 65536 bytes of script,
    // u16 string-table length, then entries of u32 id, u32 type, u8 length, characters.
    // The parser advances by 9 + length per entry. All values below are SYNTHETIC.
    private static MemoryStream File(params (uint Id, uint Type, string Text)[] strings)
    {
        var builder = new SyntheticFileBuilder();
        for (var i = 0; i < 6000; i++)
        {
            builder.UInt16((ushort)(i % 7));
        }
        builder.Bytes(Enumerable.Range(0, 65536).Select(i => (byte)(i & 0xFF)).ToArray());

        var table = new SyntheticFileBuilder();
        foreach (var (id, type, text) in strings)
        {
            table.UInt32(id).UInt32(type).Bytes((byte)(text.Length + 1)).Ascii(text, text.Length + 1);
        }
        var tableBytes = table.ToArray();
        builder.UInt16((ushort)tableBytes.Length).Bytes(tableBytes);

        return builder.ToStream();
    }

    [Fact]
    public void Parse_should_read_pointers_and_script_data()
    {
        var script = new ScriptParser().Parse(File());

        script.Pointers.Length.ShouldBe(6000);
        script.Pointers[13].ShouldBe((ushort)(13 % 7));
        script.ScriptData.Length.ShouldBe(65536);
        script.ScriptData[0x1234].ShouldBe((byte)0x34);
        script.Strings.Values.Count.ShouldBe(0);
    }

    [Fact]
    public void Parse_should_read_every_string_table_entry()
    {
        var script = new ScriptParser().Parse(File((1, 3, "first"), (2, 3, "second")));

        script.Strings.Values.Count.ShouldBe(2);
        script.Strings.Values[1].Value.ShouldBe("first");
        script.Strings.Values[2].Value.ShouldBe("second");
    }
}
