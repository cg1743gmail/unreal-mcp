import socket, json
import sys, io

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
print("Create Correct Interchange Pipeline Blueprint")
print("=" * 70)

# Step 1: Ping
print("\n[1/3] Testing...")
r = send('ping', {})
if r.get('result', {}).get('message') != 'pong':
    print(f"  [FAIL] {r}")
    exit(1)
print("  [OK] Connected")

# Step 2: Delete old
print("\n[2/3] Cleaning...")
send('delete_asset', {'asset_path': '/Game/Pipelines/BPI_FBXMaterial'})
print("  [OK] Cleaned")

# Step 3: Create using create_interchange_pipeline_blueprint command
print("\n[3/3] Creating Interchange Pipeline Blueprint...")
print("  Using parent_class: FBXMaterialPipeline (Custom C++ Pipeline)")

r = send('create_interchange_pipeline_blueprint', {
    'name': 'BPI_FBXMaterial',
    'package_path': '/Game/Pipelines',
    'parent_class': 'FBXMaterialPipeline'
})

print(f"\nMCP Response:")
print(json.dumps(r, indent=2))

if r.get('success'):
    print("\n" + "=" * 70)
    print("[SUCCESS!]")
    print("=" * 70)
    print("Asset: /Game/Pipelines/BPI_FBXMaterial")
    print("Type: Interchange Pipeline Blueprint")
    print("Parent: UUnrealMCPFBXMaterialPipeline (Custom C++)")
    print("\nThis Blueprint:")
    print("- Can override Scripted functions")
    print("- Will be recognized by Interchange system")
    print("- Inherits FBX material logic from C++")
    print("=" * 70)
else:
    print("\n" + "=" * 70)
    print("[FAILED] Check response above")
    print("=" * 70)
