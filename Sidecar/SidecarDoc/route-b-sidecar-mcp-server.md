### 路线 B 研究文档：UE 侧原生 MCP Server 的可落地形态（B1/B2）

> 本文档用于把“路线 B（UE 插件直接实现 MCP Server）”落成可实施的工程方案，并给出模块拆分与接口定义。  
> **现状**：仓库已落地 B1（Sidecar EXE stdio MCP + UE 插件执行端），使用说明见 `Sidecar/SidecarDoc/README.md`；本文档作为架构研究与后续演进参考。

---


## 1. 背景与约束

### 1.1 当前仓库的现实形态（供对照）
- **MCP Server 在 Python**：`Python/unreal_mcp_server.py` 使用 `FastMCP`（stdio transport），对外暴露 tools。
- **UE 插件是执行端**（Editor-only）：通过本机 Socket（TCP 127.0.0.1:55557）接收 JSON，然后在 `UUnrealMCPBridge::ExecuteCommand()` 分发。
- **Python ↔ UE 不是 MCP**：是自定义 `{"type": "...", "params": {...}}` 消息。

### 1.2 “路线 B” 的硬约束（为什么不能“只写 UE 插件就 stdio MCP”）
多数 MCP Client 的典型模式是：**启动一个独立进程**作为 MCP Server，并通过 **stdio** 交换 JSON-RPC。
- UE 插件（DLL）不是可执行文件，**无法被 MCP Client 直接以 stdio 方式启动**。

因此，路线 B 在工程上通常落成两种形态：
- **B1（推荐）**：独立 Sidecar EXE（stdio MCP Server） + UE 插件执行端（本机 IPC）。
- **B2（条件型）**：UE Editor 内部直接开放 MCP 网络服务（HTTP/SSE 或 WebSocket）。
  - 仅当目标 MCP Client 支持网络 transport 才适用；否则兼容性很差。

---

## 2. 方案总览

### 2.1 B1：Sidecar EXE（stdio MCP） + UE 插件（执行端）
- **对 MCP Client**：Sidecar 是标准 MCP Server（stdio）。
- **对 UE**：UE 插件提供“内部 RPC API”，负责蓝图图编辑的所有强一致性操作。
- **优点**：兼容面最大；协议层可独立测试；UE 内只做执行与校验。
- **缺点**：多一跳 IPC；需要维护一个 EXE 分发。

### 2.2 B2：UE 内直接 MCP 网络服务
- **优点**：少一个进程；UE 内更容易直接接触资源系统。
- **缺点**：取决于 MCP Client transport 支持；UE 内实现协议与并发隔离更复杂。

**本文档重点：B1**（更通用、更可交付）。

---

## 3. 工程拆分（模块清单）

### 3.1 Sidecar：`UnrealMCP.Sidecar`（独立 EXE）

#### **职责**
- MCP 协议实现（stdio）：tool 列表、tool 调用、结构化错误。
- 对 UE 插件发起内部 RPC：batch、超时、重试、归一化错误。
- （可选）高阶编排；建议先以“可组合原语”完成通用能力。

#### **推荐模块**
- **`McpHost`**：JSON-RPC 路由、`list_tools`、`call_tool`、日志与 trace。
- **`UeTransport`**：与 UE 插件通信（建议 Named Pipe，备选 TCP loopback）。
- **`Contracts`**：请求/响应 envelope、错误码、版本、数据结构（Graph/Node/Pin）。
- **`ToolAdapters`**：每个 MCP tool → 一个内部 RPC 或一个 batch RPC。
- **`Observability`**：结构化日志、请求耗时统计、超时与失败率。

### 3.2 UE 插件：`UnrealMCP`（Editor-only 执行端）

#### **职责**
- 维护本机 IPC Server（pipe/tcp），收包、分帧、按 `request_id` 回包。
- 统一在 GameThread 执行蓝图编辑（避免线程安全问题）。
- 提供稳定的“内部命令 API”：资产定位、图快照、节点生成、修改原语、编译、保存、事务。
- 提供安全策略：路径白名单、只读模式、限流。

#### **推荐模块**
- **`ServerRuntime`**：连接管理、消息分帧、队列、`request_id` 关联。
- **`AssetCommands`**：蓝图资产定位、加载、保存。
- **`GraphIntrospection`**：导出 graph snapshot（nodes/pins/links）。
- **`GraphEditing`**：原子图编辑（set pin、connect、delete、move、spawn node）。
- **`CompileCommands`**：编译与诊断输出。
- **`SecurityPolicy`**：路径白名单、dry-run、操作审计。

---

## 4. 内部 RPC（Sidecar ↔ UE）接口定义

### 4.1 消息信封（Envelope）
> 所有内部命令都使用统一 envelope，避免散落的 ad-hoc JSON。

#### Request
```json
{
  "id": "<string>",
  "api_version": 1,
  "command": "bp.get_graph_snapshot",
  "params": { }
}
```

#### Response
```json
{
  "id": "<string>",
  "ok": true,
  "result": { }
}
```

失败时：
```json
{
  "id": "<string>",
  "ok": false,
  "error": {
    "code": "BP_ASSET_NOT_FOUND",
    "message": "Blueprint not found",
    "details": { }
  }
}
```

