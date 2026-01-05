import socket, json

def mcp(cmd, params):
    s = socket.socket()
    s.connect(('127.0.0.1', 55557))
    s.sendall(json.dumps({"type": cmd, "params": params}).encode())
    data = b''
    while True:
        chunk = s.recv(4096)
        if not chunk: break
        data += chunk
        try:
            result = json.loads(data.decode())
            s.close()
            return result
        except: continue
    s.close()
    return json.loads(data.decode()) if data else {}

print("Testing MCP...")
r = mcp("ping", {})
print(f"Ping: {r.get('result', {}).get('message')}")

if r.get('result', {}).get('message') == 'pong':
    print("\n[1/4] Creating Pipeline...")
    r = mcp("create_interchange_pipeline_blueprint", {
        "name": "BP_SkeletalMesh_TextureSuffix_Pipeline",
        "package_path": "/Game/Interchange/Pipelines/",
        "parent_class": "FBXMaterialPipeline"
    })
    print(f"Result: {r.get('status')} - {r.get('result', {}).get('message', 'OK')}")
    
    print("\n[2/4] Configuring Pipeline...")
    r = mcp("configure_interchange_pipeline", {
        "pipeline_path": "/Game/Interchange/Pipelines/BP_SkeletalMesh_TextureSuffix_Pipeline",
        "settings": {
            "bAutoCreateMaterialInstances": True,
            "bAutoAssignTextures": True,
            "bSearchExistingMaterials": True,
            "MaterialInstanceSubFolder": "MaterialInstances",
            "ParentMaterial": "/Game/Materials/M_SkeletalMesh_Master.M_SkeletalMesh_Master"
        }
    })
    print(f"Result: {r.get('status')}")
    
    print("\n[3/4] Compiling Pipeline...")
    r = mcp("compile_interchange_pipeline", {
        "pipeline_path": "/Game/Interchange/Pipelines/BP_SkeletalMesh_TextureSuffix_Pipeline"
    })
    print(f"Result: {r.get('status')} - {r.get('result', {}).get('status')}")
    
    print("\n[4/4] Creating Master Material...")
    code = '''import unreal
m = "/Game/Materials/M_SkeletalMesh_Master"
if not unreal.EditorAssetLibrary.does_asset_exist(m):
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_SkeletalMesh_Master", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
    bc = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -600, -300)
    bc.set_editor_property("parameter_name", "BaseColorTexture")
    unreal.MaterialEditingLibrary.connect_material_property(bc, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    pk = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -600, 100)
    pk.set_editor_property("parameter_name", "PackedTexture")
    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    print("Created")
else:
    print("Exists")
'''
    r = mcp("execute_python", {"code": code})
    print(f"Result: {r.get('status')}")
    
    print("\n✓ Done! Pipeline ready at: /Game/Interchange/Pipelines/BP_SkeletalMesh_TextureSuffix_Pipeline")
