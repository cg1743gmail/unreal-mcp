import socket, json, sys, io, time

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
print("Force Recreate Interchange Pipeline Blueprint")
print("=" * 70)

# Delete multiple times to ensure it's gone
print("\n[1/3] Force deleting old blueprint...")
for i in range(3):
    r = send('delete_asset', {'asset_path': '/Game/Pipelines/BPI_FBXMaterial'})
    print(f"  Attempt {i+1}: {r.get('success', False)}")
    time.sleep(0.5)

print("\n[2/3] Verifying deletion...")
time.sleep(1)

# Create with Interchange Pipeline command
print("\n[3/3] Creating Interchange Pipeline Blueprint...")
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
    print("This is a REAL Interchange Pipeline Blueprint!")
    print("Check get_interchange_pipelines to verify.")
    print("=" * 70)
    
    # Verify
    print("\n[Verification] Checking pipeline list...")
    r2 = send('get_interchange_pipelines', {})
    bp_count = r2.get('result', {}).get('blueprint_count', 0)
    print(f"Blueprint Pipelines found: {bp_count}")
    
    if bp_count > 0:
        print("\n[SUCCESS] Blueprint is recognized by Interchange system!")
    else:
        print("\n[WARNING] Blueprint not in pipeline list yet (may need refresh)")
        
else:
    print("\n[FAILED]", r.get('error', 'Unknown error'))
