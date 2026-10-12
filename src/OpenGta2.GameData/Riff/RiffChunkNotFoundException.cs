using System.Runtime.Serialization;

namespace OpenGta2.GameData.Riff;

[Serializable]
public class RiffChunkNotFoundException : Exception
{
    public RiffChunkNotFoundException(string chunkName) : base($"The chunk '{chunkName}' could not be found in the specified file.")
    {
    }
}