using System;
using System.Diagnostics;
using Microsoft.Xna.Framework;
using OpenGta2.Client.Diagnostics;
using OpenGta2.Client.Levels;
using OpenGta2.GameData.Map;

namespace OpenGta2.Client;

public class CollisionMap
{
    private readonly Map _map;

    public CollisionMap(Map map)
    {
        _map = map;
    }
    
    public Vector3 CalculateMovement(Vector3 point, float radius, Vector2 delta)
    {
        var z = (int)point.Z;

        var result = CalculateMovement(new Vector2(point.X, point.Y), z, radius, delta);

        return new Vector3(result, point.Z);
    }
    
    private Vector2 CalculateMovement(Vector2 point, int z, float radius, Vector2 delta)
    {
        // var depth = 3;
        while (true)
        {
            if (delta == Vector2.Zero) return point;

            var pointInt = IntVector2.Floor(point);
            var target = point + delta;

            var min = Vector2.Min(point, target) - new Vector2(radius);
            var max = Vector2.Max(point, target) + new Vector2(radius);

            var minInt = IntVector2.Floor(min);
            var maxInt = IntVector2.Ceiling(max);

            minInt = new IntVector2(Math.Min(minInt.X, pointInt.X - 1), Math.Min(minInt.Y, pointInt.Y - 1));
            maxInt = new IntVector2(Math.Max(maxInt.X, pointInt.X + 1), Math.Max(maxInt.Y, pointInt.Y + 1));

            var collision = GetCollidingWall(minInt, maxInt, z, point, radius, delta);

            if (collision.HasValue)
            {
                return collision.Value.HitPosition;
                // var deltaMissing = target - collision.Value.HitPosition;
                //
                // var absNormal = new Vector2(MathF.Abs(collision.Value.HitNormal.X), MathF.Abs(collision.Value.HitNormal.Y));
                // var otherDirection = Vector2.One - absNormal;
                //
                // delta = deltaMissing * otherDirection;
                //
                // if (--depth == 0) return collision.Value.HitPosition;
                //
                // point = point + (collision.Value.HitPosition - point) * 0.9f;
                // continue;
            }

            return target;
        }
    }

    private record struct WallCollision(Vector2 HitPosition, Vector2 HitNormal, float HitDistanceSquared);

    private WallCollision? CheckWall(IntVector3 cell, Face face, Vector2 origin, float radius, Vector2 delta)
    {
        // check wall is solid consider wall on this cell and wall on neighboring cell
        if (!_map.IsWall(cell, face))
        {
            var faceNormal = GetFaceNormal(face);
            if (!_map.IsWall(cell + new IntVector3(faceNormal, 0), GetOpposite(face)))
            {
                return null;
            }
        }

        // check for wall intersection
        var deltaLineSegment = new LineSegment2D(origin, origin + delta);

        var wallLineSegment = GetWallSegmentForCircleCollision(cell, face, radius);

        var intersectionPoint = Line2D.Intersection(deltaLineSegment.AsLine(), wallLineSegment.AsLine());
        if (!intersectionPoint.HasValue)
        {
            return null;
        }

        var intersectionDelta = intersectionPoint.Value - origin;
        if (Vector2.Dot(delta, intersectionDelta) <= 0)
        {
            // intersection point is behind origin point
            DiagnosticHighlight.Add(new Vector3(intersectionPoint.Value, cell.Z), GtaVector.Skywards, Color.Red);

            return null;
        }
        
        var collisinWithinDelta = intersectionDelta.LengthSquared() <= delta.LengthSquared();

        var qq = wallLineSegment.DistanceToLineSquared(intersectionPoint.Value);
        var qqq =  qq < 1f;

        if (collisinWithinDelta && qqq)
        {
            // collision hit
            Debug.WriteLine($"hit! at {intersectionPoint.Value}");
            DiagnosticHighlight.Add(new Vector3(intersectionPoint.Value, cell.Z), GtaVector.Skywards, Color.Green);
            return new WallCollision(intersectionPoint.Value, GetFaceNormal(face), intersectionDelta.LengthSquared());
        }
        else if(collisinWithinDelta)

        {
            Debug.WriteLine($"{qq} -- {intersectionPoint.Value} on {wallLineSegment.From}-{wallLineSegment.To}");
        }
        
        // hit beyond movement delta
        DiagnosticHighlight.Add(new Vector3(intersectionPoint.Value, cell.Z), GtaVector.Skywards, Color.Yellow);
        return null;
    }

    private static LineSegment2D GetWallSegmentForCircleCollision(IntVector3 cell, Face face, float radius)
    {
        return face switch
        {
            Face.Top => new LineSegment2D(new Vector2(cell.X - radius, cell.Y - radius), new Vector2(cell.X + 1 + radius, cell.Y - radius)),
            Face.Bottom => new LineSegment2D(new Vector2(cell.X - radius, cell.Y + 1 + radius), new Vector2(cell.X + 1 + radius, cell.Y + 1 + radius)),
            Face.Left => new LineSegment2D(new Vector2(cell.X - radius, cell.Y - radius), new Vector2(cell.X - radius, cell.Y + 1 + radius)),
            Face.Right => new LineSegment2D(new Vector2(cell.X + 1 + radius, cell.Y - radius), new Vector2(cell.X + 1 + radius, cell.Y + 1 + radius)),
            _ => default
        };
    }

    private static void SetNearest(ref WallCollision? previous, WallCollision? next)
    {
        if (!previous.HasValue || (next.HasValue && next.Value.HitDistanceSquared < previous.Value.HitDistanceSquared))
        {
            previous = next;
        }
    }

    private WallCollision? CheckWall(IntVector3 cell, Vector2 point, float radius, Vector2 delta)
    {
        var originCol = IntVector2.Floor(point);
        WallCollision? result = null;

        DiagnosticHighlight.Add(cell, Vector3.One, Color.Purple);

        if (cell.X < originCol.X) result = CheckWall(cell, Face.Right, point, radius, delta);
        if (cell.X > originCol.X) SetNearest(ref result, CheckWall(cell, Face.Left, point, radius, delta));
        if (cell.Y < originCol.Y) SetNearest(ref result, CheckWall(cell, Face.Bottom, point, radius, delta));
        if (cell.Y > originCol.Y) SetNearest(ref result, CheckWall(cell, Face.Top, point, radius, delta));

        return result;
    }


    private WallCollision? GetCollidingWall(IntVector2 min, IntVector2 max, int z, Vector2 point, float radius, Vector2 delta)
    {
        WallCollision? collision = null;
        for (var x = min.X; x <= max.X; x++)
        {
            for (var y = min.Y; y <= max.Y; y++)
            {
                var cell = new IntVector3(x, y, z);
                var wall = CheckWall(cell, point, radius, delta);
                SetNearest(ref collision, wall);
            }
        }
        
        return collision;
    }
    
    private static IntVector2 GetFaceNormal(Face face)
    {
        return face switch
        {
            Face.Left => new IntVector2(-1, 0),
            Face.Right => new IntVector2(1, 0),
            Face.Top => new IntVector2(0, -1),
            _ => new IntVector2(0, 1),
        };
    }

    private static Face GetOpposite(Face face)
    {
        return face switch
        {
            Face.Left => Face.Right,
            Face.Right => Face.Left,
            Face.Top => Face.Bottom,
            Face.Bottom => Face.Top,
            _ => face
        };
    }
}