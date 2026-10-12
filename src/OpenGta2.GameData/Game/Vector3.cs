namespace OpenGta2.GameData.Game;

public struct Vector3 : IFormattable
{
#pragma warning disable CA1051
    public float X;
    public float Y;
    public float Z;
#pragma warning restore CA1051

    public Vector3(float x, float y, float z)
    {
        X = x;
        Y = y;
        Z = z;
    }

    public static Vector3 FromInt(int x, int y, int z) => new(x / 16384.0f, y / 16384.0f, z / 16384.0f);
    
    public override string ToString() => $"({X}, {Y}, {Z})";
    public string ToString(string? format, IFormatProvider? formatProvider) => $"({X.ToString(format, formatProvider)}, {Y.ToString(format, formatProvider)}, {Z.ToString(format, formatProvider)})";
    public string ToString(string? format) => $"({X:format}, {Y:format}, {Z:format})";
}