"""通过 UnrealMCP（UE 内 Socket Server: 127.0.0.1:55557）测试 get_interchange_pipelines

用法：
- 确保 UE Editor 已打开项目，并已加载 UnrealMCP 插件
- 运行：python TEST_GET_PIPELINES.py
"""

import json
import socket

HOST = "127.0.0.1"
PORT = 55557


def send_command(cmd_type: str, params: dict):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(15.0)
    sock.connect((HOST, PORT))

    payload = json.dumps({"type": cmd_type, "params": params}, ensure_ascii=False).encode("utf-8")
    sock.sendall(payload)

    chunks: list[bytes] = []
    while True:
        try:
            chunk = sock.recv(8192)
            if not chunk:
                break
            chunks.append(chunk)

            try:
                data = b"".join(chunks).decode("utf-8", errors="replace")
                return json.loads(data)
            except Exception:
                continue
        except socket.timeout:
            break

    if chunks:
        data = b"".join(chunks).decode("utf-8", errors="replace")
        return json.loads(data)
    return {"success": False, "message": "no response"}


def main():
    print("Testing MCP Connection...")
    ping = send_command("ping", {})
    print(json.dumps(ping, indent=2, ensure_ascii=False))

    print("\nCalling get_interchange_pipelines...")
    outer = send_command("get_interchange_pipelines", {"search_path": "/Game/"})
    print(json.dumps(outer, indent=2, ensure_ascii=False))

    res = outer.get("result") if isinstance(outer, dict) else None
    if not isinstance(res, dict):
        print("\n[ERROR] Unexpected response shape (missing 'result')")
        return

    keys = [
        "scanned_blueprint_asset_count",
        "rejected_blueprint_asset_count",
        "accepted_by_gen_is_pipeline_base",
        "accepted_by_gen_is_blueprint_base",
        "accepted_by_parent_is_pipeline_base",
        "accepted_by_parent_is_blueprint_base",
        "blueprint_count",
        "native_count",
        "total_count",
        "search_path",
        "filter_mode",
    ]
    print("\nCounts:")
    for k in keys:
        if k in res:
            print(f"  {k}: {res.get(k)}")

    bps = res.get("blueprint_pipelines")
    if isinstance(bps, list):
        print("\nBlueprint pipelines:")
        for p in bps:
            print(
                f"- {p.get('name')} | gen={p.get('class')} | parent={p.get('parent_class')} | {p.get('resolved_asset_path') or p.get('path')}"
            )


if __name__ == "__main__":
    main()
