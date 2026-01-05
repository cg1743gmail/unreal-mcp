#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
通过 MCP 执行 Blueprint 创建脚本
"""
import json
import socket
import sys

def send_mcp(cmd_type, params):
    """Send MCP command"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(3)
        sock.connect(('127.0.0.1', 55557))
        
        message = json.dumps({'type': cmd_type, 'params': params})
        sock.sendall(message.encode('utf-8'))
        
        response = b''
        while True:
            chunk = sock.recv(8192)
            if not chunk:
                break
            response += chunk
            try:
                result = json.loads(response.decode('utf-8'))
                break
            except:
                continue
        
        sock.close()
        return result
    except Exception as e:
        return {'success': False, 'error': str(e)}

print("=" * 70)
print("通过 MCP 创建 Blueprint Pipeline")
print("=" * 70)

# Test connection
print("\n[1/2] 测试连接...")
result = send_mcp('ping', {})
if result.get('result', {}).get('message') == 'pong':
    print("  ✓ MCP 连接成功")
else:
    print("  ✗ MCP 连接失败")
    sys.exit(1)

# Execute Python script via MCP
print("\n[2/2] 执行 Python 脚本...")

# 尝试方法 1: create_interchange_pipeline_blueprint 命令
print("\n方法 1: 使用 MCP 原生命令...")
result = send_mcp('create_interchange_pipeline_blueprint', {
    'name': 'BPI_FBXMaterial',
    'package_path': '/Game/Pipelines',
    'parent_class': 'BlueprintPipelineBase',
    'overwrite': True
})

if result.get('success') or result.get('status') == 'success':
    print("  ✓ 成功!")
    print(f"  Asset: {result.get('result', {}).get('asset_path', 'N/A')}")
elif 'Unknown command' in str(result.get('error', '')):
    print("  ✗ 命令不可用（需要重新编译 Plugin）")
    print("\n方法 2: 通过 TCP 直接发送 Python 代码...")
    
    # 直接将 Python 代码作为字符串发送
    python_script = """import unreal
old_path='/Game/Pipelines/BPI_FBXMaterial'
if unreal.EditorAssetLibrary.does_asset_exist(old_path):
    unreal.EditorAssetLibrary.delete_asset(old_path)
asset_tools=unreal.AssetToolsHelpers.get_asset_tools()
factory=unreal.BlueprintFactory()
factory.set_editor_property('parent_class',unreal.InterchangeGenericMaterialPipeline)
bp=asset_tools.create_asset('BPI_FBXMaterial','/Game/Pipelines',unreal.Blueprint,factory)
if bp:
    cdo=unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('material_import',unreal.MaterialImportType.IMPORT_MATERIALS)
    mat=unreal.load_asset('/Game/Materials/M_SkeletalMesh_Master')
    if mat:
        cdo.set_editor_property('parent_material',mat)
    cdo.set_editor_property('texture_import',unreal.MaterialTextureImportType.IMPORT_TEXTURES)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    print('SUCCESS:',bp.get_path_name())
else:
    print('FAILED')
"""
    
    # 注意：这里需要 UE Plugin 支持某种形式的 Python 执行命令
    # 但目前 MCP 协议中没有这个命令
    print("\n  ⚠ MCP 协议目前不支持直接执行 Python 脚本")
    print("\n  请手动执行:")
    print("  1. 打开 UE Editor: Tools > Execute Python Script")
    print("  2. 运行文件: create_bp_correct_parent.py")
    print(f"\n  或使用命令: python {sys.argv[0].replace('mcp_create_bp_final.py', 'create_bp_correct_parent.py')}")
else:
    print(f"  ✗ 错误: {result.get('error', result)}")

print("\n" + "=" * 70)
