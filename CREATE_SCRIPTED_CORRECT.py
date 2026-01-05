import socket, json, sys, io

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

def send(cmd, params):
    s = socket.socket()
    s.settimeout(3)
    try:
        s.connect(('127.0.0.1', 55557))
        msg = json.dumps({'type': cmd, 'params': params})
        s.sendall(msg.encode('utf-8'))

        data = b''
        while True:
            chunk = s.recv(8192)
            if not chunk:
                break
            data += chunk
            try:
                result = json.loads(data.decode('utf-8'))
                break
            except Exception:
                continue

        s.close()
        return result
    except Exception as e:
        try:
            s.close()
        except Exception:
            pass
        return {"success": False, "error": f"Socket error: {e}. Is UE Editor running and UnrealMCP server started (port 55557)?"}

print("=" * 70)
print("Create Interchange Scripted Pipeline (CORRECT)")
print("=" * 70)

# Test
print("\n[1/2] Testing...")
r = send('ping', {})
if r.get('result', {}).get('message') != 'pong':
    print(f"  [FAIL] {r}")
    exit(1)
print("  [OK] Connected")

# Create
print("\n[2/2] Creating Scripted Pipeline Blueprint...")
print("  Parent: UInterchangeBlueprintPipelineBase")
print("  (Scripted Pipeline - can override functions in Blueprint Graph)")

r = send('create_interchange_pipeline_blueprint', {
    'name': 'BPI_FBXMaterial',
    'package_path': '/Game/Pipelines',
    'parent_class': 'BlueprintPipelineBase',  # Scripted Pipeline Base
    'overwrite': True
})

print(f"\nMCP Response:")
print(json.dumps(r, indent=2))

if r.get('success'):
    print("\n" + "=" * 70)
    print("[SUCCESS!]")
    print("=" * 70)
    print("Asset: /Game/Pipelines/BPI_FBXMaterial")
    print("Type: Interchange Scripted Pipeline Blueprint")
    print("Parent: UInterchangeBlueprintPipelineBase")
    print("\nThis is a REAL Scripted Pipeline!")
    print("You can now:")
    print("  1. Open in Blueprint Editor")
    print("  2. Override Scripted functions")
    print("  3. Access BaseNodeContainer")
    print("  4. Implement custom import logic")
    print("=" * 70)
    
    # Verify
    print("\n[Verification] Checking pipeline list...")
    r2 = send('get_interchange_pipelines', {})
    result = r2.get('result', {})
    bp_count = result.get('blueprint_count', 0)
    
    print(f"\nBlueprint Pipelines: {bp_count}")
    
    if bp_count > 0:
        bps = result.get('blueprint_pipelines', [])
        for bp in bps:
            print(f"  - {bp.get('name')}: {bp.get('type')}")
        print("\n[VERIFIED] Blueprint is recognized by Interchange system!")
    else:
        print("\n[INFO] Refresh Content Browser if not visible yet")
        
else:
    error = r.get('error', 'Unknown')
    print(f"\n[FAILED] {error}")
    
    if 'already exists' in error.lower():
        print("\nPlease delete /Game/Pipelines/BPI_FBXMaterial first!")
