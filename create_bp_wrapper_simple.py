#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Simple Blueprint Pipeline Wrapper Creator
"""
import json
import socket
import sys

def send_mcp(cmd_type, params):
    """Send MCP command and return result"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(30)
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
print("Blueprint Pipeline Wrapper Creator")
print("=" * 70)

# Test connection
print("\n[1/3] Testing connection...")
result = send_mcp('ping', {})
if result.get('result', {}).get('message') == 'pong':
    print("  OK - UE Editor connected")
else:
    print("  FAILED - Check if UE Editor is running")
    sys.exit(1)

# Create Master Material
print("\n[2/3] Creating Master Material...")
mat_code = '''import unreal
mat_name = "M_SkeletalMesh_Master"
mat_path = "/Game/Materials"
full = f"{mat_path}/{mat_name}"
if unreal.EditorAssetLibrary.does_asset_exist(full):
    print(f"EXISTS: {full}")
else:
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.MaterialFactoryNew()
    mat = tools.create_asset(mat_name, mat_path, unreal.Material, factory)
    bc = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -600, -300)
    bc.set_editor_property("parameter_name", "BaseColorTexture")
    unreal.MaterialEditingLibrary.connect_material_property(bc, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    pk = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -800, 100)
    pk.set_editor_property("parameter_name", "PackedTexture")
    mask_n = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -550, 100)
    mask_n.set_editor_property("r", True)
    mask_n.set_editor_property("g", True)
    unreal.MaterialEditingLibrary.connect_material_expressions(pk, "RGB", mask_n, "")
    mad = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionMultiplyAdd, -400, 100)
    mad.set_editor_property("const_a", 2.0)
    mad.set_editor_property("const_b", -1.0)
    unreal.MaterialEditingLibrary.connect_material_expressions(mask_n, "", mad, "")
    unreal.MaterialEditingLibrary.connect_material_property(mad, "", unreal.MaterialProperty.MP_NORMAL)
    mask_r = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -550, 300)
    mask_r.set_editor_property("b", True)
    unreal.MaterialEditingLibrary.connect_material_expressions(pk, "RGB", mask_r, "")
    unreal.MaterialEditingLibrary.connect_material_property(mask_r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mask_ao = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -550, 500)
    mask_ao.set_editor_property("a", True)
    unreal.MaterialEditingLibrary.connect_material_expressions(pk, "RGBA", mask_ao, "")
    unreal.MaterialEditingLibrary.connect_material_property(mask_ao, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.MaterialEditingLibrary.layout_material_expressions(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    print(f"CREATED: {full}")
'''

result = send_mcp('execute_python', {'code': mat_code})
print("  Material:", result.get('message', 'Done'))

# Create Blueprint Pipeline
print("\n[3/3] Creating Blueprint Pipeline...")
bp_code = '''import unreal
bp_name = "BPI_FBXMaterial"
bp_path = "/Game/Pipelines"
full = f"{bp_path}/{bp_name}"
if unreal.EditorAssetLibrary.does_asset_exist(full):
    print(f"EXISTS: {full}")
else:
    bp = unreal.InterchangeBlueprintPipelineBaseLibrary.create_interchange_pipeline_blueprint(bp_path, bp_name, unreal.InterchangeGenericMaterialPipeline)
    if bp:
        cdo = unreal.get_default_object(bp.generated_class())
        cdo.set_editor_property("material_import", unreal.MaterialImportType.IMPORT_MATERIALS)
        master = unreal.load_asset("/Game/Materials/M_SkeletalMesh_Master")
        if master:
            cdo.set_editor_property("parent_material", master)
        cdo.set_editor_property("texture_import", unreal.MaterialTextureImportType.IMPORT_TEXTURES)
        unreal.EditorAssetLibrary.save_loaded_asset(bp)
        print(f"CREATED: {full}")
    else:
        print("FAILED to create Blueprint")
'''

result = send_mcp('execute_python', {'code': bp_code})
print("  Blueprint:", result.get('message', 'Done'))

print("\n" + "=" * 70)
print("COMPLETE!")
print("=" * 70)
print("\nAssets:")
print("  - /Game/Materials/M_SkeletalMesh_Master")
print("  - /Game/Pipelines/BPI_FBXMaterial")
print("\nOpen Content Browser to edit properties")
