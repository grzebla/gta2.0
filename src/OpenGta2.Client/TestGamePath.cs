using System;
using System.IO;

namespace OpenGta2.Client;

public static class TestGamePath
{
    // Process scope first: user-scope environment variables only exist on Windows,
    // so the original lookup always returned null on macOS and Linux.
    public static DirectoryInfo Directory =>
        new(Environment.GetEnvironmentVariable("OPENGTA2_PATH")
            ?? Environment.GetEnvironmentVariable("OPENGTA2_PATH", EnvironmentVariableTarget.User)
            ?? throw new InvalidOperationException("OPENGTA2_PATH is not set (path to an original GTA2 installation)."));

    public static Stream OpenFile(string path)
    {
        return File.OpenRead(Path.Combine(Directory.FullName, path));
    }
}