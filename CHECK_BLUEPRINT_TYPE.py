import socket, json
import sys
import io

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

print("Checking Blueprint Type...")

# Get Interchange Pipelines
r = send('get_interchange_pipelines', {})

print("\nMCP Response:")
print(json.dumps(r, indent=2))

print("\n" + "=" * 70)
print("Analysis: Is BPI_FBXMaterial in the list?")
print("=" * 70)
