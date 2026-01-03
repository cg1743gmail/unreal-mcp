# UnrealMCP Sidecar（Windows EXE MCP Server）

`UnrealMCP.Sidecar` 是一个**独立 EXE 进程**，对 MCP Client 提供 **stdio MCP（JSON-RPC + Content-Length 分帧）**，并把 `tools/call` 转发到 Unreal Editor 内的 `UnrealMCP` 插件执行（默认 `127.0.0.1:55557`）。

- **适用场景**：在 Windows 下不想依赖 Python/uv 环境，或者希望更稳定的协议实现与更强的可观测性。
- **架构定位**：路线 B1（Sidecar EXE = MCP Server；UE 插件 = 执行端）。

---

## 快速开始（推荐）

### 1) 前置条件

- **Unreal Editor 已启动**，并启用 `UnrealMCP` 插件（插件负责在本机端口提供执行服务）。
- Windows 已安装 `.NET 8`（framework-dependent 发布）。

### 2) 发布 Sidecar EXE

在仓库根目录执行：

```bat
Sidecar\publish_sidecar.bat
```

输出位置：
- `Sidecar/publish/win-x64/UnrealMCP.Sidecar.exe`

### 3) 配置 MCP Client

仓库已提供示例配置：`mcp.sidecar.json`。

你可以把其中 `command` 改成你本机的实际绝对路径（建议用正斜杠 `/`，Windows 也能识别）：

```json
{
  "mcpServers": {
    "unrealMCP-sidecar": {
      "command": "<ABS_PATH>/Sidecar/publish/win-x64/UnrealMCP.Sidecar.exe",
      "args": ["--ue-host","127.0.0.1","--ue-port","55557","--timeout-ms","10000"]
    }
  }
}
```

常见 MCP Client 配置位置：
- Claude Desktop：`%USERPROFILE%\.config\claude-desktop\mcp.json`
- Cursor：项目根目录 `.cursor/mcp.json`
- Windsurf：`%USERPROFILE%\.config\windsurf\mcp.json`

### 4) 验证

- 版本号：

```powershell
Sidecar\publish\win-x64\UnrealMCP.Sidecar.exe --version
```

- UE 连通性探针（UE 未启动/端口不通会返回 `unhealthy` 且 exit code=1）：

```powershell
Sidecar\publish\win-x64\UnrealMCP.Sidecar.exe --health
```

---

## 配置项（CLI / 环境变量）

### CLI 参数

- `--ue-host <ip/host>`：默认 `127.0.0.1`
- `--ue-port <port>`：默认 `55557`
- `--timeout-ms <ms>`：默认 `10000`
- `--version` / `-v`：输出版本并退出
- `--health`：检查 UE 可连接性并输出 `healthy/unhealthy`

### 环境变量

- **UE 连接参数（覆盖默认值，适合放在 IDE 配置里）**
  - `UNREAL_MCP_HOST` / `UNREAL_MCP_PORT` / `UNREAL_MCP_TIMEOUT_MS`
- **日志格式**
  - `UNREAL_MCP_LOG_JSON=1`：stderr 输出 JSON 结构化日志
- **stdio 输入兼容模式（仅开发/调试）**
  - `UNREAL_MCP_STDIO_ALLOW_LINE_JSON=1`：允许“单行 JSON（以换行结尾）”作为 stdin 输入（默认强制 Content-Length 分帧）
- **resources 扩展**
  - `UNREAL_MCP_RESOURCES_ENABLE_UE=1`：开启 UE 侧资源读取（会触发对 UE 的调用）

---

## MCP 协议与能力概览

### Transport（stdio）

- **默认要求**：`Content-Length: <n>\r\n\r\n{...json...}`（MCP/LSP 风格）。
- **仅调试**：可以用 `UNREAL_MCP_STDIO_ALLOW_LINE_JSON=1` 放宽为“单行 JSON”。

### capabilities

- **tools**：包含 45+ Unreal 工具，并带详细 `inputSchema`
- **prompts**：提供 `unreal.usage_guide`
- **resources**：提供静态资源（连接信息、工具列表、usage guide），以及可选 UE 资源

### 关键工具

- **`unreal.ping`**：检测 UE 插件连通性
- **`unreal.send_command`**：透传任意 UE 命令（调试/兜底用）
- **`unreal.batch`**：顺序执行多个 tool call
  - 先尝试 UE 侧 `batch`（如果 UE 插件支持且参数可映射）
  - 失败则回退为 Sidecar 本地逐条执行

---

## 进阶：手工冒烟测试（不依赖 MCP Client）

Sidecar 使用 stdio 的 `Content-Length` 分帧。你可以用 PowerShell 手动喂一个最小 `tools/list`：

```powershell
$json = '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}'
$bytes = [System.Text.Encoding]::UTF8.GetBytes($json)
$header = "Content-Length: $($bytes.Length)`r`n`r`n"
$payload = [System.Text.Encoding]::ASCII.GetBytes($header) + $bytes
$payload | & "Sidecar\publish\win-x64\UnrealMCP.Sidecar.exe"
```

---

## 常见问题（FAQ / 排错）

### 1) `--health` 显示 `unhealthy`

- 确认 UE Editor 已启动，并且启用了 `UnrealMCP` 插件。
- 确认端口：默认是 `55557`（可用 `--ue-port` 改）。
- 如果 `error` 中出现超时/拒绝连接：检查防火墙、本机端口占用、UE 插件日志。

### 2) 报错：`Missing Content-Length header` 或 `Line-delimited JSON on stdio is disabled`

说明你的输入不是 MCP/LSP 风格分帧。
- 正确做法：让 MCP Client 启动并以标准 MCP 分帧发送。
- 调试临时放宽：设置 `UNREAL_MCP_STDIO_ALLOW_LINE_JSON=1`。

### 3) 报错：`Unexpected UE response framing (not len32le and not raw JSON)`

Sidecar 期望 UE 回包为 `len32le`（4 字节长度前缀 + JSON），否则只能在检测到以 `{`/`[` 开头时回退到 raw JSON。
- 如果你的 UE 插件较旧：建议升级插件，或至少确保回包是 JSON。

### 4) 想把日志接入 ELK/Loki

设置：`UNREAL_MCP_LOG_JSON=1`，Sidecar 会把 stderr 日志变成 JSON（包含 `ts/level/msg/data`）。

---

## 设计与路线文档

- **实施路线（P0→P2 / UE 插件升级建议）**：`p0-p2-implementation-guide.md`
- **路线 B 研究（B1/B2 的工程形态与接口建议）**：`route-b-sidecar-mcp-server.md`

---

## 版本与变更

变更记录：`../CHANGELOG.md`
