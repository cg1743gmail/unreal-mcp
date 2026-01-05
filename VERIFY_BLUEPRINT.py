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
print("Verify Blueprint Type")
print("=" * 70)

# Check pipeline list
print("\n[Check 1] Interchange Pipeline List...")
r = send('get_interchange_pipelines', {})
result = r.get('result', {})

bp_count = result.get('blueprint_count', 0)
native_count = len(result.get('native_pipelines', []))

print(f"  Blueprint Pipelines: {bp_count}")
print(f"  Native Pipelines: {native_count}")

if bp_count > 0:
    print("\n  Blueprint Pipelines:")
    for bp in result.get('blueprint_pipelines', []):
        print(f"    - {bp.get('name')}: {bp.get('class', 'Unknown')}")
    print("\n  [SUCCESS] Blueprint IS recognized by Interchange!")
else:
    print("\n  [WARNING] Blueprint not in list")
    print("  This might mean:")
    print("    1. Parent class is still wrong (not InterchangeBlueprintPipelineBase)")
    print("    2. Asset Registry needs refresh")
    print("    3. Need to restart UE Editor")

print("\n[Check 2] Native Pipelines:")
for native in result.get('native_pipelines', []):
    print(f"  - {native.get('name')}: {native.get('description', 'N/A')}")

print("\n" + "=" * 70)
print("Recommendation:")
print("=" * 70)
print("1. Open /Game/Pipelines/BPI_FBXMaterial in Blueprint Editor")
print("2. Check 'Class Settings' -> Parent Class")
print("3. If NOT 'InterchangeBlueprintPipelineBase', need to restart UE Editor")
print("=" * 70)
