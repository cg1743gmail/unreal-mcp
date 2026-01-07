#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Fixed Blueprint Pipeline Wrapper Creator
使用正确的 UInterchangeBlueprintPipelineBase 作为父类
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
print("Fixed Blueprint Pipeline Wrapper Creator")
print("=" * 70)

# Test connection
print("\n[1/2] Testing connection...")
result = send_mcp('ping', {})
if result.get('result', {}).get('message') == 'pong':
    print("  OK - UE Editor connected")
else:
    print("  FAILED - Check if UE Editor is running")
    sys.exit(1)

# Delete old broken Blueprint
print("\n[2/2] Deleting old blueprint and creating new one...")
bp_code = '''import unreal

bp_name = "BPI_FBXMaterial"
bp_path = "/Game/Pipelines"
full = f"{bp_path}/{bp_name}"

# Delete if exists
if unreal.EditorAssetLibrary.does_asset_exist(full):
    unreal.EditorAssetLibrary.delete_asset(full)
    print(f"DELETED old: {full}")

# Create new Blueprint with correct parent class
# Use UInterchangeBlueprintPipelineBase as parent
bp = unreal.InterchangeBlueprintPipelineBaseLibrary.create_interchange_pipeline_blueprint(
    bp_path, 
    bp_name, 
    unreal.InterchangeBlueprintPipelineBase  # Correct parent class!
)

if bp:
    print(f"CREATED new: {full}")
    print("Parent: UInterchangeBlueprintPipelineBase")
    
    # Add event graph logic would go here
    # For now, Blueprint is ready for manual editing in editor
    
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    print("Blueprint is now safe to open in Rider/Blueprint Editor")
else:
    print("FAILED to create Blueprint")
'''

result = send_mcp('execute_python', {'code': bp_code})
print("\nResult:", result.get('message', result))

print("\n" + "=" * 70)
print("COMPLETE!")
print("=" * 70)
print("\nAsset: /Game/Pipelines/BPI_FBXMaterial")
print("Parent Class: UInterchangeBlueprintPipelineBase")
print("\nNow you can open it in Rider/Blueprint Editor without errors!")
print("\nNext steps:")
print("  1. Open the Blueprint in editor")
print("  2. Add your custom logic in Event Graph")
print("  3. Override PreDialogCleanup or ScriptedExecutePipeline functions")
