import json
import socket
import time

def send_cmd(cmd, params):
    sock = socket.socket()
    sock.settimeout(15)
    sock.connect(('127.0.0.1', 55557))
    msg = json.dumps({'type': cmd, 'params': params})
    print(f"Sending: {cmd}")
    sock.sendall(msg.encode())
    
    data = b''
    while True:
        try:
            chunk = sock.recv(8192)
            if not chunk:
                break
            data += chunk
            try:
                result = json.loads(data)
                break
            except:
                continue
        except socket.timeout:
            break
    
    sock.close()
    return json.loads(data) if data else {}

# Test 1: Get info
print("Test 1: get_interchange_info")
result = send_cmd('get_interchange_info', {})
print(f"Result: {result}")
print()

# Test 2: Create Pipeline
print("Test 2: create_interchange_pipeline_blueprint")
result = send_cmd('create_interchange_pipeline_blueprint', {
    'name': 'BP_SkeletalMesh_TextureSuffix_Pipeline',
    'package_path': '/Game/Interchange/Pipelines/',
    'parent_class': 'FBXMaterialPipeline'
})
print(f"Result: {json.dumps(result, indent=2)}")
