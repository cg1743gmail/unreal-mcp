using System.Text.Json.Nodes;
using UnrealMCP.Sidecar;
using Xunit;

namespace UnrealMCP.Sidecar.Tests;

public class McpTests
{
    #region TryGetUeSuccess Tests

    [Fact]
    public void TryGetUeSuccess_WithSuccessTrue_ReturnsTrue()
    {
        var ueResp = new JsonObject { ["success"] = true };

        var result = Mcp.TryGetUeSuccess(ueResp, out var success, out var errorMessage);

        Assert.True(result);
        Assert.True(success);
        Assert.Null(errorMessage);
    }

    [Fact]
    public void TryGetUeSuccess_WithSuccessFalse_ReturnsFalseSuccess()
    {
        var ueResp = new JsonObject
        {
            ["success"] = false,
            ["error"] = "Something went wrong"
        };

        var result = Mcp.TryGetUeSuccess(ueResp, out var success, out var errorMessage);

        Assert.True(result);
        Assert.False(success);
        Assert.Equal("Something went wrong", errorMessage);
    }

    [Fact]
    public void TryGetUeSuccess_WithStatusSuccess_ReturnsTrue()
    {
        var ueResp = new JsonObject { ["status"] = "success" };

        var result = Mcp.TryGetUeSuccess(ueResp, out var success, out var errorMessage);

        Assert.True(result);
        Assert.True(success);
    }

    [Fact]
    public void TryGetUeSuccess_WithStatusError_ReturnsFalseSuccess()
    {
        var ueResp = new JsonObject
        {
            ["status"] = "error",
            ["error"] = "Failed to execute"
        };

        var result = Mcp.TryGetUeSuccess(ueResp, out var success, out var errorMessage);

        Assert.True(result);
        Assert.False(success);
        Assert.Equal("Failed to execute", errorMessage);
    }

    [Fact]
    public void TryGetUeSuccess_WithOnlyErrorField_ReturnsFalseSuccess()
    {
        var ueResp = new JsonObject { ["error"] = "Legacy error format" };

        var result = Mcp.TryGetUeSuccess(ueResp, out var success, out var errorMessage);

        Assert.True(result);
        Assert.False(success);
        Assert.Equal("Legacy error format", errorMessage);
    }

    [Fact]
    public void TryGetUeSuccess_WithUnknownShape_ReturnsFalse()
    {
        var ueResp = new JsonObject { ["data"] = "some data" };

        var result = Mcp.TryGetUeSuccess(ueResp, out var success, out var errorMessage);

        Assert.False(result);
        Assert.True(success); // Default value
    }

    [Fact]
    public void TryGetUeSuccess_WithNull_ReturnsFalse()
    {
        var result = Mcp.TryGetUeSuccess(null, out var success, out var errorMessage);

        Assert.False(result);
    }

    [Fact]
    public void TryGetUeSuccess_WithJsonArray_ReturnsFalse()
    {
        var ueResp = new JsonArray { "item1", "item2" };

        var result = Mcp.TryGetUeSuccess(ueResp, out var success, out var errorMessage);

        Assert.False(result);
    }

    #endregion

    #region TryBuildUeBatchParams Tests

    [Fact]
    public void TryBuildUeBatchParams_WithValidCalls_ReturnsResult()
    {
        var calls = new JsonArray
        {
            new JsonObject
            {
                ["name"] = "ping",
                ["arguments"] = new JsonObject()
            },
            new JsonObject
            {
                ["name"] = "get_actors_in_level",
                ["arguments"] = new JsonObject()
            }
        };

        var result = Mcp.TryBuildUeBatchParams(calls, stopOnError: true);

        Assert.NotNull(result);
        Assert.Equal(2, result.Commands.Count);
        Assert.Equal("ping", result.Commands[0].Type);
        Assert.Equal("get_actors_in_level", result.Commands[1].Type);
        Assert.True(result.Params["stop_on_error"]?.GetValue<bool>());
    }

    [Fact]
    public void TryBuildUeBatchParams_WithStopOnErrorFalse_PassesThrough()
    {
        var calls = new JsonArray
        {
            new JsonObject { ["name"] = "ping" }
        };

        var result = Mcp.TryBuildUeBatchParams(calls, stopOnError: false);

        Assert.NotNull(result);
        Assert.False(result.Params["stop_on_error"]?.GetValue<bool>());
    }

    [Fact]
    public void TryBuildUeBatchParams_WithNestedBatch_ReturnsNull()
    {
        var calls = new JsonArray
        {
            new JsonObject { ["name"] = "ping" },
            new JsonObject { ["name"] = "unreal.batch" }
        };

        var result = Mcp.TryBuildUeBatchParams(calls, stopOnError: true);

        Assert.Null(result);
    }

