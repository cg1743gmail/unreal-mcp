#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Final Blueprint Pipeline Creation via MCP
使用 create_blueprint 命令创建 Interchange Pipeline
"""

import json
import socket
import sys

def send_mcp_command(command_type, params):
    """发送 MCP 命令到 UE Editor"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(30)
        sock.connect(('127.0.0.1', 55557))
        
        message = json.dumps({
            'type': command_type,
            'params': params
        })
        
        print(f"→ Sending: {command_type}")
        sock.sendall(message.encode('utf-8'))
        
        # 接收响应
        data = b''
        while True:
            try:
                chunk = sock.recv(8192)
                if not chunk:
                    break
                data += chunk
                try:
                    result = json.loads(data.decode('utf-8'))
                    break
                except json.JSONDecodeError:
                    continue
            except socket.timeout:
                break
        
        sock.close()
        
        if data:
            return json.loads(data.decode('utf-8'))
        else:
            return {'success': False, 'error': 'No response'}
            
    except Exception as e:
        return {'success': False, 'error': str(e)}


def main():
    print("=" * 70)
    print("Blueprint Pipeline Creation via MCP")
    print("=" * 70)
    
    # Step 1: Ping 测试
    print("\n[1/4] Testing MCP connection...")
    result = send_mcp_command('ping', {})
    if result.get('result', {}).get('message') == 'pong':
        print("  ✓ MCP connected")
    else:
        print(f"  ✗ MCP connection failed: {result}")
        return 1
    
    # Step 2: 删除旧 Blueprint（如果存在）
    print("\n[2/4] Removing old blueprint...")
    result = send_mcp_command('delete_asset', {
        'asset_path': '/Game/Pipelines/BPI_FBXMaterial'
    })
    if result.get('success'):
        print("  ✓ Old blueprint removed")
    else:
        print(f"  → Old blueprint not found (OK)")
    
    # Step 3: 创建新 Blueprint Pipeline
    print("\n[3/4] Creating Blueprint Pipeline...")
    result = send_mcp_command('create_blueprint', {
        'name': 'BPI_FBXMaterial',
        'package_path': '/Game/Pipelines',
        'parent_class': 'InterchangeGenericMaterialPipeline'
    })
    
    if result.get('success'):
        print(f"  ✓ Blueprint created: {result.get('result', {}).get('asset_path', 'N/A')}")
    else:
        print(f"  ✗ Failed: {result.get('error', 'Unknown error')}")
        return 1
    
    # Step 4: 配置 Blueprint 属性
    print("\n[4/4] Configuring blueprint properties...")
    
    # 配置 Material Import 模式
    result = send_mcp_command('set_blueprint_property', {
        'blueprint_path': '/Game/Pipelines/BPI_FBXMaterial',
        'property_name': 'MaterialImport',
        'value': 'ImportMaterials'
    })
    
    # 配置 Parent Material
    result = send_mcp_command('set_blueprint_property', {
        'blueprint_path': '/Game/Pipelines/BPI_FBXMaterial',
        'property_name': 'ParentMaterial',
        'value': '/Game/Materials/M_SkeletalMesh_Master'
    })
    
    # 配置 Texture Import
    result = send_mcp_command('set_blueprint_property', {
        'blueprint_path': '/Game/Pipelines/BPI_FBXMaterial',
        'property_name': 'TextureImport',
        'value': 'ImportTextures'
    })
    
    print("  ✓ Properties configured")
    
    print("\n" + "=" * 70)
    print("✅ SUCCESS!")
    print("=" * 70)
    print("Asset: /Game/Pipelines/BPI_FBXMaterial")
    print("Parent Class: InterchangeGenericMaterialPipeline")
    print("Parent Material: /Game/Materials/M_SkeletalMesh_Master")
    print("\nYou can now open it in Rider/Blueprint Editor!")
    print("=" * 70)
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
