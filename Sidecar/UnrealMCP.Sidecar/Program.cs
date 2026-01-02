using System.Net.Sockets;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace UnrealMCP.Sidecar;

internal static class Program
{
    private static async Task Main(string[] args)
    {
        ApplyRuntimeConfig(args);
        Log.Info($"Starting UnrealMCP.Sidecar. UE={UeProxy.Host}:{UeProxy.Port}, TimeoutMs={UeProxy.TimeoutMs}");

        while (true)
        {
            string? msg;
            try
            {
                msg = await Stdio.ReadJsonAsync(CancellationToken.None);
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

        Log.Info("stdin closed; exiting");
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
                            ["version"] = "0.2.0"
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

    private static class Log
    {
        public static void Info(string message) => Console.Error.WriteLine($"[UnrealMCP.Sidecar][INFO] {message}");
        public static void Warn(string message) => Console.Error.WriteLine($"[UnrealMCP.Sidecar][WARN] {message}");
        public static void Error(string message) => Console.Error.WriteLine($"[UnrealMCP.Sidecar][ERROR] {message}");
    }

    private static class JsonUtil
    {
        public static readonly JsonSerializerOptions JsonOptions = new()
        {
            PropertyNamingPolicy = null,
            WriteIndented = false
        };
    }

    private static class Mcp
    {
        private const string UsageGuidePromptName = "unreal.usage_guide";

        private const string ResourceConnection = "unreal://connection";
        private const string ResourceCommands = "unreal://commands";
        private const string ResourceTools = "unreal://tools";
        private const string ResourceUsageGuide = "unreal://prompt/unreal.usage_guide";

        // Keep in sync with UE plugin command dispatch (UUnrealMCPBridge::ExecuteCommand).
        public static readonly string[] ProxiedCommands =
        {
            // Editor
            "get_actors_in_level",
            "find_actors_by_name",
            "spawn_actor",
            "create_actor",
            "delete_actor",
            "set_actor_transform",
            "get_actor_properties",
            "set_actor_property",
            "spawn_blueprint_actor",
            "focus_viewport",
            "take_screenshot",

            // Blueprint (asset/component)
            "create_blueprint",
            "add_component_to_blueprint",
            "set_component_property",
            "set_physics_properties",
            "compile_blueprint",
            "set_blueprint_property",
            "set_static_mesh_properties",
            "set_pawn_properties",

            // Blueprint nodes
            "connect_blueprint_nodes",
            "add_blueprint_get_self_component_reference",
            "add_blueprint_self_reference",
            "find_blueprint_nodes",
            "add_blueprint_event_node",
            "add_blueprint_input_action_node",
            "add_blueprint_function_node",
            "add_blueprint_get_component_node",
            "add_blueprint_variable",

            // Project
            "create_input_mapping",

            // UMG
            "create_umg_widget_blueprint",
            "add_text_block_to_widget",
            "add_button_to_widget",
            "bind_widget_event",
            "set_text_block_binding",
            "add_widget_to_viewport",

            // Interchange
            "import_model",
            "create_interchange_blueprint",
            "create_custom_interchange_blueprint",
            "get_interchange_assets",
            "reimport_asset",
            "get_interchange_info",

            // Misc
            "ping"
        };

        public static bool IsProxiedCommand(string toolName)
            => ProxiedCommands.Contains(toolName, StringComparer.Ordinal);

        public static JsonArray GetPrompts()
        {
            return new JsonArray
            {
                new JsonObject
                {
                    ["name"] = UsageGuidePromptName,
                    ["description"] = "Comprehensive guide for Unreal MCP tools and best practices"
                }
            };
        }

        public static JsonObject? GetPrompt(string name)
        {
            if (name == UsageGuidePromptName)
            {
                return new JsonObject
                {
                    ["description"] = "Comprehensive guide for Unreal MCP tools and best practices",
                    ["text"] = UnrealMcpUsageGuide.GetGuideText()
                };
            }
            return null;
        }

        public static JsonArray GetTools()
        {
            var tools = new JsonArray
            {
                new JsonObject
                {
                    ["name"] = "unreal.ping",
                    ["description"] = "Ping the Unreal Engine instance to check connectivity",
                    ["inputSchema"] = new JsonObject { ["type"] = "object", ["properties"] = new JsonObject() }
                },
                new JsonObject
                {
                    ["name"] = "unreal.send_command",
                    ["description"] = "Send a raw command to Unreal Engine",
                    ["inputSchema"] = new JsonObject
                    {
                        ["type"] = "object",
                        ["properties"] = new JsonObject
                        {
                            ["type"] = new JsonObject
                            {
                                ["type"] = "string",
                                ["description"] = "Command type"
                            },
                            ["params"] = new JsonObject
                            {
                                ["type"] = "object",
                                ["description"] = "Command parameters"
                            }
                        },
                        ["required"] = new JsonArray { "type" }
                    }
                },
                new JsonObject
                {
                    ["name"] = "unreal.batch",
                    ["description"] = "Execute multiple Unreal MCP tool calls in order (transaction-style).",
                    ["inputSchema"] = new JsonObject
                    {
                        ["type"] = "object",
                        ["properties"] = new JsonObject
                        {
                            ["calls"] = new JsonObject
                            {
                                ["type"] = "array",
                                ["description"] = "Array of tool calls: [{ name: string, arguments: object }]",
                                ["items"] = new JsonObject
                                {
                                    ["type"] = "object",
                                    ["properties"] = new JsonObject
                                    {
                                        ["name"] = new JsonObject { ["type"] = "string" },
                                        ["arguments"] = new JsonObject { ["type"] = "object" }
                                    },
                                    ["required"] = new JsonArray { "name" }
                                }
                            },
                            ["stop_on_error"] = new JsonObject { ["type"] = "boolean", ["default"] = true },
                            ["notify"] = new JsonObject { ["type"] = "boolean", ["description"] = "Send progress notifications/message during execution", ["default"] = false },
                            ["use_ue_batch"] = new JsonObject { ["type"] = "boolean", ["description"] = "Try to use UE plugin batch command when possible", ["default"] = true }
                        },
                        ["required"] = new JsonArray { "calls" }
                    }
                }
            };

            foreach (var tool in ToolSchemas.GetDetailedToolSchemas())
                tools.Add(tool);

            return tools;
        }

        public static JsonArray GetResources()
        {
            return new JsonArray
            {
                new JsonObject
                {
                    ["uri"] = ResourceConnection,
                    ["name"] = "Unreal connection info",
                    ["mimeType"] = "application/json",
                    ["description"] = "Current UE host/port/timeout configuration and last-known connectivity"
                },
                new JsonObject
                {
                    ["uri"] = ResourceCommands,
                    ["name"] = "Supported UE commands",
                    ["mimeType"] = "application/json",
                    ["description"] = "List of proxied UE plugin command types"
                },
                new JsonObject
                {
                    ["uri"] = ResourceTools,
                    ["name"] = "Tool list (schema)",
                    ["mimeType"] = "application/json",
                    ["description"] = "tools/list snapshot"
                },
                new JsonObject
                {
                    ["uri"] = ResourceUsageGuide,
                    ["name"] = "Usage guide prompt",
                    ["mimeType"] = "text/markdown",
                    ["description"] = "unreal.usage_guide content"
                }
            };
        }

        public static async Task<JsonArray?> ReadResourceAsync(string uri)
        {
            if (uri == ResourceConnection)
            {
                var status = await UeProxy.CheckConnectionAsync();
                return new JsonArray
                {
                    new JsonObject
                    {
                        ["uri"] = uri,
                        ["mimeType"] = "application/json",
                        ["text"] = new JsonObject
                        {
                            ["host"] = UeProxy.Host,
                            ["port"] = UeProxy.Port,
                            ["timeoutMs"] = UeProxy.TimeoutMs,
                            ["status"] = status
                        }.ToJsonString(JsonUtil.JsonOptions)
                    }
                };
            }

            if (uri == ResourceCommands)
            {
                return new JsonArray
                {
                    new JsonObject
                    {
                        ["uri"] = uri,
                        ["mimeType"] = "application/json",
                        ["text"] = new JsonObject
                        {
                            ["commands"] = new JsonArray(ProxiedCommands.Select(x => (JsonNode?)x).ToArray())
                        }.ToJsonString(JsonUtil.JsonOptions)
                    }
                };
            }

            if (uri == ResourceTools)
            {
                return new JsonArray
                {
                    new JsonObject
                    {
                        ["uri"] = uri,
                        ["mimeType"] = "application/json",
                        ["text"] = new JsonObject
                        {
                            ["tools"] = GetTools()
                        }.ToJsonString(JsonUtil.JsonOptions)
                    }
                };
            }

            if (uri == ResourceUsageGuide)
            {
                return new JsonArray
                {
                    new JsonObject
                    {
                        ["uri"] = uri,
                        ["mimeType"] = "text/markdown",
                        ["text"] = UnrealMcpUsageGuide.GetGuideText()
                    }
                };
            }

            return null;
        }

        public static async Task<JsonObject> ExecuteToolAsync(string toolName, JsonObject args)
        {
            if (toolName == "unreal.batch")
            {
                return await ExecuteBatchAsync(args);
            }

            if (toolName == "unreal.ping")
            {
                using var cts = new CancellationTokenSource(TimeSpan.FromMilliseconds(UeProxy.TimeoutMs));
                var ueResp = await UeProxy.SendAsync("ping", new JsonObject(), cts.Token);
                var text = ueResp is null ? "No response" : ueResp.ToJsonString(JsonUtil.JsonOptions);
                return ToolCallResultText(text, ueResp);
            }

            if (toolName == "unreal.send_command")
            {
                var type = args["type"]?.GetValue<string>();
                var p = args["params"] as JsonObject;

                if (string.IsNullOrWhiteSpace(type))
                    return ToolCallResultText("Missing arguments.type", null, isError: true);

                using var cts = new CancellationTokenSource(TimeSpan.FromMilliseconds(UeProxy.TimeoutMs));
                var ueResp = await UeProxy.SendAsync(type!, p, cts.Token);
                var text = ueResp is null ? "No response" : ueResp.ToJsonString(JsonUtil.JsonOptions);
                return ToolCallResultText(text, ueResp, isError: false);
            }

            if (IsProxiedCommand(toolName))
            {
                // Compatibility aliases (keep tool surface stable even if UE plugin has legacy naming).
                var commandType = toolName;
                if (toolName == "add_blueprint_get_component_node")
                    commandType = "add_blueprint_get_self_component_reference";
                if (toolName == "create_actor")
                    commandType = "spawn_actor";

                using var cts = new CancellationTokenSource(TimeSpan.FromMilliseconds(UeProxy.TimeoutMs));
                var ueResp = await UeProxy.SendAsync(commandType, args ?? new JsonObject(), cts.Token);
                var text = ueResp is null ? "No response" : ueResp.ToJsonString(JsonUtil.JsonOptions);
                return ToolCallResultText(text, ueResp, isError: false);
            }

            return ToolCallResultText($"Unknown tool: {toolName}", null, isError: true);
        }

        private static async Task<JsonObject> ExecuteBatchAsync(JsonObject args)
        {
            var calls = args["calls"] as JsonArray;
            if (calls is null)
                return ToolCallResultText("Missing arguments.calls (array)", null, isError: true);

            var stopOnError = args["stop_on_error"]?.GetValue<bool?>() ?? true;
            var notify = args["notify"]?.GetValue<bool?>() ?? false;
            var useUeBatch = args["use_ue_batch"]?.GetValue<bool?>() ?? true;

            // Attempt UE-side batch if enabled and every item maps cleanly to a UE command.
            if (useUeBatch)
            {
                var ueBatch = TryBuildUeBatchParams(calls);
                if (ueBatch is not null)
                {
                    if (notify) await NotifyAsync("info", $"Batch: sending {ueBatch.Commands.Count} command(s) to UE" );

                    try
                    {
                        using var cts = new CancellationTokenSource(TimeSpan.FromMilliseconds(UeProxy.TimeoutMs));
                        var ueResp = await UeProxy.SendAsync("batch", ueBatch.Params, cts.Token);
                        var ueText = ueResp is null ? "No response" : ueResp.ToJsonString(JsonUtil.JsonOptions);
                        return ToolCallResultText(ueText, ueResp, isError: false);
                    }
                    catch (Exception ex)
                    {
                        // Transport failure: fall back to local execution to avoid dropping the request.
                        if (notify) await NotifyAsync("warn", $"Batch: UE batch transport failed, falling back. {ex.Message}" );
                    }
                }
            }

            var results = new JsonArray();
            var okCount = 0;
            var errCount = 0;

            for (var i = 0; i < calls.Count; i++)
            {
                var call = calls[i] as JsonObject;
                var name = call?["name"]?.GetValue<string>();
                var arguments = call?["arguments"] as JsonObject ?? new JsonObject();

                if (string.IsNullOrWhiteSpace(name))
                {
                    errCount++;
                    results.Add(new JsonObject
                    {
                        ["index"] = i,
                        ["ok"] = false,
                        ["error"] = "Missing call.name"
                    });
                    if (stopOnError) break;
                    continue;
                }

                if (notify) await NotifyAsync("info", $"Batch: {i + 1}/{calls.Count} {name}");

                JsonObject toolResult;
                try
                {
                    toolResult = await ExecuteToolAsync(name!, arguments);
                }
                catch (Exception ex)
                {
                    toolResult = ToolCallResultText(ex.Message, new JsonObject { ["exception"] = ex.ToString() }, isError: true);
                }

                var isError = toolResult["isError"]?.GetValue<bool?>() ?? false;
                if (isError) errCount++; else okCount++;

                results.Add(new JsonObject
                {
                    ["index"] = i,
                    ["name"] = name,
                    ["ok"] = !isError,
                    ["result"] = toolResult
                });

                if (isError && stopOnError)
                    break;
            }

            var summary = new JsonObject
            {
                ["total"] = calls.Count,
                ["ok"] = okCount,
                ["error"] = errCount,
                ["stop_on_error"] = stopOnError
            };

            var structured = new JsonObject
            {
                ["summary"] = summary,
                ["items"] = results
            };

            var text = $"Batch finished. total={calls.Count}, ok={okCount}, error={errCount}";
            return ToolCallResultText(text, structured, isError: errCount > 0);
        }

        private sealed class UeBatchBuildResult
        {
            public required JsonObject Params { get; init; }
            public required List<(string Type, JsonObject Params)> Commands { get; init; }
        }

        private static UeBatchBuildResult? TryBuildUeBatchParams(JsonArray calls)
        {
            var commands = new List<(string Type, JsonObject Params)>();

            foreach (var node in calls)
            {
                var call = node as JsonObject;
                var name = call?["name"]?.GetValue<string>();
                var arguments = call?["arguments"] as JsonObject ?? new JsonObject();

                if (string.IsNullOrWhiteSpace(name))
                    return null;

                if (name == "unreal.ping")
                {
                    commands.Add(("ping", new JsonObject()));
                    continue;
                }

                if (name == "unreal.send_command")
                {
                    var type = arguments["type"]?.GetValue<string>();
                    var p = arguments["params"] as JsonObject ?? new JsonObject();
                    if (string.IsNullOrWhiteSpace(type)) return null;
                    commands.Add((type!, p));
                    continue;
                }

                if (IsProxiedCommand(name!))
                {
                    var commandType = name!;
                    if (name == "add_blueprint_get_component_node")
                        commandType = "add_blueprint_get_self_component_reference";
                    if (name == "create_actor")
                        commandType = "spawn_actor";
                    commands.Add((commandType, arguments));
                    continue;
                }

                // Batch only proxies UE-facing operations.
                return null;
            }

            var arr = new JsonArray();
            foreach (var (type, p) in commands)
            {
                arr.Add(new JsonObject
                {
                    ["type"] = type,
                    ["params"] = p
                });
            }

            return new UeBatchBuildResult
            {
                Commands = commands,
                Params = new JsonObject
                {
                    ["commands"] = arr,
                    ["stop_on_error"] = true
                }
            };
        }

        private static async Task NotifyAsync(string level, string message, JsonNode? data = null)
        {
            var notification = new JsonObject
            {
                ["jsonrpc"] = "2.0",
                ["method"] = "notifications/message",
                ["params"] = new JsonObject
                {
                    ["level"] = level,
                    ["message"] = message,
                    ["data"] = data
                }
            };
            await Stdio.WriteAnyAsync(notification, CancellationToken.None);
        }

        private static JsonNode? CloneNode(JsonNode? node)
            => node is null ? null : JsonNode.Parse(node.ToJsonString(JsonUtil.JsonOptions));

        public static JsonObject MakeJsonRpcResponse(JsonNode? id, JsonNode result)
        {
            return new JsonObject
            {
                ["jsonrpc"] = "2.0",
                ["id"] = CloneNode(id),
                ["result"] = result
            };
        }

        public static JsonObject MakeJsonRpcError(JsonNode? id, int code, string message, JsonNode? data = null)
        {
            var err = new JsonObject
            {
                ["code"] = code,
                ["message"] = message
            };
            if (data is not null)
                err["data"] = CloneNode(data);

            return new JsonObject
            {
                ["jsonrpc"] = "2.0",
                ["id"] = CloneNode(id),
                ["error"] = err
            };
        }

        public static JsonObject ToolCallResultText(string text, JsonNode? structured = null, bool isError = false)
        {
            var result = new JsonObject
            {
                ["content"] = new JsonArray(
                    new JsonObject
                    {
                        ["type"] = "text",
                        ["text"] = text
                    }
                )
            };

            // Not in the strict MCP spec; kept for debugging.
            if (structured is not null)
                result["structuredContent"] = structured;
            if (isError)
                result["isError"] = true;

            return result;
        }
    }

    private static class UeProxy
    {
        public static string Host = "127.0.0.1";
        public static int Port = 55557;
        public static int TimeoutMs = 10_000;

        public static async Task<JsonObject> CheckConnectionAsync()
        {
            try
            {
                using var client = new TcpClient();
                client.ReceiveTimeout = 3000;
                client.SendTimeout = 3000;

                using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(2));
                await client.ConnectAsync(Host, Port, cts.Token);

                return new JsonObject
                {
                    ["status"] = "connected",
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

        public static async Task<JsonNode?> SendAsync(string commandType, JsonObject? @params, CancellationToken ct)
        {
            // Request remains JSON for backward compatibility. We ask UE to reply using len32le framing.
            var payload = new JsonObject
            {
                ["type"] = commandType,
                ["params"] = @params ?? new JsonObject(),
                ["_mcp"] = new JsonObject
                {
                    ["response_framing"] = "len32le"
                }
            };

            var json = payload.ToJsonString(JsonUtil.JsonOptions);
            var bytes = Encoding.UTF8.GetBytes(json);

            using var client = new TcpClient
            {
                ReceiveTimeout = TimeoutMs,
                SendTimeout = TimeoutMs
            };

            await client.ConnectAsync(Host, Port, ct);

            using var stream = client.GetStream();
            await stream.WriteAsync(bytes, 0, bytes.Length, ct);
            await stream.FlushAsync(ct);

            // Try length-prefix first. If it doesn't look like a valid prefix, fall back to JSON accumulation.
            var header = new byte[4];
            var headerRead = await ReadSomeAsync(stream, header, 0, 4, ct);
            if (headerRead <= 0)
                return null;

            if (headerRead == 4 && header[0] != (byte)'{' && header[0] != (byte)'[')
            {
                var len = BitConverter.ToInt32(header, 0);
                if (len > 0 && len < 64 * 1024 * 1024)
                {
                    var body = new byte[len];
                    await ReadExactAsync(stream, body, 0, len, ct);
                    var bodyText = Encoding.UTF8.GetString(body);
                    return JsonNode.Parse(bodyText);
                }
            }

            // Fallback: treat bytes as start of JSON.
            var sb = new StringBuilder();
            sb.Append(Encoding.UTF8.GetString(header, 0, headerRead));

            var buffer = new byte[4096];
            while (true)
            {
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

        private static async Task<int> ReadSomeAsync(NetworkStream stream, byte[] buffer, int offset, int count, CancellationToken ct)
        {
            var read = 0;
            while (read < count)
            {
                var r = await stream.ReadAsync(buffer, offset + read, count - read, ct);
                if (r <= 0)
                    return read;
                read += r;
                if (!stream.DataAvailable)
                    break;
            }
            return read;
        }

        private static async Task ReadExactAsync(NetworkStream stream, byte[] buffer, int offset, int count, CancellationToken ct)
        {
            var read = 0;
            while (read < count)
            {
                var r = await stream.ReadAsync(buffer, offset + read, count - read, ct);
                if (r <= 0)
                    throw new EndOfStreamException("UE connection closed while reading response body");
                read += r;
            }
        }
    }

    private static class Stdio
    {
        private static readonly Stream StdIn = Console.OpenStandardInput();
        private static readonly Stream StdOut = Console.OpenStandardOutput();
        private static readonly SemaphoreSlim WriteLock = new(1, 1);

        public static async Task<string?> ReadJsonAsync(CancellationToken ct)
        {
            int first = await ReadFirstNonWhitespaceByteAsync(ct);
            if (first < 0)
                return null;

            // If it looks like JSON, fallback mode: read until newline.
            if (first == '{' || first == '[')
            {
                var ms = new MemoryStream();
                ms.WriteByte((byte)first);
                while (true)
                {
                    var b = new byte[1];
                    var r = await StdIn.ReadAsync(b, 0, 1, ct);
                    if (r <= 0)
                        break;
                    ms.WriteByte(b[0]);
                    if (b[0] == (byte)'\n')
                        break;
                }
                return Encoding.UTF8.GetString(ms.ToArray()).Trim();
            }

            // Otherwise treat as header-framed; accumulate until \r\n\r\n or \n\n.
            var header = new MemoryStream();
            header.WriteByte((byte)first);

            int contentLength = -1;
            while (true)
            {
                var b = new byte[1];
                var r = await StdIn.ReadAsync(b, 0, 1, ct);
                if (r <= 0)
                    return null;

                header.WriteByte(b[0]);

                var bytes = header.ToArray();
                var headerText = Encoding.ASCII.GetString(bytes);

                var splitIdx = headerText.IndexOf("\r\n\r\n", StringComparison.Ordinal);
                var delimLen = 4;
                if (splitIdx < 0)
                {
                    splitIdx = headerText.IndexOf("\n\n", StringComparison.Ordinal);
                    delimLen = 2;
                }

                if (splitIdx < 0)
                    continue;

                var headerPart = headerText.Substring(0, splitIdx);
                foreach (var line in headerPart.Split(new[] { "\r\n", "\n" }, StringSplitOptions.RemoveEmptyEntries))
                {
                    var colon = line.IndexOf(':');
                    if (colon <= 0)
                        continue;

                    var key = line.Substring(0, colon).Trim();
                    var val = line.Substring(colon + 1).Trim();
                    if (key.Equals("Content-Length", StringComparison.OrdinalIgnoreCase) && int.TryParse(val, out var n))
                        contentLength = n;
                }

                if (contentLength < 0)
                    throw new InvalidOperationException("Missing Content-Length header");

                var bodyPrefix = bytes[(splitIdx + delimLen)..];

                var body = new byte[contentLength];
                var offset = 0;

                var toCopy = Math.Min(bodyPrefix.Length, contentLength);
                Array.Copy(bodyPrefix, 0, body, 0, toCopy);
                offset += toCopy;

                while (offset < contentLength)
                {
                    var rr = await StdIn.ReadAsync(body, offset, contentLength - offset, ct);
                    if (rr <= 0)
                        throw new EndOfStreamException("stdin closed while reading body");
                    offset += rr;
                }

                return Encoding.UTF8.GetString(body);
            }
        }

        private static async Task<int> ReadFirstNonWhitespaceByteAsync(CancellationToken ct)
        {
            var buf = new byte[1];
            while (true)
            {
                var r = await StdIn.ReadAsync(buf, 0, 1, ct);
                if (r <= 0)
                    return -1;

                var b = buf[0];
                if (b is (byte)' ' or (byte)'\t' or (byte)'\r' or (byte)'\n')
                    continue;

                return b;
            }
        }

        public static async Task WriteJsonAsync(JsonObject response, CancellationToken ct)
        {
            await WriteAnyAsync(response, ct);
        }

        public static async Task WriteAnyAsync(JsonObject message, CancellationToken ct)
        {
            var json = message.ToJsonString(JsonUtil.JsonOptions);
            var payload = Encoding.UTF8.GetBytes(json);
            var header = Encoding.ASCII.GetBytes($"Content-Length: {payload.Length}\r\n\r\n");

            await WriteLock.WaitAsync(ct);
            try
            {
                await StdOut.WriteAsync(header, 0, header.Length, ct);
                await StdOut.WriteAsync(payload, 0, payload.Length, ct);
                await StdOut.FlushAsync(ct);
            }
            finally
            {
                WriteLock.Release();
            }
        }
    }
}
