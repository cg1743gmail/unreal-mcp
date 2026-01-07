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
print("Create Scripted Pipeline Blueprint (CORRECT WAY)")
print("=" * 70)

# Test connection
print("\n[1/2] Testing...")
r = send('ping', {})
if r.get('result', {}).get('message') != 'pong':
    print(f"  [FAIL] {r}")
    exit(1)
print("  [OK] Connected")

# Create Scripted Pipeline
print("\n[2/2] Creating Scripted Pipeline Blueprint...")
print("  Parent: InterchangeBlueprintPipelineBase (Scripted Pipeline Base)")

r = send('create_interchange_pipeline_blueprint', {
    'name': 'BPI_FBXMaterial',
    'package_path': '/Game/Pipelines',
    'parent_class': 'PipelineBase'  # Will resolve to InterchangeBlueprintPipelineBase
})

print(f"\nMCP Response:")
print(json.dumps(r, indent=2))

if r.get('success'):
    print("\n" + "=" * 70)
    print("[SUCCESS!]")
    print("=" * 70)
    print("Asset: /Game/Pipelines/BPI_FBXMaterial")
    print("Type: Interchange Scripted Pipeline Blueprint")
    print("Parent: InterchangeBlueprintPipelineBase")
    print("\nThis Blueprint can:")
    print("- Override Scripted functions in Blueprint Graph")
    print("- Be recognized by Interchange system")
    print("- Access BaseNodeContainer and Factory Nodes")
    print("=" * 70)
    
    # Verify
    print("\n[Verification] Checking pipeline list...")
    r2 = send('get_interchange_pipelines', {})
    bp_count = r2.get('result', {}).get('blueprint_count', 0)
    print(f"Blueprint Pipelines found: {bp_count}")
    
    if bp_count > 0:
        bps = r2.get('result', {}).get('blueprint_pipelines', [])
        print("\nBlueprint Pipelines:")
        for bp in bps:
            print(f"  - {bp.get('name')}: {bp.get('path')}")
        print("\n[SUCCESS] Blueprint is recognized by Interchange!")
    else:
        print("\n[WARNING] Not in list yet (Content Browser may need refresh)")
        
else:
    error = r.get('error', 'Unknown error')
    print(f"\n[FAILED] {error}")
    
    if 'already exists' in error.lower():
        print("\n[ACTION REQUIRED]")
        print("Please delete the old blueprint in Content Browser:")
        print("  /Game/Pipelines/BPI_FBXMaterial")
        print("Then run this script again.")
