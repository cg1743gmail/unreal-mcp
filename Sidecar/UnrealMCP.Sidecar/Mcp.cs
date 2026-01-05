using System.Text.Json.Nodes;

namespace UnrealMCP.Sidecar;

internal static class Mcp
{
    private const string UsageGuidePromptName = "unreal.usage_guide";

    private const string ResourceConnection = "unreal://connection";
    private const string ResourceCommands = "unreal://commands";
    private const string ResourceTools = "unreal://tools";
    private const string ResourceUsageGuide = "unreal://prompt/unreal.usage_guide";

    // P2-3 (resources): optional UE-backed resources. Disabled by default to avoid surprising hosts.
    // Enable by setting: UNREAL_MCP_RESOURCES_ENABLE_UE=1
    private const string ResourceUePing = "unreal://ue/ping";
    private const string ResourceUeActorsInLevel = "unreal://ue/actors_in_level";

    private static bool EnableUeResources =>
        string.Equals(Environment.GetEnvironmentVariable("UNREAL_MCP_RESOURCES_ENABLE_UE"), "1", StringComparison.Ordinal);

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
        "list_blueprint_components",
        "get_component_property",
        "get_blueprint_property",


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
        "create_interchange_pipeline_blueprint",
        "get_interchange_pipelines",
        "configure_interchange_pipeline",
        "get_interchange_pipeline_graph",
        "add_interchange_pipeline_function_override",
        "add_interchange_pipeline_node",
        "connect_interchange_pipeline_nodes",
        "find_interchange_pipeline_nodes",
        "add_interchange_iterate_nodes_block",
        "compile_interchange_pipeline",

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
        var resources = new JsonArray
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

        if (EnableUeResources)
        {
            resources.Add(new JsonObject
            {
                ["uri"] = ResourceUePing,
                ["name"] = "UE ping",
                ["mimeType"] = "application/json",
                ["description"] = "Calls UE ping and returns the raw UE response"
            });

            resources.Add(new JsonObject
            {
                ["uri"] = ResourceUeActorsInLevel,
                ["name"] = "UE actors in level",
                ["mimeType"] = "application/json",
                ["description"] = "Calls UE get_actors_in_level and returns the raw UE response"
            });
        }

        return resources;
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

        if (EnableUeResources && uri == ResourceUePing)
        {
            using var cts = new CancellationTokenSource(TimeSpan.FromMilliseconds(UeProxy.TimeoutMs));
            var ueResp = await UeProxy.SendAsync("ping", new JsonObject(), cts.Token, BuildUeMcpMeta());
            var text = ueResp is null ? "null" : ueResp.ToJsonString(JsonUtil.JsonOptions);


            return new JsonArray
            {
                new JsonObject
                {
                    ["uri"] = uri,
                    ["mimeType"] = "application/json",
                    ["text"] = text
                }
            };
        }

