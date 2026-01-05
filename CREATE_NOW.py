import socket, json, time
import sys
import io

# Fix Windows GBK encoding
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
print("Auto Create Blueprint Pipeline via MCP")
print("=" * 70)

# Step 1: Ping
print("\n[1/3] Testing connection...")
try:
    r = send('ping', {})
    if r.get('result', {}).get('message') == 'pong':
        print("  [OK] MCP Connected")
    else:
        print(f"  [FAIL] Unexpected response: {r}")
        exit(1)
except Exception as e:
    print(f"  [FAIL] Connection failed: {e}")
    exit(1)

# Step 2: Delete old (ignore errors)
print("\n[2/3] Cleaning up old blueprint...")
try:
    send('delete_asset', {'asset_path': '/Game/Pipelines/BPI_FBXMaterial'})
    print("  [OK] Deleted old blueprint")
except:
    print("  [OK] No old blueprint found")

# Step 3: Create new Blueprint
print("\n[3/3] Creating Blueprint Pipeline...")
try:
    r = send('create_blueprint', {
        'name': 'BPI_FBXMaterial',
        'package_path': '/Game/Pipelines',
        'parent_class': 'InterchangeGenericMaterialPipeline'
    })
    
    print(f"\nMCP Response:")
    print(json.dumps(r, indent=2))
    
    if r.get('success') or r.get('status') == 'success':
        print("\n" + "=" * 70)
        print("[SUCCESS!]")
        print("=" * 70)
        print("Asset Created: /Game/Pipelines/BPI_FBXMaterial")
        print("Parent Class: InterchangeGenericMaterialPipeline")
        print("\nNow you can:")
        print("1. Open it in Content Browser")
        print("2. Edit properties in Details panel")
        print("3. Set Parent Material to M_SkeletalMesh_Master")
        print("=" * 70)
    else:
        print("\n" + "=" * 70)
        print("[WARNING] Check MCP Response Above")
        print("=" * 70)
        
except Exception as e:
    print(f"\n[FAIL] Failed: {e}")
    exit(1)
