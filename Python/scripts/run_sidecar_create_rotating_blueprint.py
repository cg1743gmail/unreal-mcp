#!/usr/bin/env python
"""Run UnrealMCP.Sidecar as a stdio MCP server and create a rotating mesh Blueprint.

Prereqs:
- Unreal Editor running + UnrealMCP plugin enabled (TCP: 127.0.0.1:55557)
- Sidecar built at: Sidecar/publish/win-x64/UnrealMCP.Sidecar.exe

This script speaks MCP JSON-RPC over stdio using Content-Length framing.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import time
from datetime import datetime


def _frame(payload: bytes) -> bytes:
    return f"Content-Length: {len(payload)}\r\n\r\n".encode("ascii") + payload


def _send(proc: subprocess.Popen, obj: dict) -> None:
    data = json.dumps(obj, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    assert proc.stdin is not None
    proc.stdin.write(_frame(data))
    proc.stdin.flush()


def _read(proc: subprocess.Popen, timeout_s: float = 30.0) -> dict | None:
    assert proc.stdout is not None
    start = time.time()
    header = b""

    # read until header delimiter
    while b"\r\n\r\n" not in header:
        if time.time() - start > timeout_s:
            raise TimeoutError("timeout waiting for MCP response header")
        b = proc.stdout.read(1)
        if not b:
            return None
        header += b

    head, rest = header.split(b"\r\n\r\n", 1)
    m = re.search(br"Content-Length:\s*(\d+)", head, re.IGNORECASE)
    if not m:
        raise RuntimeError(f"Missing Content-Length header. header={head!r}")

    n = int(m.group(1))
    body = rest
    while len(body) < n:
        if time.time() - start > timeout_s:
            raise TimeoutError("timeout waiting for MCP response body")
        chunk = proc.stdout.read(n - len(body))
        if not chunk:
            break
        body += chunk

    return json.loads(body.decode("utf-8"))


def _read_until_id(proc: subprocess.Popen, want_id: int, timeout_s: float) -> dict:
    """Read messages until we get a JSON-RPC response with a matching id.

    Sidecar may emit progress via `notifications/message` when running `unreal.batch`.
    """
    start = time.time()
    while True:
        remaining = max(0.1, timeout_s - (time.time() - start))
        if remaining <= 0:
            raise TimeoutError(f"timeout waiting for JSON-RPC id={want_id}")

        msg = _read(proc, timeout_s=remaining)
        if msg is None:
            raise RuntimeError("stdio closed while waiting for response")

        # Batch progress notifications (no id)
        if isinstance(msg, dict) and msg.get("id") == want_id:
            return msg

        method = msg.get("method") if isinstance(msg, dict) else None
        if method and method.startswith("notifications/"):
            # Print progress messages to help debugging.
            params = msg.get("params", {})
            print(json.dumps({"notification": method, "params": params}, ensure_ascii=False))
            continue

        # Unexpected message; keep going but show it.
        print(json.dumps({"unexpected": msg}, ensure_ascii=False))


def main() -> int:

    ap = argparse.ArgumentParser()
    ap.add_argument("--sidecar", default=r"F:\MyLife_Project\UGIT\UEMCP\Sidecar\publish\win-x64\UnrealMCP.Sidecar.exe")
    ap.add_argument("--ue-host", default="127.0.0.1")
    ap.add_argument("--ue-port", type=int, default=55557)
    ap.add_argument("--timeout-ms", type=int, default=10000)
    ap.add_argument("--folder", default="/Game/UnrealMCP/Blueprints/")
    ap.add_argument("--name", default="BP_MCP_RotatingMesh")
    ap.add_argument("--yaw-deg-per-sec", type=float, default=90.0)
    args = ap.parse_args()

    # Use a unique name to avoid "already exists" failures.
    suffix = datetime.now().strftime("%Y%m%d_%H%M%S")
    bp_name = f"{args.name}_{suffix}"

    env = os.environ.copy()
    env.setdefault("UNREAL_MCP_LOG_JSON", "1")

    proc = subprocess.Popen(
        [
            args.sidecar,
            "--ue-host",
            args.ue_host,
            "--ue-port",
            str(args.ue_port),
            "--timeout-ms",
            str(args.timeout_ms),
        ],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env,
    )

    try:
        _send(
            proc,
            {
                "jsonrpc": "2.0",
                "id": 1,
                "method": "initialize",
                "params": {
                    "protocolVersion": "2024-11-05",
                    "capabilities": {},
                    "clientInfo": {"name": "auto-mcp", "version": "0"},
                },
            },
        )
        init_resp = _read_until_id(proc, want_id=1, timeout_s=10.0)
        print("initialize response:")
        print(json.dumps(init_resp, ensure_ascii=False, indent=2))

        batch = {
            "calls": [
                {
                    "name": "create_blueprint",
                    "arguments": {
                        "name": bp_name,
                        "parent_class": "Actor",
                        "folder_path": args.folder,
                    },
                },
                {
                    "name": "add_component_to_blueprint",
                    "arguments": {
                        "blueprint_name": bp_name,
                        "component_type": "StaticMeshComponent",
                        "component_name": "Mesh",
                    },
                },
                {
                    "name": "set_static_mesh_properties",
                    "arguments": {
                        "blueprint_name": bp_name,
                        "component_name": "Mesh",
                        "static_mesh": "/Engine/BasicShapes/Cube.Cube",
                    },
                },

                {
                    "name": "add_component_to_blueprint",
                    "arguments": {
                        "blueprint_name": bp_name,
                        "component_type": "RotatingMovementComponent",
                        "component_name": "RotatingMovement",
                    },
                },
                {
                    "name": "set_component_property",
                    "arguments": {
                        "blueprint_name": bp_name,
                        "component_name": "RotatingMovement",
                        "property_name": "RotationRate",
                        "property_value": [0.0, float(args.yaw_deg_per_sec), 0.0],
                    },
                },
                {"name": "compile_blueprint", "arguments": {"blueprint_name": bp_name}},
                {
                    "name": "spawn_blueprint_actor",
                    "arguments": {
                        "blueprint_name": bp_name,
                        "actor_name": f"{bp_name}_Instance",
                        "location": [0.0, 0.0, 100.0],
                        "rotation": [0.0, 0.0, 0.0],
                        "scale": [1.0, 1.0, 1.0],
                    },
                },
            ],
            "stop_on_error": True,
            "notify": True,
            "use_ue_batch": True,
        }

        _send(
            proc,
            {
                "jsonrpc": "2.0",
                "id": 2,
                "method": "tools/call",
                "params": {"name": "unreal.batch", "arguments": batch},
            },
        )
        batch_resp = _read_until_id(proc, want_id=2, timeout_s=120.0)
        print("\nbatch response:")
        print(json.dumps(batch_resp, ensure_ascii=False, indent=2))

        print(f"\nCreated Blueprint: {bp_name}")
        return 0

    finally:
        try:
            proc.terminate()
        except Exception:
            pass

        try:
            if proc.stderr is not None:
                err = proc.stderr.read().decode("utf-8", errors="ignore").strip()
                if err:
                    print("\n--- sidecar stderr ---")
                    print(err)
        except Exception:
            pass


if __name__ == "__main__":
    raise SystemExit(main())
