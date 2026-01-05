import json, socket

def send_mcp(cmd, params):
    sock = socket.socket()
    sock.settimeout(30)
    sock.connect(('127.0.0.1', 55557))
    msg = json.dumps({'type': cmd, 'params': params})
    sock.sendall(msg.encode())
    data = b''
    while True:
        chunk = sock.recv(8192)
        if not chunk:
            break
        data += chunk
        try:
            result = json.loads(data)
            break
        except:
            continue
    sock.close()
    return result

print("="*70)
print("通过 MCP create_blueprint 命令创建 Pipeline")
print("="*70)

# Test
print("\n[1/2] 测试连接...")
r = send_mcp('ping', {})
if r.get('result', {}).get('message') == 'pong':
    print("  ✓ 连接成功")
else:
    print("  ✗ 失败")
    exit(1)

# Create Blueprint
print("\n[2/2] 创建 Blueprint...")
r = send_mcp('create_blueprint', {
    'name': 'BPI_FBXMaterial',
    'package_path': '/Game/Pipelines',
    'parent_class': 'InterchangeGenericMaterialPipeline'
})

print("\n结果:")
print(json.dumps(r, indent=2, ensure_ascii=False))

if r.get('success') or r.get('status') == 'success':
    print("\n✓ 成功创建!")
    print(f"Asset: {r.get('result', {}).get('asset_path', 'N/A')}")
else:
    print(f"\n✗ 失败: {r.get('error', r.get('message', 'Unknown'))}")
