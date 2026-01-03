using System.Reflection;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace UnrealMCP.Sidecar;

internal static class Program
{
    /// <summary>
    /// Reads version from AssemblyInformationalVersion, falling back to AssemblyVersion.
    /// </summary>
    internal static string GetVersion()
    {
        var asm = Assembly.GetExecutingAssembly();
        var infoVersion = asm.GetCustomAttribute<AssemblyInformationalVersionAttribute>()?.InformationalVersion;
        if (!string.IsNullOrWhiteSpace(infoVersion))
        {
            // Strip build metadata (e.g., "+abc123") if present
            var plusIdx = infoVersion.IndexOf('+');
            return plusIdx > 0 ? infoVersion[..plusIdx] : infoVersion;
        }
        var asmVersion = asm.GetName().Version;
        return asmVersion is not null ? $"{asmVersion.Major}.{asmVersion.Minor}.{asmVersion.Build}" : "0.0.0";
    }

    private static readonly CancellationTokenSource ShutdownCts = new();

    private static async Task Main(string[] args)
    {
        // Handle --version / --health before anything else
        if (args.Contains("--version") || args.Contains("-v"))
        {
            Console.WriteLine(GetVersion());
            return;
        }

        if (args.Contains("--health"))
        {
            var status = await UeProxy.CheckConnectionAsync();
            var ok = status["status"]?.GetValue<string>() == "connected";
            Console.WriteLine(ok ? "healthy" : "unhealthy");
            Environment.ExitCode = ok ? 0 : 1;
            return;
        }

        // Graceful shutdown: SIGINT (Ctrl+C) / SIGTERM
        Console.CancelKeyPress += (_, e) =>
        {
            e.Cancel = true; // Prevent immediate termination
            Log.Info("Received SIGINT, shutting down gracefully...");
            ShutdownCts.Cancel();
        };
        AppDomain.CurrentDomain.ProcessExit += (_, _) =>
        {
            if (!ShutdownCts.IsCancellationRequested)
            {
                Log.Info("Received SIGTERM, shutting down gracefully...");
                ShutdownCts.Cancel();
            }
        };

        ApplyRuntimeConfig(args);
        Log.Info($"Starting UnrealMCP.Sidecar v{GetVersion()}. UE={UeProxy.Host}:{UeProxy.Port}, TimeoutMs={UeProxy.TimeoutMs}");

        while (!ShutdownCts.IsCancellationRequested)
        {
            string? msg;
            try
            {
                msg = await Stdio.ReadJsonAsync(ShutdownCts.Token);
            }
            catch (OperationCanceledException)
            {
                break;
            }
            catch (Exception e)
            {
                Log.Error($"Failed to read stdio message: {e.Message}");
                break;
            }

            if (msg is null)
                break;

            var req = ParseRequest(msg);
            if (req is null)
                continue;

            await HandleAsync(req);
        }

        Log.Info("Shutdown complete; exiting");
    }

    private static McpRequest? ParseRequest(string text)
    {
        if (string.IsNullOrWhiteSpace(text))
            return null;

        JsonNode? root;
        try
        {
            root = JsonNode.Parse(text);
        }
        catch (Exception e)
        {
            Log.Warn($"Failed to parse JSON: {e.Message}");
            return null;
        }

        if (root is not JsonObject obj)
            return null;

        var method = obj["method"]?.GetValue<string>();
        if (string.IsNullOrWhiteSpace(method))
            return null;

        var idNode = obj["id"]; // keep a detached copy (JsonNode cannot be re-parented)
        var id = idNode is null ? null : JsonNode.Parse(idNode.ToJsonString(JsonUtil.JsonOptions));

        var jsonrpc = obj["jsonrpc"]?.GetValue<string>();

        var paramsNode = obj["params"] as JsonObject;
        var @params = paramsNode is null ? null : JsonNode.Parse(paramsNode.ToJsonString(JsonUtil.JsonOptions)) as JsonObject;

        return new McpRequest(jsonrpc, id, method!, @params);
    }

