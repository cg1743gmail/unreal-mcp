using System.Diagnostics;
using System.Text.Json;

namespace UnrealMCP.Sidecar;

internal static class Log
{
    internal sealed record Context(string RequestId, string? TraceId);

    private sealed class Scope : IDisposable
    {
        private readonly Context? _prev;
        public Scope(Context? prev) => _prev = prev;
        public void Dispose() => Current.Value = _prev;
    }

    private static readonly AsyncLocal<Context?> Current = new();

    public static Context? GetCurrentContext() => Current.Value;

    public static IDisposable BeginScope(Context ctx)
    {
        var prev = Current.Value;
        Current.Value = ctx;
        return new Scope(prev);
    }

    /// <summary>
    /// Enable JSON structured logging by setting UNREAL_MCP_LOG_JSON=1
    /// </summary>
    private static bool UseJsonFormat =>
        string.Equals(Environment.GetEnvironmentVariable("UNREAL_MCP_LOG_JSON"), "1", StringComparison.Ordinal);

    public static void Info(string message) => Write("INFO", message);
    public static void Warn(string message) => Write("WARN", message);
    public static void Error(string message) => Write("ERROR", message);

    public static void Info(string message, object? data) => Write("INFO", message, data);
    public static void Warn(string message, object? data) => Write("WARN", message, data);
    public static void Error(string message, object? data) => Write("ERROR", message, data);

    private static void Write(string level, string message, object? data = null)
    {
        var ctx = Current.Value;

        if (UseJsonFormat)
        {
            var entry = new
            {
                ts = DateTime.UtcNow.ToString("O"),
                level,
                msg = message,
                request_id = ctx?.RequestId,
                trace_id = ctx?.TraceId,
                activity_trace_id = Activity.Current?.TraceId.ToString(),
                data
            };
            var json = JsonSerializer.Serialize(entry, JsonUtil.JsonOptions);
            Console.Error.WriteLine(json);
        }
        else
        {
            var prefix = ctx is null ? "" : $"[req={ctx.RequestId}] ";
            Console.Error.WriteLine($"[UnrealMCP.Sidecar][{level}] {prefix}{message}");
        }
    }
}

