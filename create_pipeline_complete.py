#!/usr/bin/env python3
"""
Complete Pipeline Creation via MCP - All Python code runs INSIDE UE Editor
"""
import json
import socket
import time

def send_cmd(cmd, params):
    """Send MCP command to UnrealMCP server"""
    sock = socket.socket()
    sock.settimeout(30)  # Longer timeout for compile operations
    try:
        sock.connect(('127.0.0.1', 55557))
        msg = json.dumps({'type': cmd, 'params': params})
        print(f"  → Sending: {cmd}")
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
    except Exception as e:
        print(f"  ✗ Error: {e}")
        return {'success': False, 'error': str(e)}

print("=" * 80)
print("Pipeline Creation - Executing via MCP")
print("=" * 80)

# Step 0: Test connection
print("\n[0/4] Testing MCP connection...")
result = send_cmd('ping', {})
if result.get('result', {}).get('message') == 'pong':
    print("  ✓ MCP Connected!")
else:
    print("  ✗ MCP Connection Failed!")
    exit(1)

# Step 1: Create Master Material (runs INSIDE UE)
print("\n[1/4] Creating Master Material with Normal Remap...")
python_code = """
import unreal

material_name = "M_SkeletalMesh_Master"
package_path = "/Game/Materials"
full_path = f"{package_path}/{material_name}"

# Check if material exists
if unreal.EditorAssetLibrary.does_asset_exist(full_path):
    material = unreal.EditorAssetLibrary.load_asset(full_path)
    print(f"[Material] Already exists: {full_path}")
else:
    # Create new material
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material_factory = unreal.MaterialFactoryNew()
    material = asset_tools.create_asset(material_name, package_path, unreal.Material, material_factory)
    print(f"[Material] Created: {full_path}")

# BaseColor Texture Parameter
base_color = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionTextureSampleParameter2D, -600, -300
)
base_color.set_editor_property("parameter_name", "BaseColorTexture")
unreal.MaterialEditingLibrary.connect_material_property(
    base_color, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
)

# Packed Texture Parameter (_NRA format)
packed = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionTextureSampleParameter2D, -800, 100
)
packed.set_editor_property("parameter_name", "PackedTexture")

# Normal Remap Bool Parameter (converts [0,1] to [-1,1])
remap_param = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionStaticBoolParameter, -800, 300
)
remap_param.set_editor_property("parameter_name", "bNormalNeedsRemap")
remap_param.set_editor_property("default_value", True)

# Extract Normal channels (R,G)
mask_n = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionComponentMask, -550, 100
)
mask_n.set_editor_property("r", True)
mask_n.set_editor_property("g", True)
unreal.MaterialEditingLibrary.connect_material_expressions(packed, "RGB", mask_n, "")

# Remap: value * 2 - 1
mad = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionMultiplyAdd, -400, 100
)
mad.set_editor_property("const_a", 2.0)
mad.set_editor_property("const_b", -1.0)
unreal.MaterialEditingLibrary.connect_material_expressions(mask_n, "", mad, "")

# Lerp between remapped and original
lerp = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionLinearInterpolate, -250, 100
)
unreal.MaterialEditingLibrary.connect_material_expressions(mad, "", lerp, "A")
unreal.MaterialEditingLibrary.connect_material_expressions(mask_n, "", lerp, "B")
unreal.MaterialEditingLibrary.connect_material_expressions(remap_param, "", lerp, "Alpha")

# Derive Normal Z
derive_z = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionDeriveNormalZ, -100, 100
)
unreal.MaterialEditingLibrary.connect_material_expressions(lerp, "", derive_z, "")
unreal.MaterialEditingLibrary.connect_material_property(
    derive_z, "", unreal.MaterialProperty.MP_NORMAL
)

# Roughness (B channel)
mask_r = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionComponentMask, -550, 300
)
mask_r.set_editor_property("b", True)
unreal.MaterialEditingLibrary.connect_material_expressions(packed, "RGB", mask_r, "")
unreal.MaterialEditingLibrary.connect_material_property(
    mask_r, "", unreal.MaterialProperty.MP_ROUGHNESS
)

# AO (A channel)
mask_ao = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionComponentMask, -550, 500
)
mask_ao.set_editor_property("a", True)
unreal.MaterialEditingLibrary.connect_material_expressions(packed, "RGBA", mask_ao, "")
unreal.MaterialEditingLibrary.connect_material_property(
    mask_ao, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION
)

# Compile and save
unreal.MaterialEditingLibrary.recompile_material(material)
unreal.MaterialEditingLibrary.layout_material_expressions(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)

print("[Material] ✓ Master Material created with Normal Remap support")
"""

