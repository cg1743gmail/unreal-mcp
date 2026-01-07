"""Test MCP Pipeline Creation"""
import json
import socket
import sys

HOST = "127.0.0.1"
PORT = 55557

def send_command(cmd_type, params):
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(15.0)
        sock.connect((HOST, PORT))
        
        msg = json.dumps({"type": cmd_type, "params": params})
        sock.sendall(msg.encode('utf-8'))
        
        chunks = []
        while True:
            try:
                chunk = sock.recv(8192)
                if not chunk:
                    break
                chunks.append(chunk)
                try:
                    result = json.loads(b''.join(chunks).decode('utf-8'))
                    sock.close()
                    return result
                except:
                    continue
            except:
                break
        
        sock.close()
        return json.loads(b''.join(chunks).decode('utf-8')) if chunks else {"success": False}
    except Exception as e:
        print(f"Error: {e}")
        return {"success": False, "message": str(e)}

print("Testing MCP Connection...")
result = send_command("ping", {})
print(f"Ping result: {result}")

if result.get("message") == "pong":
    print("\n✓ MCP Connection OK!")
    print("\nStep 1: Creating Master Material...")
    
    python_code = '''import unreal
material_name = "M_SkeletalMesh_Master"
package_path = "/Game/Materials"
full_path = f"{package_path}/{material_name}"
print(f"Creating material at: {full_path}")
if unreal.EditorAssetLibrary.does_asset_exist(full_path):
    print("Material already exists")
else:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material_factory = unreal.MaterialFactoryNew()
    material = asset_tools.create_asset(material_name, package_path, unreal.Material, material_factory)
    print(f"Created: {full_path}")
'''
    
    result = send_command("execute_python", {"code": python_code})
    print(f"Result: {result}")
else:
    print("✗ MCP Connection Failed!")
    sys.exit(1)
