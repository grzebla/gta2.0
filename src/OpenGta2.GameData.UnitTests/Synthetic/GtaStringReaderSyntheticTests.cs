using OpenGta2.GameData;
using OpenGta2.GameData.Riff;
using Shouldly;
using Xunit;

namespace OpenGta2.Data.UnitTests.Synthetic;

[Trait("Category", "Synthetic")]
public class GtaStringReaderSyntheticTests
{
    // SYNTHETIC GXT-like file: TKEY = (u32 offset into TDAT, 8-byte key) entries,
    // TDAT = 16-bit characters terminated by 0x0000. Keys and texts are invented.
    private static byte[] Utf16Text(string text)
    {
        var bytes = new List<byte>();
        foreach (var c in text)
        {
            bytes.Add((byte)c);
            bytes.Add(0);
        }
        bytes.Add(0);
        bytes.Add(0);
        return [.. bytes];
    }

    private static MemoryStream File(ushort version = 100)
    {
        var first = Utf16Text("Hello");
        var second = Utf16Text("Synthetic text 42");
        var tdat = first.Concat(second).ToArray();

        var tkey = new SyntheticFileBuilder()
            .UInt32(0).Ascii("greet", 8)
            .UInt32((uint)first.Length).Ascii("synth01", 8)
            .ToArray();

        return SyntheticFileBuilder.Container("GBL", version)
            .Chunk("TKEY", tkey)
            .Chunk("TDAT", tdat)
            .ToStream();
    }

    [Fact]
    public void Read_should_resolve_every_key_to_its_text()
    {
        using var riff = new RiffReader(File());

        var result = new GtaStringReader(riff).Read();

        result.Count.ShouldBe(2);
        result.ShouldContainKeyAndValue("greet", "Hello");
        result.ShouldContainKeyAndValue("synth01", "Synthetic text 42");
    }

    [Fact]
    public void Unsupported_version_should_be_rejected()
    {
        using var riff = new RiffReader(File(version: 99));

        Should.Throw<InvalidDataException>(() => new GtaStringReader(riff));
    }
}
