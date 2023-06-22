using OpenGta2.Client.Levels;
using OpenGta2.GameData.Map;

namespace OpenGta2.Client;

public static class BlockFaceExtensions
{
    public static ref FaceInfo GetFace(this ref BlockInfo block, Face face)
    {
        switch (face)
        {
            case Face.Left: return ref block.Left;
            case Face.Right: return ref block.Right;
            case Face.Top: return ref block.Top;
            case Face.Bottom: return ref block.Bottom;
            default: return ref block.Lid;
        }
    }
}