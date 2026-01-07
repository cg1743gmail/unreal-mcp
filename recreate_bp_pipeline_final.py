#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Recreate Blueprint Pipeline - Direct Socket Approach
直接通过 Socket 与 UE Plugin 通信（绕过 Sidecar）
"""
import json
import socket
import sys

def send_to_ue_direct(cmd_type, params):
    """直接发送命令到 UE Plugin (TCP 55557)"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(3)
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
print("Recreate Blueprint Pipeline (Direct Method)")
print("=" * 70)

# Test connection
print("\n[1/2] Testing connection...")
result = send_to_ue_direct('ping', {})
if result.get('result', {}).get('message') == 'pong':
    print("  OK - UE Plugin connected")
else:
    print("  FAILED - UE Plugin not responding")
    sys.exit(1)

# Create Pipeline Blueprint using native UE command
print("\n[2/2] Creating Blueprint Pipeline...")
result = send_to_ue_direct('create_interchange_pipeline_blueprint', {
    'package_path': '/Game/Pipelines',
    'name': 'BPI_FBXMaterial',
    'parent_class': 'BlueprintPipelineBase',
    'overwrite': True
})

print(f"\nFull Response: {json.dumps(result, indent=2)}")

if result.get('success') or result.get('status') == 'success':
    asset_path = result.get('result', {}).get('asset_path', '/Game/Pipelines/BPI_FBXMaterial')
    print(f"\n  SUCCESS!")
    print(f"  Asset: {asset_path}")
    print(f"\n  Now you can open it in Rider/Blueprint Editor!")
else:
    error = result.get('error', result.get('message', 'Unknown error'))
    print(f"\n  ERROR: {error}")
    
    if 'Unknown command' in str(error):
        print("\n  The command is not recognized by UE Plugin.")
        print("  This might be a version issue.")
        print("\n  Fallback: Use Content Browser manually:")
        print("    1. Right click in Content Browser")
        print("    2. Interchange -> Blueprint Pipeline")
        print("    3. Parent: InterchangeBlueprintPipelineBase")
        print("    4. Name: BPI_FBXMaterial")

print("\n" + "=" * 70)