    private static async Task HandleAsync(McpRequest req)
    {
        var id = req.Id;
        JsonObject response;

        var requestId = MakeRequestId(req.Id);
        using var _ = Log.BeginScope(new Log.Context(requestId, TraceId: requestId));

        try
        {
            switch (req.Method)
            {

                case "initialize":
                {
                    var ueStatus = await UeProxy.CheckConnectionAsync();

                    var result = new JsonObject
                    {
                        ["protocolVersion"] = req.Params?["protocolVersion"]?.GetValue<string>() ?? "2024-11-05",
                        ["serverInfo"] = new JsonObject
                        {
                            ["name"] = "UnrealMCP.Sidecar",
                            ["version"] = GetVersion()
                        },
                        ["capabilities"] = new JsonObject
                        {
                            ["tools"] = new JsonObject(),
                            ["prompts"] = new JsonObject(),
                            ["resources"] = new JsonObject()
                        },
                        ["_meta"] = new JsonObject
                        {
                            ["unrealConnection"] = ueStatus,
                            ["supportsNotifications"] = true
                        }
                    };

                    response = Mcp.MakeJsonRpcResponse(id, result);
                    break;
                }

                case "prompts/list":
                {
                    var result = new JsonObject { ["prompts"] = Mcp.GetPrompts() };
                    response = Mcp.MakeJsonRpcResponse(id, result);
                    break;
                }

                case "prompts/get":
                {
                    var promptName = req.Params?["name"]?.GetValue<string>();
                    if (string.IsNullOrWhiteSpace(promptName))
                    {
                        response = Mcp.MakeJsonRpcError(id, -32602, "Missing params.name");
                        break;
                    }

                    var prompt = Mcp.GetPrompt(promptName);
                    if (prompt is null)
                    {
                        response = Mcp.MakeJsonRpcError(id, -32602, $"Unknown prompt: {promptName}");
                        break;
                    }

                    var result = new JsonObject
                    {
                        ["description"] = prompt["description"],
                        ["messages"] = new JsonArray
                        {
                            new JsonObject
                            {
                                ["role"] = "user",
                                ["content"] = new JsonObject
                                {
                                    ["type"] = "text",
                                    ["text"] = prompt["text"]?.GetValue<string>()
                                }
                            }
                        }
                    };

                    response = Mcp.MakeJsonRpcResponse(id, result);
                    break;
                }

                case "resources/list":
                {
                    var result = new JsonObject { ["resources"] = Mcp.GetResources() };
                    response = Mcp.MakeJsonRpcResponse(id, result);
                    break;
                }

                case "resources/read":
                {
                    var uri = req.Params?["uri"]?.GetValue<string>();
                    if (string.IsNullOrWhiteSpace(uri))
                    {
                        response = Mcp.MakeJsonRpcError(id, -32602, "Missing params.uri");
                        break;
                    }

                    var contents = await Mcp.ReadResourceAsync(uri);
                    if (contents is null)
                    {
                        response = Mcp.MakeJsonRpcError(id, -32602, $"Unknown resource: {uri}");
                        break;
                    }

                    response = Mcp.MakeJsonRpcResponse(id, new JsonObject { ["contents"] = contents });
                    break;
                }

                case "tools/list":
                {
                    var result = new JsonObject { ["tools"] = Mcp.GetTools() };
                    response = Mcp.MakeJsonRpcResponse(id, result);
                    break;
                }

                case "tools/call":
                {
                    var toolName = req.Params?["name"]?.GetValue<string>();
                    var args = req.Params?["arguments"] as JsonObject;

                    if (string.IsNullOrWhiteSpace(toolName))
                    {
                        response = Mcp.MakeJsonRpcError(id, -32602, "Missing params.name");
                        break;
                    }

                    var toolResult = await Mcp.ExecuteToolAsync(toolName, args ?? new JsonObject());
                    response = Mcp.MakeJsonRpcResponse(id, toolResult);
                    break;
                }

                // Many clients send notifications; ignore safely.
                case "notifications/initialized":
                {
                    return;
                }

                default:
                {
                    response = Mcp.MakeJsonRpcError(id, -32601, $"Method not implemented: {req.Method}");
                    break;
                }
            }
        }
        catch (Exception e)
        {
            response = Mcp.MakeJsonRpcError(id, -32000, "Server error", new JsonObject { ["exception"] = e.ToString() });
        }

        await Stdio.WriteJsonAsync(response, CancellationToken.None);
    }

    private static string MakeRequestId(JsonNode? id)
    {
        if (id is JsonValue v)
        {
            try
            {
                if (v.TryGetValue<string>(out var s) && !string.IsNullOrWhiteSpace(s))
                    return s;
            }
            catch { /* ignore */ }

            try
            {
                if (v.TryGetValue<long>(out var n))
                    return n.ToString();
            }
            catch { /* ignore */ }

            try
            {
                if (v.TryGetValue<double>(out var d))
                    return d.ToString("G", System.Globalization.CultureInfo.InvariantCulture);
            }
            catch { /* ignore */ }
        }

        return Guid.NewGuid().ToString("N");
    }

    private static void ApplyRuntimeConfig(string[] args)
    {

        // Env overrides (handy for IDE config)
        var hostEnv = Environment.GetEnvironmentVariable("UNREAL_MCP_HOST");
        if (!string.IsNullOrWhiteSpace(hostEnv))
            UeProxy.Host = hostEnv;

        var portEnv = Environment.GetEnvironmentVariable("UNREAL_MCP_PORT");
        if (int.TryParse(portEnv, out var port))
            UeProxy.Port = port;

        var timeoutEnv = Environment.GetEnvironmentVariable("UNREAL_MCP_TIMEOUT_MS");
        if (int.TryParse(timeoutEnv, out var timeoutMs))
            UeProxy.TimeoutMs = timeoutMs;

        // CLI overrides
        for (var i = 0; i < args.Length; i++)
        {
            var a = args[i];
            if (a == "--ue-host" && i + 1 < args.Length) { UeProxy.Host = args[++i]; continue; }
            if (a == "--ue-port" && i + 1 < args.Length && int.TryParse(args[i + 1], out var p)) { UeProxy.Port = p; i++; continue; }
            if (a == "--timeout-ms" && i + 1 < args.Length && int.TryParse(args[i + 1], out var t)) { UeProxy.TimeoutMs = t; i++; continue; }
        }
    }

    private record McpRequest(string? Jsonrpc, JsonNode? Id, string Method, JsonObject? Params);
}
