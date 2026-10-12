using Microsoft.Xna.Framework;

namespace OpenGta2.Client;

public readonly struct Line2D
{
    public Line2D(Vector2 from, Vector2 to)
    {
        From = from;
        To = to;
    }

    public Vector2 From { get; }
    public Vector2 To { get; }
    
    public static Vector2? Intersection(Line2D a, Line2D b)
    {
        var c = a.GetComponents();
        var d = b.GetComponents();

        var u = c.Y * d.Z - d.Y * c.Z;
        var v = d.X * c.Z - c.X * d.Z;
        var w = c.X * d.Y - d.X * c.Y;

        if(u == 0)
            return null;

        return new Vector2(u / w, v / w);
    }

    private Vector3 GetComponents()
    {
        var a = From.Y - To.Y;
        var b = To.X - From.X;
        var c = From.X * To.Y - From.Y * To.X;

        return new Vector3(a, b, c);
    }
}