### 4.2 必须解决：消息分帧（Framing）
现有实现依赖“单次 `recv` 就拿到完整 JSON”，稳定性不足。
推荐二选一：
- **长度前缀**：`uint32 length` + `payload JSON bytes`
- **换行分隔**：要求 payload 绝不包含裸换行（实现复杂，且容易踩坑）

B1 建议优先 **长度前缀**。

---

## 5. 关键数据结构（Contracts）

### 5.1 `blueprint_path`
- 强烈建议所有接口使用 `blueprint_path`（UE 对象路径），例如：
  - `/Game/UI/WBP_HUD.WBP_HUD`
- 允许 Sidecar 提供 `resolve_asset` 工具把 name → path。

### 5.2 `graph_ref`
```json
{
  "kind": "ubergraph",
  "name": "EventGraph"
}
```
可扩展：function/macro 等。

### 5.3 `node_id` 与 `pin_ref`
- `node_id`：使用 `NodeGuid` 字符串可作为第一阶段方案。
- `pin_ref`：不要只用 `pin_name`（不稳定）；建议返回并接受：
  - `pin_id`（优先：可稳定定位）
  - 过渡期：`pin_name + direction + index` 并在 snapshot 里给足消歧信息。

---

## 6. MCP Tools（Sidecar 对外）建议清单

### 6.1 读取类
- **`ue.ping()`**：验证 UE 插件连接。
- **`bp.resolve_asset(query)`**：name/path → `blueprint_path`（或候选列表）。
- **`bp.get_graph_snapshot(blueprint_path, graph_ref, options)`**：读图的唯一权威入口。

### 6.2 写入类（原子操作）
- **`bp.batch(ops[])`**：单事务执行多步（强烈推荐作为主要写入入口）。
- **`bp.create_node(action_ref, blueprint_path, graph_ref, position, init)`**
- **`bp.set_pin_default(node_id, pin_ref, value)`**
- **`bp.set_pin_object(node_id, pin_ref, object_path)`**
- **`bp.connect(src, dst)`** / **`bp.disconnect(...)`**
- **`bp.delete_node(node_id)`** / **`bp.move_node(node_id, x, y)`**

### 6.3 工程化
- **`bp.compile(blueprint_path)`**：返回结构化错误（errors/warnings）。
- **`bp.save(blueprint_path)`**：明确保存结果。

---

## 7. “通用创建节点”的实现策略（Action/Spawner 驱动）

### 7.1 为什么需要 Action/Spawner
硬编码 `UK2Node_*` 只能覆盖少量节点，无法通用。
建议 UE 侧提供：
- **`bp.list_node_actions(query, context)`**：返回可创建的节点 action。
- **`bp.create_node(action_id, ...)`**：用 `BlueprintNodeSpawner` / ActionDatabase / SchemaAction 生成节点。

### 7.2 Sidecar 的角色
Sidecar 不需要理解 UE 节点内部细节：
- 搜索 action → spawn → set defaults → connect

---

## 8. 强一致性要求（UE 侧必须保证）

### 8.1 事务与撤销
- 写操作必须用 `FScopedTransaction` + 对相关对象调用 `Modify()`。

### 8.2 蓝图脏标记与结构性修改
- 只改 pin 默认值/连线：通常 `MarkBlueprintAsModified`。
- 增删节点、增删变量、改图结构：需要结构性修改标记（至少区分并触发必要刷新）。

### 8.3 编译与保存
- `compile` 必须返回结构化诊断（错误列表/严重性/节点定位）。
- `save` 必须显式落盘（避免“改了但没保存”的错觉）。

---

## 9. 安全与治理（上线必备）
- **路径白名单**：只允许写指定 `/Game/...` 子树。
- **只读模式 / dry-run**：先只读后写，降低风险。
- **限流与串行写**：写操作在 UE 内建议串行化，避免并发修改同一图。
- **审计日志**：记录 batch ops 与结果，支持回放排查。

---

## 10. 里程碑（可交付拆分）

### M1（可演示读图）
- Sidecar stdio MCP + UE IPC + `ue.ping` + `bp.resolve_asset` + `bp.get_graph_snapshot`

### M2（可写可落盘）
- `bp.batch` + `set_pin_default` + `connect/disconnect` + `delete/move` + `compile/save`

### M3（通用创建）
- `list_node_actions` + `create_node(action_id)`

### M4（稳定化）
- `pin_ref` 稳定化、错误码体系完善、回滚语义、权限策略与审计完善

---

## 11. 风险清单（提前规避）
- **Transport 兼容性**：若目标 MCP Client 不支持网络 transport，则 B2 不可用；B1 兼容性最好。
- **图编辑稳定性**：缺事务/Modify/结构性标记会导致不可预期编辑器状态。
- **Pin 定位不稳定**：只靠 `pin_name` 容易失败；需要稳定 `pin_ref`。
- **批处理回滚**：没有 batch + rollback，复杂编辑会频繁落在半完成状态。

---

### 附：与现有实现的映射建议（用于迁移）
- 保留并升级 `UUnrealMCPBridge::ExecuteCommand()`：把 `command/type` 统一为 `command`，并引入 `api_version` 与 `id`。
- 逐步把现有 `add_blueprint_*` 变成 `create_node(action_ref...)` 的特化调用，而不是继续扩增硬编码节点。
