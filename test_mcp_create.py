import json, socket

sock = socket.socket()
sock.settimeout(30)
sock.connect(('127.0.0.1', 55557))

# Test ping
msg = json.dumps({'type': 'ping', 'params': {}})
sock.sendall(msg.encode())
data = sock.recv(8192)
print("Ping:", json.loads(data))
sock.close()

# Create Blueprint Pipeline
sock = socket.socket()
sock.settimeout(30)
sock.connect(('127.0.0.1', 55557))
msg = json.dumps({
    'type': 'create_interchange_pipeline_blueprint',
    'params': {
        'name': 'BPI_FBXMaterial',
        'package_path': '/Game/Pipelines',
        'parent_class': 'InterchangeGenericMaterialPipeline'
    }
})
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

print("\nResult:")
print(json.dumps(result, indent=2))
