#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Create Blueprint Pipeline - Use Correct MCP Command
使用正确的 MCP 命令创建 Blueprint Pipeline
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
print("Create Blueprint Pipeline Wrapper (Correct Method)")
print("=" * 70)

# Test connection
print("\n[1/3] Testing connection...")
result = send_mcp('ping', {})
if result.get('result', {}).get('message') == 'pong':
    print("  OK - UE Editor connected")
else:
    print("  FAILED - Is UE Editor running?")
    print("  FAILED - Or Sidecar needs restart after recompile")
    sys.exit(1)

# Delete old broken BP
print("\n[2/3] Deleting old broken Blueprint...")
delete_code = '''import unreal
old_path = "/Game/Pipelines/BPI_FBXMaterial"
if unreal.EditorAssetLibrary.does_asset_exist(old_path):
    unreal.EditorAssetLibrary.delete_asset(old_path)
    print(f"DELETED: {old_path}")
else:
    print(f"NOT FOUND: {old_path}")
'''

# Try to delete via TCP send (UE Plugin might have Python execution)
# Since execute_python doesn't exist in Sidecar, we'll skip this step
print("  Skipped - Manual deletion recommended")

# Create Pipeline Blueprint using MCP command
print("\n[3/3] Creating Blueprint Pipeline (after Sidecar restart)...")
print("\nIMPORTANT: You need to RESTART Sidecar first!")
print("Steps:")
print("  1. Stop current Sidecar process")
print("  2. Restart Sidecar with updated commands")
print("  3. Run this script again")
print("\nAfter restart, the command will be:")
print("  create_interchange_pipeline_blueprint")
print("  package_path: /Game/Pipelines")
print("  blueprint_name: BPI_FBXMaterial")
print("  parent_class: InterchangeBlueprintPipelineBase")

# Try the command anyway (will fail if Sidecar not restarted)
result = send_mcp('create_interchange_pipeline_blueprint', {
    'package_path': '/Game/Pipelines',
    'blueprint_name': 'BPI_FBXMaterial',
    'parent_class': 'InterchangeBlueprintPipelineBase'
})

if result.get('success') or result.get('status') == 'success':
    print("\n  SUCCESS! Blueprint created")
    print(f"  Asset: {result.get('result', {}).get('asset_path', '/Game/Pipelines/BPI_FBXMaterial')}")
elif 'Unknown command' in str(result.get('error', '')):
    print("\n  ERROR: Command not recognized")
    print("  Please RESTART Sidecar and try again!")
else:
    print(f"\n  ERROR: {result}")

print("\n" + "=" * 70)
