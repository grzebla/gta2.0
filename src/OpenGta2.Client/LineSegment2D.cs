using System;
using Microsoft.Xna.Framework;

namespace OpenGta2.Client;

public readonly struct LineSegment2D
{
    public LineSegment2D(Vector2 from, Vector2 to)
    {
        From = from;
        To = to;
    }

    public Vector2 From { get; }
    public Vector2 To { get; }
    
    public Line2D AsLine() => new(From, To);

    public float DistanceToLineSquared(Vector2 point)
    {
        var l2 = (From - To).LengthSquared();
        if (l2 == 0.0f) return (point - From).LengthSquared();

        var t = MathF.Max(0, MathF.Min(1, Vector2.Dot(point - From, To - From) / l2));
        var projection = From + t * (To - From);
        return (point - projection).LengthSquared();
    }

    public float DistanceToLine(Vector2 point) => MathF.Sqrt(DistanceToLineSquared(point));
}