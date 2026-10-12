using OpenGta2.Client.Levels;
using OpenGta2.GameData.Map;

namespace OpenGta2.Client;

public static class MapExtensions
{
    public static ref BlockInfo GetBlock(this Map map, IntVector3 cell)
    {
        return ref map.GetBlock(cell.X, cell.Y, cell.Z);
    }
    
    public static bool IsWall(this Map map, IntVector3 cell, Face face)
    {
        var column = map.GetColumn(cell.X, cell.Y);

        if (cell.Z < column.Offset || cell.Z >= column.Height)
        {
            return false;
        }
        
        ref var block = ref map.CompressedMap.Blocks[column.Blocks[cell.Z - column.Offset]];
        ref var faceInfo = ref block.GetFace(face);
        return faceInfo.Wall;
    }
}