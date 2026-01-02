# 启动 MCP 服务的完整步骤

## 1. 环境要求
- Python 3.10+
- Unreal Engine 5.5 编辑器

## 2. 依赖安装与环境配置

### 2.1 安装 uv 包管理器（推荐）
```bash
curl -LsSf https://astral.sh/uv/install.sh | sh  # Unix/macOS
# 或在 Windows 上使用 PowerShell
powershell -c "irm https://astral.sh/uv/install.ps1 | iex"
```

### 2.2 创建并激活虚拟环境
```bash
# 创建虚拟环境（将在当前目录下创建.venv文件夹）
uv venv

# 激活虚拟环境
source .venv/bin/activate  # Unix/macOS (在bash/zsh中)
.venv\Scripts\activate     # Windows (在cmd或PowerShell中)
```

**注意事项：**
- 虚拟环境默认在**当前工作目录**下创建，即MCP服务器所在目录
- 所有安装命令在Windows的cmd和PowerShell中都可以完成
- 确保在执行命令前已进入正确的目录（`e:\Work\UGit\UEMCP\unreal-mcp\Python`）

### 2.3 安装项目依赖
```bash
uv pip install -e .
```

## 3. 启动 MCP 服务

### 3.1 启动 Unreal Engine 编辑器
确保 Unreal Engine 5.5 编辑器已启动并加载了项目。

### 3.2 运行 MCP 服务器
```bash
python unreal_mcp_server.py
```

服务器将使用 `stdio` 传输方式启动，这是 MCP 客户端默认支持的方式。

## 4. 客户端配置
配置你的 MCP 客户端（如 Claude Desktop、Cursor、Windsurf）连接到 Unreal MCP 服务器。

## 5. 注意事项

- **Unreal 编辑器必须先启动**：服务器需要连接到运行中的 Unreal 编辑器
- **默认连接配置**：服务器默认尝试连接到 `127.0.0.1:55557`
- **日志文件**：详细日志将记录在 `unreal_mcp.log` 文件中
- **依赖检查**：确保所有依赖都已正确安装

## 6. 测试脚本
如果需要测试功能，可以使用 `scripts` 文件夹中的脚本直接测试，无需启动 MCP 服务器：
```bash
python scripts/actors/test_cube.py
```

## 核心文件说明
- `unreal_mcp_server.py`：MCP 服务器主入口
- `pyproject.toml`：项目配置和依赖声明
- `tools/` 文件夹：包含各种 Unreal 操作工具的实现

按照以上步骤操作，你应该能够成功启动并使用 Unreal MCP 服务。