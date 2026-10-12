using OpenGta2.GameData.Riff;
using Shouldly;
using Xunit;

namespace OpenGta2.Data.UnitTests.Synthetic;

[Trait("Category", "Synthetic")]
public class RiffReaderSyntheticTests
{
    private static MemoryStream ThreeChunkFile() =>
        SyntheticFileBuilder.Container("TEST", 7)
            .Chunk("AAAA", [1, 2, 3])
            .Chunk("BBBB", [10, 20, 30, 40, 50])
            .Chunk("CCCC", [])
            .ToStream();

    [Fact]
    public void Header_should_expose_type_and_version()
    {
        using var reader = new RiffReader(ThreeChunkFile());

        reader.Type.ShouldBe("TEST");
        reader.Version.ShouldBe((ushort)7);
    }

    [Fact]
    public void Next_should_enumerate_chunks_in_file_order_with_exact_lengths()
    {
        using var reader = new RiffReader(ThreeChunkFile());

        var names = new List<(string, long)>();
        while (reader.Next() is { } chunk)
        {
            names.Add((chunk.Name, chunk.Stream.Length));
        }

        names.ShouldBe([("AAAA", 3L), ("BBBB", 5L), ("CCCC", 0L)]);
    }

    [Fact]
    public void Chunk_stream_should_not_read_past_chunk_end()
    {
        using var reader = new RiffReader(ThreeChunkFile());
        var chunk = reader.GetRequiredChunk("AAAA");

        var buffer = new byte[16];
        var read = chunk.Stream.Read(buffer, 0, buffer.Length);

        read.ShouldBe(3);
        buffer[..3].ShouldBe(new byte[] { 1, 2, 3 });
        chunk.Stream.Read(buffer, 0, buffer.Length).ShouldBe(0);
    }

    [Fact]
    public void GetChunk_should_return_an_earlier_chunk_after_a_later_one_was_read()
    {
        using var reader = new RiffReader(ThreeChunkFile());

        reader.GetRequiredChunk("BBBB").Stream.ReadByte().ShouldBe(10);
        var first = reader.GetRequiredChunk("AAAA");

        first.Stream.ReadByte().ShouldBe(1);
    }

    [Fact]
    public void GetChunk_should_return_null_for_missing_chunk_and_GetRequiredChunk_should_throw()
    {
        using var reader = new RiffReader(ThreeChunkFile());

        reader.GetChunk("ZZZZ").ShouldBeNull();
        Should.Throw<RiffChunkNotFoundException>(() => reader.GetRequiredChunk("ZZZZ"));
    }

    [Fact]
    public void GetRequiredChunk_with_wrong_expected_length_should_reject_the_file()
    {
        using var reader = new RiffReader(ThreeChunkFile());

        Should.Throw<InvalidDataException>(() => reader.GetRequiredChunk("BBBB", 4));
    }

    [Fact]
    public void Partially_read_chunk_should_be_skipped_on_non_seekable_streams()
    {
        using var reader = new RiffReader(new NonSeekableStream(ThreeChunkFile()));

        reader.Next()!.Stream.ReadByte().ShouldBe(1); // leave 2 bytes unread
        var second = reader.Next()!;

        second.Name.ShouldBe("BBBB");
        second.Stream.ReadByte().ShouldBe(10);
    }

    [Theory]
    [InlineData(0, 5)]
    [InlineData(-1, 4)]
    [InlineData(-5, 0)]
    public void Seek_from_end_should_honour_the_offset(long offset, long expectedPosition)
    {
        using var reader = new RiffReader(ThreeChunkFile());
        var chunk = reader.GetRequiredChunk("BBBB");

        chunk.Stream.Seek(offset, SeekOrigin.End).ShouldBe(expectedPosition);
        chunk.Stream.Position.ShouldBe(expectedPosition);
        if (expectedPosition < 5)
        {
            chunk.Stream.ReadByte().ShouldBe(new byte[] { 10, 20, 30, 40, 50 }[expectedPosition]);
        }
    }

    [Fact]
    public void Truncated_header_should_throw()
    {
        var bytes = SyntheticFileBuilder.Container("TEST", 1).ToArray()[..5];

        Should.Throw<InvalidDataException>(() => new RiffReader(new MemoryStream(bytes)));
    }
}
