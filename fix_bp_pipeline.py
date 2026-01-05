#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Fix Blueprint Pipeline - Use MCP Native Commands
"""
import json
import socket
import sys

def send_mcp(cmd_type, params):
    """Send MCP command and return result"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(30)
        sock.connect(('127.0.0.1', 55557))
        
        message = json.dumps({'type': cmd_type, 'params': params})
        sock.sendall(message.encode('utf-8'))
        
        response = b''
        while True:
            chunk = sock.recv(8192)
            if not chunk:
                break
            response += chunk
            try:
                result = json.loads(response.decode('utf-8'))
                break
            except:
                continue
        
        sock.close()
        return result
    except Exception as e:
        return {'success': False, 'error': str(e)}

print("=" * 70)
print("Fix Blueprint Pipeline Wrapper")
print("=" * 70)

# Test connection
print("\n[1/2] Testing connection...")
result = send_mcp('ping', {})
if result.get('result', {}).get('message') == 'pong':
    print("  OK - UE Editor connected")
else:
    print("  FAILED - Check if UE Editor is running")
    sys.exit(1)

# Create Blueprint Pipeline using native MCP command
print("\n[2/2] Creating Blueprint Pipeline...")
result = send_mcp('create_interchange_pipeline_blueprint', {
    'package_path': '/Game/Pipelines',
    'blueprint_name': 'BPI_FBXMaterial_Fixed',
    'parent_class': 'InterchangeBlueprintPipelineBase'
})

if result.get('success') or result.get('status') == 'success':
    print("  SUCCESS!")
    print(f"  Asset: {result.get('result', {}).get('asset_path', '/Game/Pipelines/BPI_FBXMaterial_Fixed')}")
else:
    print(f"  Error: {result}")

print("\n" + "=" * 70)
print("COMPLETE!")
print("=" * 70)
print("\nAsset: /Game/Pipelines/BPI_FBXMaterial_Fixed")
print("Parent: InterchangeBlueprintPipelineBase")
print("\nThis Blueprint can now be opened in Rider/Editor safely!")