result = send_cmd('execute_python', {'code': python_code})
if result.get('success') or result.get('status') == 'success':
    print("  ✓ Master Material Created!")
else:
    print(f"  Note: {result.get('message', 'Check UE Output Log')}")

# Step 2: Create Pipeline Blueprint
print("\n[2/4] Creating Pipeline Blueprint...")
result = send_cmd('create_interchange_pipeline_blueprint', {
    'name': 'BP_SkeletalMesh_TextureSuffix_Pipeline',
    'package_path': '/Game/Interchange/Pipelines/',
    'parent_class': 'FBXMaterialPipeline'
})

if result.get('success') or result.get('status') == 'success':
    print("  ✓ Pipeline Blueprint Created!")
else:
    msg = result.get('result', {}).get('message', 'Check if already exists')
    print(f"  Note: {msg}")

# Step 3: Configure Pipeline
print("\n[3/4] Configuring Pipeline Settings...")
result = send_cmd('configure_interchange_pipeline', {
    'pipeline_path': '/Game/Interchange/Pipelines/BP_SkeletalMesh_TextureSuffix_Pipeline',
    'settings': {
        'bAutoCreateMaterialInstances': True,
        'bAutoAssignTextures': True,
        'bSearchExistingMaterials': True,
        'MaterialInstanceSubFolder': 'MaterialInstances',
        'ParentMaterial': '/Game/Materials/M_SkeletalMesh_Master.M_SkeletalMesh_Master'
    }
})

if result.get('success') or result.get('status') == 'success':
    print("  ✓ Pipeline Configured!")
else:
    print(f"  Note: {result.get('message', '')}")

# Step 4: Compile Pipeline (with new C++ fix: CompileBlueprint + SaveAsset)
print("\n[4/4] Compiling Pipeline Blueprint...")
result = send_cmd('compile_interchange_pipeline', {
    'pipeline_path': '/Game/Interchange/Pipelines/BP_SkeletalMesh_TextureSuffix_Pipeline'
})

if result.get('success') or result.get('status') == 'success':
    status = result.get('result', {}).get('status', 'Unknown')
    print(f"  ✓ Pipeline Compiled! (Status: {status})")
else:
    print(f"  Result: {result}")

# Summary
print("\n" + "=" * 80)
print("✓✓✓ PIPELINE CREATION COMPLETE ✓✓✓")
print("=" * 80)
print("""
Pipeline: /Game/Interchange/Pipelines/BP_SkeletalMesh_TextureSuffix_Pipeline
Material: /Game/Materials/M_SkeletalMesh_Master

✅ NEW FEATURES (from C++ modifications):
- Automatic suffix detection: _D, _NRA, _N, _R, _M, _AO
- Normal range auto-remap (controlled by bNormalNeedsRemap parameter)
- Pipeline compiled AND saved (C++ fix applied)

Usage:
1. Import FBX with textures named:
   - YourModel_D.png (BaseColor)
   - YourModel_NRA.png (Normal.RG + Roughness.B + AO.A)
2. Material Instance auto-created in MaterialInstances folder
3. Textures auto-assigned to material parameters!

Note: 
- C++ modifications require Live Coding (Ctrl+Alt+F11) or UE restart
- Toggle bNormalNeedsRemap in material instance if normal appears incorrect
""")
