#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""MCP Auto Create Blueprint Pipeline"""

import json
import socket
import sys

def mcp(cmd, params):
    """Send MCP command"""
    sock = socket.socket()
    sock.settimeout(30)
    try:
        sock.connect(('127.0.0.1', 55557))
        sock.sendall(json.dumps({'type': cmd, 'params': params}).encode())
        
        data = b''
        while True:
            chunk = sock.recv(8192)
            if not chunk:
                break
            data += chunk
            try:
                return json.loads(data.decode('utf-8'))
            except:
                continue
    except Exception as e:
        return {'success': False, 'error': str(e)}
    finally:
        sock.close()
    return {'success': False, 'error': 'No response'}


print("=" * 70)
print("MCP Auto Create Blueprint Pipeline")
print("=" * 70)

# 1. Ping
print("\n[1/3] Ping...")
r = mcp('ping', {})
if r.get('result', {}).get('message') == 'pong':
    print("  ✓ Connected")
else:
    print(f"  ✗ Failed: {r}")
    sys.exit(1)

# 2. Delete old
print("\n[2/3] Delete old...")
mcp('delete_asset', {'asset_path': '/Game/Pipelines/BPI_FBXMaterial'})
print("  → Deleted")

# 3. Create Blueprint
print("\n[3/3] Create Blueprint...")
r = mcp('create_blueprint', {
    'name': 'BPI_FBXMaterial',
    'package_path': '/Game/Pipelines',
    'parent_class': 'InterchangeGenericMaterialPipeline'
})

if r.get('success') or r.get('status') == 'success':
    print(f"  ✓ Created: {r}")
    print("\n" + "=" * 70)
    print("✅ SUCCESS!")
    print("=" * 70)
    print("Asset: /Game/Pipelines/BPI_FBXMaterial")
    print("Parent: InterchangeGenericMaterialPipeline")
    sys.exit(0)
else:
    print(f"  ✗ Failed: {r}")
    sys.exit(1)
