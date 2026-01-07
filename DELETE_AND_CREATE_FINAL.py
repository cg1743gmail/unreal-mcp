import socket, json, sys, io

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

def send(cmd, params):
    s = socket.socket()
    s.settimeout(30)
    s.connect(('127.0.0.1', 55557))
    msg = json.dumps({'type': cmd, 'params': params})
    s.sendall(msg.encode())
    
    data = b''
    while True:
        chunk = s.recv(8192)
        if not chunk:
            break
        data += chunk
        try:
            result = json.loads(data)
            break
        except:
            continue
    
    s.close()
    return result

print("=" * 70)
print("Final: Delete and Create Correct Pipeline")
print("=" * 70)

# Use execute_python to force delete
print("\n[1/2] Force deleting via Python...")

delete_code = """
import unreal

old_path = '/Game/Pipelines/BPI_FBXMaterial'
if unreal.EditorAssetLibrary.does_asset_exist(old_path):
    # Force delete with confirmation
    unreal.EditorAssetLibrary.delete_directory(old_path)
    unreal.EditorAssetLibrary.delete_asset(old_path)
    print(f'Deleted: {old_path}')
else:
    print(f'Not found: {old_path}')
"""

r = send('execute_python_script', {'script': delete_code})
print(f"  Result: {r}")

# Now create with correct command
print("\n[2/2] Creating Interchange Pipeline Blueprint...")

r = send('create_interchange_pipeline_blueprint', {
    'name': 'BPI_FBXMaterial',
    'package_path': '/Game/Pipelines',
    'parent_class': 'FBXMaterialPipeline'
})

print(f"\nResponse:")
print(json.dumps(r, indent=2))

if r.get('success'):
    print("\n" + "=" * 70)
    print("[SUCCESS] Created Interchange Pipeline Blueprint!")
    print("=" * 70)
else:
    print("\n[INFO] If still fails, manually delete in Content Browser:")
    print("  1. Right-click /Game/Pipelines/BPI_FBXMaterial")
    print("  2. Delete")
    print("  3. Run this script again")
