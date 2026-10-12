using System.Text;

namespace OpenGta2.Data.UnitTests.Synthetic;

/// <summary>
/// Builds SYNTHETIC binary files in the container layouts implemented by OpenGta2.GameData
/// (4-byte type, 16-bit version, then chunks of 4-byte name + 32-bit little-endian length + data).
/// The content is invented for tests and is NOT original GTA 2 data.
/// </summary>
internal sealed class SyntheticFileBuilder
{
    private readonly MemoryStream _stream = new();

    public static SyntheticFileBuilder Container(string type, ushort version)
    {
        var builder = new SyntheticFileBuilder();
        builder.Ascii(type, 4).UInt16(version);
        return builder;
    }

    public SyntheticFileBuilder Chunk(string name, byte[] data)
    {
        Ascii(name, 4).UInt32((uint)data.Length);
        _stream.Write(data);
        return this;
    }

    public SyntheticFileBuilder Ascii(string value, int length)
    {
        var bytes = new byte[length];
        Encoding.ASCII.GetBytes(value, 0, Math.Min(value.Length, length), bytes, 0);
        _stream.Write(bytes);
        return this;
    }

    public SyntheticFileBuilder Bytes(params byte[] data)
    {
        _stream.Write(data);
        return this;
    }

    public SyntheticFileBuilder UInt16(ushort value)
    {
        _stream.Write(BitConverter.GetBytes(value));
        return this;
    }

    public SyntheticFileBuilder UInt32(uint value)
    {
        _stream.Write(BitConverter.GetBytes(value));
        return this;
    }

    public byte[] ToArray() => _stream.ToArray();

    public MemoryStream ToStream() => new(ToArray(), writable: false);
}

/// <summary>Wraps a stream and hides seeking, to exercise forward-only code paths.</summary>
internal sealed class NonSeekableStream(Stream inner) : Stream
{
    public override bool CanRead => true;
    public override bool CanSeek => false;
    public override bool CanWrite => false;
    public override long Length => throw new NotSupportedException();
    public override long Position
    {
        get => inner.Position;
        set => throw new NotSupportedException();
    }

    public override int Read(byte[] buffer, int offset, int count) => inner.Read(buffer, offset, count);
    public override int Read(Span<byte> buffer) => inner.Read(buffer);
    public override long Seek(long offset, SeekOrigin origin) => throw new NotSupportedException();
    public override void Flush() => throw new NotSupportedException();
    public override void SetLength(long value) => throw new NotSupportedException();
    public override void Write(byte[] buffer, int offset, int count) => throw new NotSupportedException();
}