        if (EnableUeResources && uri == ResourceUeActorsInLevel)
        {
            using var cts = new CancellationTokenSource(TimeSpan.FromMilliseconds(UeProxy.TimeoutMs));
            var ueResp = await UeProxy.SendAsync("get_actors_in_level", new JsonObject(), cts.Token, BuildUeMcpMeta());
            var text = ueResp is null ? "null" : ueResp.ToJsonString(JsonUtil.JsonOptions);


            return new JsonArray
            {
                new JsonObject
                {
                    ["uri"] = uri,
                    ["mimeType"] = "application/json",
                    ["text"] = text
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
            var ueResp = await UeProxy.SendAsync("ping", new JsonObject(), cts.Token, BuildUeMcpMeta());
            return ToolCallResultFromUeResponse("ping", ueResp);
        }


        if (toolName == "unreal.send_command")
        {
            var type = args["type"]?.GetValue<string>();
            var p = args["params"] as JsonObject;

            if (string.IsNullOrWhiteSpace(type))
                return ToolCallResultText("Missing arguments.type", null, isError: true);

            using var cts = new CancellationTokenSource(TimeSpan.FromMilliseconds(UeProxy.TimeoutMs));
            var ueResp = await UeProxy.SendAsync(type!, p, cts.Token, BuildUeMcpMeta());
            return ToolCallResultFromUeResponse(type!, ueResp);

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
            var ueResp = await UeProxy.SendAsync(commandType, args ?? new JsonObject(), cts.Token, BuildUeMcpMeta());
            return ToolCallResultFromUeResponse(commandType, ueResp);

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

        // If caller asks for progress notifications, execute locally so we can emit per-step updates.
        if (notify)
            useUeBatch = false;


        // Attempt UE-side batch if enabled and every item maps cleanly to a UE command.
        if (useUeBatch)
        {
            var ueBatch = TryBuildUeBatchParams(calls, stopOnError);
            if (ueBatch is not null)
            {
                if (notify) await NotifyAsync("info", $"Batch: sending {ueBatch.Commands.Count} command(s) to UE");

                try
                {
                    using var cts = new CancellationTokenSource(TimeSpan.FromMilliseconds(UeProxy.TimeoutMs));
                    var ueResp = await UeProxy.SendAsync("batch", ueBatch.Params, cts.Token, BuildUeMcpMeta());
                    return ToolCallResultFromUeResponse("batch", ueResp);

                }
                catch (Exception ex)
                {
                    // Transport failure: fall back to local execution to avoid dropping the request.
                    if (notify) await NotifyAsync("warn", $"Batch: UE batch transport failed, falling back. {ex.Message}");
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

            // Prevent accidental recursion.
            if (name == "unreal.batch")
            {
                errCount++;
                results.Add(new JsonObject
                {
                    ["index"] = i,
                    ["name"] = name,
                    ["ok"] = false,
                    ["error"] = "Nested unreal.batch is not allowed"
                });
                if (stopOnError) break;
                continue;
            }

            if (notify) await NotifyBatchProgressAsync("info", i + 1, calls.Count, name!, null);


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
            {
                if (notify)
                {
                    var next = BuildNextActionFromToolResult(name!, toolResult);
                    await NotifyBatchProgressAsync("error", i + 1, calls.Count, $"failed: {name}", next);
                }
                break;
            }

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
        if (notify)
        {
            var level = errCount > 0 ? "warn" : "info";
            await NotifyBatchProgressAsync(level, calls.Count, calls.Count, "complete", structured);
        }
        return ToolCallResultText(text, structured, isError: errCount > 0);

    }

    internal sealed class UeBatchBuildResult
    {
        public required JsonObject Params { get; init; }
        public required List<(string Type, JsonObject Params)> Commands { get; init; }
    }

    internal static UeBatchBuildResult? TryBuildUeBatchParams(JsonArray calls, bool stopOnError)
    {
        var commands = new List<(string Type, JsonObject Params)>();

        foreach (var node in calls)
        {
            var call = node as JsonObject;
            var name = call?["name"]?.GetValue<string>();
            var arguments = call?["arguments"] as JsonObject ?? new JsonObject();

            if (string.IsNullOrWhiteSpace(name))
                return null;

            // Block nesting; batch should only proxy UE-facing operations.
            if (name == "unreal.batch")
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
                ["stop_on_error"] = stopOnError
            }
        };
    }

    internal static JsonObject ToolCallResultFromUeResponse(string action, JsonNode? ueResp)
    {
        if (ueResp is null)
            return ToolCallResultText($"{action}: No response from UE", null, isError: true);

        var text = ueResp.ToJsonString(JsonUtil.JsonOptions);

        if (TryGetUeSuccess(ueResp, out var success, out var errorMessage))
        {
            if (success)
                return ToolCallResultText(text, ueResp, isError: false);

            var msg = string.IsNullOrWhiteSpace(errorMessage) ? $"{action}: UE returned an error" : $"{action}: {errorMessage}";
            return ToolCallResultText(msg, ueResp, isError: true);
        }

        // Unknown response shape: keep backward compatibility, do not force an error.
        return ToolCallResultText(text, ueResp, isError: false);
    }

    internal static bool TryGetUeSuccess(JsonNode? ueResp, out bool success, out string? errorMessage)
    {
        success = true;
        errorMessage = null;

        if (ueResp is not JsonObject obj)
            return false;

        // Newer UE plugin returns explicit bool.
        try
        {
            var s = obj["success"]?.GetValue<bool?>();
            if (s.HasValue)
            {
                success = s.Value;
                if (!success)
                    errorMessage = obj["error"]?.GetValue<string>();
                return true;
            }
        }
        catch
        {
            // ignore
        }

        // Fallback: status string.
        try
        {
            var status = obj["status"]?.GetValue<string>();
            if (!string.IsNullOrWhiteSpace(status))
            {
                success = status.Equals("success", StringComparison.OrdinalIgnoreCase);
                if (!success)
                    errorMessage = obj["error"]?.GetValue<string>();
                return true;
            }
        }
        catch
        {
            // ignore
        }

        // Legacy heuristic: error field implies failure.
        try
        {
            var err = obj["error"]?.GetValue<string>();
            if (!string.IsNullOrWhiteSpace(err))
            {
                success = false;
                errorMessage = err;
                return true;
            }
        }
        catch
        {
            // ignore
        }

        return false;
    }

    private static JsonObject BuildUeMcpMeta()
    {
        var meta = new JsonObject
        {
            ["response_framing"] = "len32le"
        };

        // Request correlation
        var ctx = Log.GetCurrentContext();
        if (ctx is not null)
        {
            meta["request_id"] = ctx.RequestId;
            if (!string.IsNullOrWhiteSpace(ctx.TraceId))
                meta["trace_id"] = ctx.TraceId;
        }

        // Optional auth token (UE side may enforce it)
        var token = Environment.GetEnvironmentVariable("UNREAL_MCP_SECURITY_TOKEN");
        if (!string.IsNullOrWhiteSpace(token))
            meta["token"] = token;

        return meta;
    }

    private static async Task NotifyAsync(string level, string message, JsonNode? data = null)
    {
        var ctx = Log.GetCurrentContext();
        if (data is null)
            data = new JsonObject();

        if (data is JsonObject obj)
        {
            if (ctx is not null)
            {
                obj["request_id"] = ctx.RequestId;
                if (!string.IsNullOrWhiteSpace(ctx.TraceId))
                    obj["trace_id"] = ctx.TraceId;
            }
        }

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

    private static JsonObject BuildNextActionFromToolResult(string toolName, JsonObject toolResult)
    {
        // Conservative, machine-usable suggestions.
        // Prefer tool actions when we can infer them; otherwise emit a hint.

        // Transport errors usually come through as plain text + exception.
        var structured = toolResult["structuredContent"];
        if (structured is JsonObject sObj)
        {
            // If this is a proxied UE response, it might include error_code.
            if (sObj["error_code"] is JsonValue)
            {
                var code = sObj["error_code"]?.GetValue<string>();
                if (code == "ERR_BAD_REQUEST")
                {
                    return new JsonObject { ["type"] = "hint", ["message"] = "检查调用参数是否缺失/类型不匹配（ERR_BAD_REQUEST）。" };
                }

                if (code == "ERR_ASSET_NOT_FOUND" || code == "ERR_AMBIGUOUS_ASSET")
                {
                    return new JsonObject
                    {
                        ["type"] = "hint",
                        ["message"] = "优先传 canonical 的 blueprint_path（/Game/...），避免仅用 name。若有 candidates，请从候选中选择。"
                    };
                }

                if (code == "ERR_WRITE_NOT_ALLOWED")
                {
                    return new JsonObject
                    {
                        ["type"] = "hint",
                        ["message"] = "写入被拦截：检查 DefaultEngine.ini 的 [UnrealMCP] AllowedWriteRoots / bStrictWriteAllowlist。"
                    };
                }

                if (code == "ERR_UNAUTHORIZED")
                {
                    return new JsonObject
                    {
                        ["type"] = "hint",
                        ["message"] = "鉴权失败：设置环境变量 UNREAL_MCP_SECURITY_TOKEN，并与 UE 配置的 [UnrealMCP] SecurityToken 保持一致。"
                    };
                }

                if (code == "ERR_READ_ONLY")
                {
                    return new JsonObject
                    {
                        ["type"] = "hint",
                        ["message"] = "当前 UE 处于只读：关闭 [UnrealMCP] bReadOnly 后重试。"
                    };
                }
            }
        }

        // Default: suggest a ping when anything fails.
        return new JsonObject
        {
            ["type"] = "tool",
            ["name"] = "unreal.ping",
            ["arguments"] = new JsonObject()
        };
    }

    private static async Task NotifyBatchProgressAsync(string level, int current, int total, string step, JsonNode? extra)
    {
        var pct = total <= 0 ? 0 : (int)Math.Round((double)current / total * 100.0);
        var data = new JsonObject
        {
            ["kind"] = "batch_progress",
            ["current"] = current,
            ["total"] = total,
            ["percent"] = pct,
            ["step"] = step
        };

        if (extra is not null)
            data["extra"] = extra;

        await NotifyAsync(level, $"Batch: {current}/{total} ({pct}%) {step}", data);
    }




    private static JsonNode? CloneNode(JsonNode? node)
        => node is null ? null : JsonNode.Parse(node.ToJsonString(JsonUtil.JsonOptions));

    public static JsonObject MakeJsonRpcResponse(JsonNode? id, JsonNode result)
    {
        return new JsonObject
        {
            ["jsonrpc"] = "2.0",
            ["id"] = CloneNode(id),
            ["result"] = CloneNode(result)
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
            result["structuredContent"] = CloneNode(structured);


        // Correlation (helps hosts aggregate logs across tool calls)
        var ctx = Log.GetCurrentContext();
        if (ctx is not null)
        {
            result["_meta"] = new JsonObject
            {
                ["request_id"] = ctx.RequestId,
                ["trace_id"] = ctx.TraceId
            };
        }

        if (isError)
            result["isError"] = true;


        return result;
    }
}
