using System.Text.Json;

namespace UnrealMCP.Sidecar;

internal static class JsonUtil
{
    public static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = null,
        WriteIndented = false
    };
}
