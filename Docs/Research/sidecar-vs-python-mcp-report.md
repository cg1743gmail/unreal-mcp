# Sidecar vs Python（unreal_mcp_server）技术架构与功能评分报告

> 版本：v1（生成于 2026-01-03）
>
> 范围：对比本仓库中两种 MCP Server 实现：
> - Sidecar：`Sidecar/UnrealMCP.Sidecar`（.NET 8 EXE）
> - Python：`Python/unreal_mcp_server.py`（FastMCP）
>
---

## 1. 背景与目标

本项目通过 MCP（Model Context Protocol）让 MCP Client（如 CodeBuddy / Cursor / Windsurf / Claude Desktop）以自然语言调用工具，最终驱动 Unreal Editor 内的 `UnrealMCP` 插件执行命令。

该仓库同时提供两种 MCP Server 形态：
- **Python Server**：更偏“参考实现/开发态桥接”，以 Python 生态快速扩展工具。
- **Sidecar EXE**：更偏“Windows 生产可用前端”，强调协议正确性、稳定性、强 schema、可观测性与治理能力。

---

## 2. 架构总览（数据流与边界）

### 2.1 Sidecar（.NET 8 EXE）

**链路**：MCP Client ⇄（stdio MCP, Content-Length 分帧）⇄ Sidecar ⇄（TCP）⇄ UE 插件（默认 `127.0.0.1:55557`）

**关键实现点**（来自代码）：
- MCP stdio：Sidecar 自己实现读取/写入，按 MCP/LSP 风格要求 `Content-Length: <n>\r\n\r\n{json}`。
- 初始化：`initialize` 返回 `serverInfo/capabilities`，并在 `_meta` 中返回 `unrealConnection`（UE 连接状态）与 `supportsNotifications`。
- UE 回包分帧：发送请求时携带 `_mcp.response_framing = "len32le"`，期望 UE 回包为 `len32le`（4 字节长度前缀 + JSON），仅在检测到 `{`/`[` 时才回退到 raw JSON 兼容。
- 可观测性：请求级 `request_id/trace_id` scope，支持 `UNREAL_MCP_LOG_JSON=1` 输出结构化日志。
- 批量工具：提供 `unreal.batch`（可选进度通知/失败 next_action、UE-batch 优先 + fallback）。
- Prompt/Resources：提供 `unreal.usage_guide` prompt 与 `resources`（可选 UE-backed resources）。

### 2.2 Python（FastMCP）

**链路**：MCP Client ⇄（FastMCP stdio）⇄ Python Server ⇄（TCP）⇄ UE 插件（默认 `127.0.0.1:55557`）

**关键实现点**（来自代码）：
- MCP：基于 `FastMCP` 实现 stdio MCP。
- 工具注册：通过 `Python/tools/*.py` 使用 `@mcp.tool()` 注册大量工具；prompt 使用 `@mcp.prompt()`（函数 `info`）。
- UE 回包拼包：`receive_full_response` 采取“不断累积 bytes，持续尝试 `json.loads()` 成功即认为收齐”的策略；超时后尝试用已收齐的部分做一次解析。
- 日志：主要写入 `unreal_mcp.log`，并显式避免 stdout handler（降低协议污染风险，但仍需要工程纪律）。

---

## 3. 核心差异对比（你实际会感受到的区别）

### 3.1 协议健壮性（stdio / MCP 兼容性）

- **Sidecar**：自管 `Content-Length` 分帧读写，工程上更可控；显式忽略 `notifications/initialized` 等通知请求。
- **Python**：依赖 FastMCP 框架；整体可用，但更容易被“额外 stdout 输出/print”破坏协议（需要持续约束）。

### 3.2 UE 回包可靠性（大包/分包/超时）

- **Sidecar**：优先 `len32le`，通过长度字段一次性读取完整 JSON，协议边界清晰；raw JSON 兜底更严格（仅首字节像 JSON 才尝试）。
- **Python**：通过反复 `json.loads()` 判断消息完整性，在边界场景（大消息、分包、多响应、非 JSON 前缀字节、超时）更容易出不一致行为。

### 3.3 工具 schema 完整度（AI 是否更“填得对参数”）

- **Sidecar**：内置 `ToolSchemas`，为 45+ 工具提供完整 `inputSchema`（类型、required、默认值、数组长度等）。
- **Python**：更多依赖 Python 函数签名/注释推导，schema 表达能力与一致性弱于 Sidecar。

### 3.4 可观测性与追踪（排错效率）

- **Sidecar**：请求级 `request_id/trace_id` + `UNREAL_MCP_LOG_JSON=1`，更适合在 IDE/CI/日志平台里追踪。
- **Python**：偏单机文件日志，缺少端到端 correlation id（至少在当前仓库实现中如此）。

### 3.5 安全与治理（鉴权/只读/审计）

- **Sidecar**：会读取 `UNREAL_MCP_SECURITY_TOKEN` 并作为 `_mcp.token` 传给 UE（可配合 UE 侧 `[UnrealMCP] SecurityToken` 做鉴权）；也更适合做只读/白名单等“可控写入”。
- **Python**：当前实现更偏功能桥接，没有同等成熟的 token/trace 体系。

---

## 4. 功能面清单对比