    [Fact]
    public void TryBuildUeBatchParams_WithMissingName_ReturnsNull()
    {
        var calls = new JsonArray
        {
            new JsonObject { ["arguments"] = new JsonObject() }
        };

        var result = Mcp.TryBuildUeBatchParams(calls, stopOnError: true);

        Assert.Null(result);
    }

    [Fact]
    public void TryBuildUeBatchParams_WithUnknownTool_ReturnsNull()
    {
        var calls = new JsonArray
        {
            new JsonObject { ["name"] = "unknown_tool" }
        };

        var result = Mcp.TryBuildUeBatchParams(calls, stopOnError: true);

        Assert.Null(result);
    }

    [Fact]
    public void TryBuildUeBatchParams_WithUnrealPing_MapsCorrectly()
    {
        var calls = new JsonArray
        {
            new JsonObject { ["name"] = "unreal.ping" }
        };

        var result = Mcp.TryBuildUeBatchParams(calls, stopOnError: true);

        Assert.NotNull(result);
        Assert.Single(result.Commands);
        Assert.Equal("ping", result.Commands[0].Type);
    }

    [Fact]
    public void TryBuildUeBatchParams_WithSendCommand_ExtractsTypeAndParams()
    {
        var calls = new JsonArray
        {
            new JsonObject
            {
                ["name"] = "unreal.send_command",
                ["arguments"] = new JsonObject
                {
                    ["type"] = "custom_command",
                    ["params"] = new JsonObject { ["key"] = "value" }
                }
            }
        };

        var result = Mcp.TryBuildUeBatchParams(calls, stopOnError: true);

        Assert.NotNull(result);
        Assert.Single(result.Commands);
        Assert.Equal("custom_command", result.Commands[0].Type);
    }

    [Fact]
    public void TryBuildUeBatchParams_WithCreateActor_MapsToSpawnActor()
    {
        var calls = new JsonArray
        {
            new JsonObject { ["name"] = "create_actor" }
        };

        var result = Mcp.TryBuildUeBatchParams(calls, stopOnError: true);

        Assert.NotNull(result);
        Assert.Equal("spawn_actor", result.Commands[0].Type);
    }

    #endregion

    #region ToolCallResultFromUeResponse Tests

    [Fact]
    public void ToolCallResultFromUeResponse_WithNull_ReturnsError()
    {
        var result = Mcp.ToolCallResultFromUeResponse("test", null);

        Assert.True(result["isError"]?.GetValue<bool>());
        Assert.Contains("No response", result["content"]?[0]?["text"]?.GetValue<string>());
    }

    [Fact]
    public void ToolCallResultFromUeResponse_WithSuccessTrue_ReturnsNoError()
    {
        var ueResp = new JsonObject { ["success"] = true, ["data"] = "test" };

        var result = Mcp.ToolCallResultFromUeResponse("test", ueResp);

        Assert.Null(result["isError"]);
    }

    [Fact]
    public void ToolCallResultFromUeResponse_WithSuccessFalse_ReturnsError()
    {
        var ueResp = new JsonObject { ["success"] = false, ["error"] = "Test error" };

        var result = Mcp.ToolCallResultFromUeResponse("test", ueResp);

        Assert.True(result["isError"]?.GetValue<bool>());
        Assert.Contains("Test error", result["content"]?[0]?["text"]?.GetValue<string>());
    }

    #endregion

    #region IsProxiedCommand Tests

    [Fact]
    public void IsProxiedCommand_WithValidCommand_ReturnsTrue()
    {
        Assert.True(Mcp.IsProxiedCommand("ping"));
        Assert.True(Mcp.IsProxiedCommand("get_actors_in_level"));
        Assert.True(Mcp.IsProxiedCommand("spawn_actor"));
    }

    [Fact]
    public void IsProxiedCommand_WithInvalidCommand_ReturnsFalse()
    {
        Assert.False(Mcp.IsProxiedCommand("unknown_command"));
        Assert.False(Mcp.IsProxiedCommand(""));
        Assert.False(Mcp.IsProxiedCommand("PING")); // Case sensitive
    }

    #endregion

    #region MakeJsonRpcResponse Tests

    [Fact]
    public void MakeJsonRpcResponse_CreatesValidResponse()
    {
        var result = Mcp.MakeJsonRpcResponse(1, new JsonObject { ["data"] = "test" });

        Assert.Equal("2.0", result["jsonrpc"]?.GetValue<string>());
        Assert.Equal(1, result["id"]?.GetValue<int>());
        Assert.NotNull(result["result"]);
    }

    [Fact]
    public void MakeJsonRpcError_CreatesValidError()
    {
        var result = Mcp.MakeJsonRpcError(1, -32600, "Invalid Request");

        Assert.Equal("2.0", result["jsonrpc"]?.GetValue<string>());
        Assert.Equal(1, result["id"]?.GetValue<int>());
        Assert.Equal(-32600, result["error"]?["code"]?.GetValue<int>());
        Assert.Equal("Invalid Request", result["error"]?["message"]?.GetValue<string>());
    }

    #endregion
}
