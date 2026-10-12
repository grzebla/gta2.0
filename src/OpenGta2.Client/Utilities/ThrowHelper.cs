using System;

namespace OpenGta2.Client.Utilities
{
    internal static class ThrowHelper
    {
        public static InvalidOperationException GetContentNotLoaded() => new("The content of this component has not yet been loaded.");
        public static InvalidOperationException GetLevelNotLoaded() => new("No level is currently loaded.");
    }
}