| 项 | Sidecar | Python |
|---|---:|---:|
| stdio MCP 分帧（Content-Length） | ✅（自实现） | ✅（FastMCP） |
| `initialize` 返回 UE 连接状态 | ✅（`_meta.unrealConnection`） | ⚠️（框架默认能力，未见同等级 meta） |
| `tools/list` 强 schema（45+） | ✅（`ToolSchemas`） | ⚠️（签名推导为主） |
| `unreal.batch`（notify + fallback） | ✅ | ⚠️（未见同等级能力） |
| `prompts`（`unreal.usage_guide`） | ✅ | ✅（`info`） |
| `resources`（静态/可选 UE-backed） | ✅ | ⚠️（未见同等级能力） |
| CLI `--health/--version` | ✅ | ❌ |
| request/trace correlation | ✅ | ⚠️ |
| token 透传（`UNREAL_MCP_SECURITY_TOKEN`） | ✅ | ⚠️ |

---

## 5. 评分报告（10 分制，含权重）

> 评分基于“仓库当前实现的工程化成熟度”，不是语言优劣。

| 维度 | 权重 | Sidecar | Python | 评语 |
|---|---:|---:|---:|---|
| 部署/依赖成本 | 12% | 9.0 | 6.5 | Sidecar 仅 .NET 8；Python 依赖 uv/venv/包环境 |
| MCP 协议健壮性 | 15% | 9.0 | 7.5 | Sidecar 自管分帧与输出约束更直接 |
| UE 回包与分帧可靠性 | 15% | 8.5 | 6.5 | Sidecar `len32le` 边界清晰；Python 以 json.loads 判定完整 |
| 工具 schema 完整度 | 15% | 9.5 | 6.5 | Sidecar 内置详细 `inputSchema` |
| 可观测性/可追踪性 | 10% | 9.0 | 6.0 | Sidecar `request_id/trace_id` + JSON logs |
| 安全/治理能力 | 10% | 8.0 | 4.5 | Sidecar 支持 token 透传与更清晰治理路径 |
| 功能面（prompts/resources/batch/notify） | 13% | 9.0 | 6.5 | Sidecar 能力更产品化 |
| 可扩展性（加新工具成本） | 10% | 7.0 | 8.5 | Python 修改工具更快；Sidecar 需同步 schema/发布 |
| **综合得分（加权）** | **100%** | **8.7/10** | **6.6/10** | Windows + 长期使用更推荐 Sidecar |

---

## 6. 结论（选型建议）

- **优先选 Sidecar**：
  - 你在 Windows 上用 CodeBuddy/Cursor 等 IDE 长期接入；
  - 你希望“更稳定 + 强 schema + 进度通知 + 更可观测 + 可做鉴权/只读治理”。

- **优先选 Python**：
  - 你需要快速迭代工具/做实验；
  - 可接受环境依赖与偶发边界问题；
  - 你希望用 Python 生态快速新增/修改工具。

---

## 7. 落地建议（可执行清单）

### 7.1 推荐落地组合（Windows + IDE）

- **推荐主链路**：CodeBuddy（MCP Client）→ Sidecar（MCP Server）→ UE 插件（执行端）
- **保留 Python**：作为工具开发/原型验证通道；成熟后再把 schema/关键能力固化到 Sidecar。

### 7.2 CodeBuddy 配置建议（双环境）

在 `~/.codebuddy/mcp.json` 中保留两个 server：
- `unrealMCP-sidecar`：日常使用（默认启用）
- `unrealMCP-python`：开发态（默认禁用，需要时启用）

这样可以在不改 UE 的情况下，快速切换“稳定执行”与“快速迭代”。

### 7.3 UE 工程治理建议（写入安全 + 审计）

- **写入范围**：将 `AllowedWriteRoots` 保持在一个明确根目录（如 `/Game/UnrealMCP/`），并打开 `bStrictWriteAllowlist=True`。
- **只读开关**：当你只想做查询/检查时，可启用 `[UnrealMCP] bReadOnly=True`（或临时打开），避免误写资产。
- **鉴权（可选但推荐）**：
  - UE 侧配置 `[UnrealMCP] SecurityToken=<token>`
  - Sidecar 侧在 CodeBuddy 的 MCP server `env` 中设置 `UNREAL_MCP_SECURITY_TOKEN=<token>`
  - 这样可以避免本机其他进程随意连端口执行写操作。

### 7.4 可观测性与排错建议

- **开启结构化日志（推荐）**：在 CodeBuddy 的 Sidecar MCP server `env` 里设置 `UNREAL_MCP_LOG_JSON=1`，便于搜索 `request_id/trace_id`。
- **健康检查**：
  - 开发机上先用 `UnrealMCP.Sidecar.exe --health` 判断端口与 UE 插件是否可连；
  - `unreal.ping` 用于 MCP 侧 end-to-end 检测。

### 7.5 迭代策略（避免重复劳动）

- 新工具优先在 Python 侧快速验证（接口、参数、UE 命令形态）。
- 形成稳定工具后，再把：
  - `inputSchema`（Sidecar `ToolSchemas`）
  - 关键提示词/用法（`unreal.usage_guide`）
  - 进度通知/批处理模式
  逐步固化到 Sidecar，确保 IDE 侧体验与稳定性。
