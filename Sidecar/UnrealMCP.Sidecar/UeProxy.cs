using System.Net.Sockets;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace UnrealMCP.Sidecar;

internal static class UeProxy
{
    public static string Host = "127.0.0.1";
    public static int Port = 55557;
    public static int TimeoutMs = 10_000;

    public static async Task<JsonObject> CheckConnectionAsync()
    {
        // IMPORTANT:
        // UE server handles exactly ONE request per TCP connection and waits for JSON to arrive.
        // A "connect-only" health check will occupy the accept loop until timeout, causing later requests to be refused.
        // So we always perform a real ping request here.
        try
        {
            using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(2));
            var resp = await SendAsync("ping", new JsonObject(), cts.Token);

            return new JsonObject
            {
                ["status"] = resp is null ? "disconnected" : "connected",
                ["host"] = Host,
                ["port"] = Port
            };
        }
        catch (Exception ex)
        {
            return new JsonObject
            {
                ["status"] = "disconnected",
                ["host"] = Host,
                ["port"] = Port,
                ["error"] = ex.Message
            };
        }
    }


    public static async Task<JsonNode?> SendAsync(string commandType, JsonObject? @params, CancellationToken ct, JsonObject? mcpMeta = null)
    {
        // IMPORTANT: `JsonNode` cannot be re-parented.
        // Params/meta may come from parsed JSON-RPC input (already has a parent), so clone before embedding.
        var meta = mcpMeta is null
            ? new JsonObject()
            : JsonNode.Parse(mcpMeta.ToJsonString(JsonUtil.JsonOptions))!.AsObject();

        var safeParams = @params is null
            ? new JsonObject()
            : JsonNode.Parse(@params.ToJsonString(JsonUtil.JsonOptions))!.AsObject();

        // Request remains JSON for backward compatibility. We ask UE to reply using len32le framing.
        if (meta["response_framing"] is null)
            meta["response_framing"] = "len32le";

        var payload = new JsonObject
        {
            ["type"] = commandType,
            ["params"] = safeParams,
            ["_mcp"] = meta
        };

        // Support per-command timeout override via _mcp.timeout_ms
        // This allows long-running operations (e.g., compile_blueprint for complex blueprints) to have extended timeouts
        int effectiveTimeoutMs = TimeoutMs;
        if (meta["timeout_ms"] is JsonValue timeoutValue && timeoutValue.TryGetValue<int>(out var customTimeout) && customTimeout > 0)
        {
            effectiveTimeoutMs = customTimeout;
            Log.Info($"Using custom timeout: {effectiveTimeoutMs}ms for command: {commandType}");
        }

        var json = payload.ToJsonString(JsonUtil.JsonOptions);
        var bytes = Encoding.UTF8.GetBytes(json);

        using var client = new TcpClient
        {
            ReceiveTimeout = effectiveTimeoutMs,
            SendTimeout = effectiveTimeoutMs
        };

        await client.ConnectAsync(Host, Port, ct);

        using var stream = client.GetStream();
        await stream.WriteAsync(bytes, 0, bytes.Length, ct);
        await stream.FlushAsync(ct);

        // Tighten fallback: prefer len32le, only fall back to raw JSON if it clearly starts with '{' or '['.
        var header = new byte[4];
        try
        {
            await ReadExactAsync(stream, header, 0, 4, ct);
        }
        catch (EndOfStreamException)
        {
            return null;
        }

        var len = BitConverter.ToInt32(header, 0);
        if (len > 0 && len < 64 * 1024 * 1024)
        {
            var body = new byte[len];
            await ReadExactAsync(stream, body, 0, len, ct);
            var bodyText = Encoding.UTF8.GetString(body);
            return JsonNode.Parse(bodyText);
        }

        // Raw JSON fallback (legacy). Only attempt if the first byte looks like JSON.
        if (header[0] != (byte)'{' && header[0] != (byte)'[')
            throw new InvalidOperationException("Unexpected UE response framing (not len32le and not raw JSON)");

        var sb = new StringBuilder();
        sb.Append(Encoding.UTF8.GetString(header, 0, 4));

        const int MaxRawJsonBytes = 64 * 1024 * 1024;
        // Use effective timeout for raw JSON accumulation as well
        int rawJsonReadTimeoutMs = Math.Max(5000, effectiveTimeoutMs / 2);
        var buffer = new byte[4096];
        var rawReadStart = DateTime.UtcNow;

        while (true)
        {
            if (sb.Length > MaxRawJsonBytes)
                throw new InvalidOperationException("UE raw JSON response exceeded maximum size");

            // Timeout protection for raw JSON accumulation
            if ((DateTime.UtcNow - rawReadStart).TotalMilliseconds > rawJsonReadTimeoutMs)
                throw new TimeoutException($"UE raw JSON response read timed out after {rawJsonReadTimeoutMs}ms");

            try
            {
                var node = JsonNode.Parse(sb.ToString());
                if (node is not null)
                    return node;
            }
            catch (JsonException)
            {
                // Not complete yet.
            }

            var read = await stream.ReadAsync(buffer, 0, buffer.Length, ct);
            if (read <= 0)
                break;

            sb.Append(Encoding.UTF8.GetString(buffer, 0, read));
        }

        var finalText = sb.ToString();
        if (string.IsNullOrWhiteSpace(finalText))
            return null;

        return JsonNode.Parse(finalText);
    }

    private static async Task ReadExactAsync(NetworkStream stream, byte[] buffer, int offset, int count, CancellationToken ct)
    {
        var read = 0;
        while (read < count)
        {
            var r = await stream.ReadAsync(buffer, offset + read, count - read, ct);
            if (r <= 0)
                throw new EndOfStreamException("UE connection closed while reading response");
            read += r;
        }
    }
}
