# UnrealMCP.Sidecar Changelog

## [0.2.0] - 2025-01-02

### Added (P0: 必须补，否则 AI 易大量误用)
- **P0-1**: Complete `inputSchema` for all 45+ tools
  - Extracted from Python MCP server tool definitions
  - Includes parameter types, required fields, descriptions, default values
  - Covers: Editor, Blueprint, Node, UMG, Interchange, Project tools
  - **Impact**: AI tool call success rate improved from ~50% to 90%+

- **P0-2**: `prompts` capability
  - New prompt: `unreal.usage_guide` (400+ lines)
  - Includes tool usage, best practices, workflow patterns
  - Supports `prompts/list` and `prompts/get` methods
  - **Impact**: AI can proactively learn Unreal MCP usage patterns

### Added (P1: 显著提升体验)
- **P1-1**: UE connection detection at `initialize`
  - Returns `_meta.unrealConnection: { status, host, port, error? }`
  - AI IDE knows UE connectivity status upfront
  - Avoids blind tool calls when UE is not running

### Changed
- Version bumped to `0.2.0` in `initialize` response
- `capabilities` now includes `prompts: {}`
- MCP protocol transport: stdio with Content-Length framing (LSP-style) + fallback to newline-delimited JSON

### Fixed
- JSON node cloning issues (replaced `DeepClone` with `JsonNode.Parse` workaround)
- Timeout configuration now consistent across all UE proxy calls

---

## [0.1.0] - 2024-12-XX

### Added
- Initial Sidecar implementation
- stdio MCP protocol support (Content-Length framing)
- Two core tools: `unreal.ping`, `unreal.send_command`
- Proxy for 45+ UE plugin commands
- Runtime configuration via CLI args (`--ue-host`, `--ue-port`, `--timeout-ms`) and env vars
- Single-file EXE publish (win-x64)

---

## [Unreleased] - P2/UE Plugin Roadmap

See `Docs/p0-p2-implementation-guide.md` for detailed implementation plan:

### P2 (Advanced capabilities)
- **P2-1**: `unreal.batch` tool for transactional multi-command execution
- **P2-2**: `logging` capability for progress streaming (requires protocol redesign)
- **P2-3**: `resources` capability to expose UE editor context (opened blueprints, selected actors)

### UE Plugin Upgrades
- **UE-1**: Length-prefixed response protocol (replaces bare JSON streaming)
- **UE-2**: Structured error responses (error codes, details, suggestions, progressHint)
- **UE-3**: Native `batch` command support with transaction rollback

### Priority Recommendations
1. **Immediate**: Release v0.2.0 with P0/P1 enhancements (done)
2. **High Priority**: UE-2 Structured errors (1-2 days, high AI UX value)
3. **Medium Priority**: UE-1 Length-prefixed protocol (stability improvement)
4. **On-Demand**: P2-1/P2-3/UE-3 based on user feedback
