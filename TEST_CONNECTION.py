import socket, json
try:
    s = socket.socket()
    s.connect(('127.0.0.1', 55557))
    s.sendall(b'{"type":"ping","params":{}}')
    data = s.recv(1024)
    result = json.loads(data)
    s.close()
    if result.get('result', {}).get('message') == 'pong':
        print("✓ MCP is ready! You can now run: python create_pipeline_complete.py")
    else:
        print("✗ MCP responded but unexpected result")
except Exception as e:
    print(f"✗ Cannot connect to MCP: {e}")
    print("  Make sure UE Editor is running!")
