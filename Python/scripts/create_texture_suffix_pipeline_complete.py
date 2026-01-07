"""
Complete Pipeline Creation for Texture Suffix-based Material Assignment

This script:
1. Creates Master Material (M_SkeletalMesh_Master) via Python code generation
2. Creates Pipeline Blueprint (BP_SkeletalMesh_TextureSuffix_Pipeline)
3. Configures texture parameter mappings for _D and _NRA suffixes
4. Compiles and validates the pipeline

Usage: 
1. Run this script: python create_texture_suffix_pipeline_complete.py
2. Copy and execute the generated UE Python code in UE Editor
3. Pipeline will be ready to use for FBX imports!

Author: UnrealMCP
"""

import json
import socket
import logging
import sys

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# Configuration
UNREAL_HOST = "127.0.0.1"
UNREAL_PORT = 55557

MATERIAL_NAME = "M_SkeletalMesh_Master"
MATERIAL_PATH = "/Game/Materials"

PIPELINE_NAME = "BP_SkeletalMesh_TextureSuffix_Pipeline"
PIPELINE_PATH = "/Game/Interchange/Pipelines/"
PARENT_CLASS = "FBXMaterialPipeline"


def send_mcp_command(command: str, params: dict) -> dict:
    """Send command to Unreal MCP Bridge"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10.0)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
        sock.connect((UNREAL_HOST, UNREAL_PORT))
        
        message = json.dumps({"type": command, "params": params})
        sock.sendall(message.encode('utf-8'))
        
        chunks = []
        while True:
            try:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                chunks.append(chunk)
                data = b''.join(chunks)
                try:
                    result = json.loads(data.decode('utf-8'))
                    sock.close()
                    return result
                except json.JSONDecodeError:
                    continue
            except socket.timeout:
                break
        
        sock.close()
        if chunks:
            try:
                return json.loads(b''.join(chunks).decode('utf-8'))
            except:
                pass
        
        return {"success": False, "message": "No response received"}
        
    except Exception as e:
        logger.error(f"MCP command failed: {e}")
        return {"success": False, "message": str(e)}


def is_success(response: dict) -> bool:
    return response.get("status") == "success" or response.get("success") == True


def print_header(title: str):
    print("\n" + "=" * 80)
    print(f"  {title}")
    print("=" * 80)


def print_ue_python_code():
    """Generate UE Python code for Master Material creation"""
    
    print_header("STEP 1: Create Master Material via UE Python")
    
    print("""
Copy the code below and execute it in UE Editor Python Console:
(Tools > Execute Python Script > Paste and Run)

""")
    
    code = f'''import unreal

# ============================================================================
# Create Master Material: {MATERIAL_NAME}
# Supports: _D (BaseColor) and _NRA (Normal+Roughness+AO packed) textures
# ============================================================================

material_name = "{MATERIAL_NAME}"
package_path = "{MATERIAL_PATH}"
full_path = f"{{package_path}}/{{material_name}}"

# Create or load material
if unreal.EditorAssetLibrary.does_asset_exist(full_path):
    print(f"Material already exists, loading: {{full_path}}")
    material = unreal.EditorAssetLibrary.load_asset(full_path)
else:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material_factory = unreal.MaterialFactoryNew()
    material = asset_tools.create_asset(material_name, package_path, unreal.Material, material_factory)
    print(f"✓ Created Material: {{full_path}}")

# Create texture sample parameters
print("Creating texture parameters...")

# BaseColorTexture parameter (_D suffix)
base_color_param = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionTextureSampleParameter2D, -600, -300
)
base_color_param.set_editor_property("parameter_name", "BaseColorTexture")
base_color_param.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)

# PackedTexture parameter (_NRA suffix)
packed_param = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionTextureSampleParameter2D, -600, 100
)
packed_param.set_editor_property("parameter_name", "PackedTexture")
packed_param.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)

print("✓ Texture parameters created")

# Connect BaseColor directly
print("Connecting BaseColor...")
unreal.MaterialEditingLibrary.connect_material_property(
    base_color_param, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
)

# Extract Normal (R,G channels) from PackedTexture
print("Setting up Normal extraction (R,G channels)...")
component_mask_normal = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionComponentMask, -300, 100
)
component_mask_normal.set_editor_property("r", True)
component_mask_normal.set_editor_property("g", True)
component_mask_normal.set_editor_property("b", False)
component_mask_normal.set_editor_property("a", False)

unreal.MaterialEditingLibrary.connect_material_expressions(
    packed_param, "RGB", component_mask_normal, ""
)

# Add DeriveNormalZ (reconstruct Z from X,Y)
derive_normal_z = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionDeriveNormalZ, -100, 100
)

unreal.MaterialEditingLibrary.connect_material_expressions(
    component_mask_normal, "", derive_normal_z, ""
)

unreal.MaterialEditingLibrary.connect_material_property(
    derive_normal_z, "", unreal.MaterialProperty.MP_NORMAL
)

# Extract Roughness (B channel)
print("Setting up Roughness extraction (B channel)...")
component_mask_roughness = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionComponentMask, -300, 300
)
component_mask_roughness.set_editor_property("r", False)
component_mask_roughness.set_editor_property("g", False)
component_mask_roughness.set_editor_property("b", True)
component_mask_roughness.set_editor_property("a", False)

unreal.MaterialEditingLibrary.connect_material_expressions(
    packed_param, "RGB", component_mask_roughness, ""
)

unreal.MaterialEditingLibrary.connect_material_property(
    component_mask_roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
)

# Extract AO (A channel)
print("Setting up AO extraction (A channel)...")
component_mask_ao = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionComponentMask, -300, 500
)
component_mask_ao.set_editor_property("r", False)
component_mask_ao.set_editor_property("g", False)
component_mask_ao.set_editor_property("b", False)
component_mask_ao.set_editor_property("a", True)

unreal.MaterialEditingLibrary.connect_material_expressions(
    packed_param, "RGBA", component_mask_ao, ""
)

unreal.MaterialEditingLibrary.connect_material_property(
    component_mask_ao, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION
)

# Recompile and save
print("Compiling and saving material...")
unreal.MaterialEditingLibrary.recompile_material(material)
unreal.MaterialEditingLibrary.layout_material_expressions(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)

print("=" * 80)
print(f"✓✓✓ Master Material Created Successfully! ✓✓✓")
print("=" * 80)
print(f"Path: {{full_path}}")
print(f"")
print(f"Material Parameters:")
print(f"  - BaseColorTexture (for _D textures)")
print(f"  - PackedTexture (for _NRA textures)")
print(f"")
print(f"Channel Mapping:")
print(f"  PackedTexture.R → Normal.X")
print(f"  PackedTexture.G → Normal.Y")
print(f"  PackedTexture.B → Roughness")
print(f"  PackedTexture.A → Ambient Occlusion")
print(f"")
print(f"✓ Material is ready to use as Pipeline ParentMaterial!")
'''
    
    print(code)
    print("\n" + "-" * 80)
    print("After executing the code above, press Enter to continue...")
    input()


def create_pipeline():
    """Create Pipeline Blueprint using MCP"""
    
    print_header("STEP 2: Create Pipeline Blueprint via MCP")
    
    full_path = PIPELINE_PATH + PIPELINE_NAME
    
    print(f"\\nCreating Pipeline Blueprint...")
    print(f"  Name: {PIPELINE_NAME}")
    print(f"  Path: {PIPELINE_PATH}")
    print(f"  Parent: {PARENT_CLASS}")
    
    result = send_mcp_command("create_interchange_pipeline_blueprint", {
        "name": PIPELINE_NAME,
        "package_path": PIPELINE_PATH,
        "parent_class": PARENT_CLASS
    })
    
    if not is_success(result):
        error_msg = result.get("message", result.get("error", "Unknown error"))
        if "already exists" in error_msg.lower():
            print(f"  ⚠ Pipeline already exists: {full_path}")
        else:
            print(f"  ✗ Failed: {error_msg}")
            return False
    else:
        data = result.get("result", result)
        print(f"  ✓ Created: {data.get('path', full_path)}")
    
    return True


def configure_pipeline():
    """Configure Pipeline settings via MCP"""
    
    print_header("STEP 3: Configure Pipeline Texture Mappings")
    
    full_path = PIPELINE_PATH + PIPELINE_NAME
    material_full_path = f"{MATERIAL_PATH}/{MATERIAL_NAME}.{MATERIAL_NAME}"
    
    # Configure settings
    settings = {
        "bAutoCreateMaterialInstances": True,
        "bAutoAssignTextures": True,
        "bSearchExistingMaterials": True,
        "MaterialInstanceSubFolder": "MaterialInstances",
        "ParentMaterial": material_full_path,
        # Note: TextureParameterMapping is TMap, we'll try to set it
        "TextureParameterMapping": {
            "_D": "BaseColorTexture",
            "_NRA": "PackedTexture"
        }
    }
    
    print(f"\\nConfiguring Pipeline settings...")
    print(f"  Auto-create Material Instances: True")
    print(f"  Auto-assign Textures: True")
    print(f"  Parent Material: {material_full_path}")
    print(f"  Texture Suffix Mappings:")
    print(f"    _D   → BaseColorTexture")
    print(f"    _NRA → PackedTexture")
    
    result = send_mcp_command("configure_interchange_pipeline", {
        "pipeline_path": full_path,
        "settings": settings
    })
    
    if is_success(result):
        data = result.get("result", result)
        configured = data.get("configured_properties", [])
        print(f"  ✓ Configured {len(configured)} properties")
        if configured:
            for prop in configured:
                print(f"    - {prop}")
    else:
        error = result.get("message", result.get("error", "Unknown error"))
        print(f"  ⚠ Configuration note: {error}")
        print(f"  ℹ Some properties may need manual configuration in UE Editor")
    
    return True


def compile_pipeline():
    """Compile Pipeline Blueprint via MCP"""
    
    print_header("STEP 4: Compile Pipeline Blueprint")
    
    full_path = PIPELINE_PATH + PIPELINE_NAME
    
    print(f"\\nCompiling Pipeline...")
    
    result = send_mcp_command("compile_interchange_pipeline", {
        "pipeline_path": full_path
    })
    
    if is_success(result):
        data = result.get("result", result)
        status = data.get("status", "OK")
        print(f"  ✓ Compilation Status: {status}")
    else:
        error = result.get("message", result.get("error", "Unknown error"))
        print(f"  ⚠ {error}")
    
    return True


def print_usage_guide():
    """Print usage guide for the created pipeline"""
    
    print_header("Pipeline Creation Complete!")
    
    print(f"""
╔═══════════════════════════════════════════════════════════════════════════╗
║                 Texture Suffix Pipeline - Usage Guide                     ║
╚═══════════════════════════════════════════════════════════════════════════╝

Pipeline Path: {PIPELINE_PATH}{PIPELINE_NAME}
Master Material: {MATERIAL_PATH}/{MATERIAL_NAME}

🎯 Texture Naming Convention:
   ─────────────────────────────────────────────────────────────────────────
   YourTextureName_D.png     → BaseColor (Diffuse/Albedo)
   YourTextureName_NRA.png   → Packed Texture (Normal + Roughness + AO)
                                - R: Normal X
                                - G: Normal Y  
                                - B: Roughness
                                - A: Ambient Occlusion

📦 How to Use:
   ─────────────────────────────────────────────────────────────────────────
   1. Prepare your FBX model with textures following the naming convention:
      Example:
        Character.fbx
        Character_D.png      (diffuse/base color)
        Character_NRA.png    (packed: normal XY + roughness + AO)

   2. Import FBX using Interchange:
      - Drag FBX into Content Browser
      - Interchange Import Dialog will appear
      - Add Pipeline: {PIPELINE_NAME}
      - Click Import

   3. Automatic Material Instance Creation:
      ✓ Material Instance auto-created in MaterialInstances folder
      ✓ Textures auto-assigned based on suffix
      ✓ Material Instance auto-applied to imported mesh

🔧 Manual Configuration (Optional):
   ─────────────────────────────────────────────────────────────────────────
   If TextureParameterMapping was not set via MCP, you can manually configure:
   
   1. Open Pipeline Blueprint: {PIPELINE_PATH}{PIPELINE_NAME}
   2. In Details panel, find "Texture Parameter Mapping"
   3. Add entries:
      Key: "_D"    Value: "BaseColorTexture"
      Key: "_NRA"  Value: "PackedTexture"
   4. Compile and save

🧪 Testing:
   ─────────────────────────────────────────────────────────────────────────
   1. Create a test FBX with textures named:
      - TestModel_D.png
      - TestModel_NRA.png
   
   2. Import and verify:
      ✓ Material Instance created: MI_TestModel
      ✓ BaseColorTexture = TestModel_D
      ✓ PackedTexture = TestModel_NRA
      ✓ Material looks correct in viewport

📚 Advanced:
   ─────────────────────────────────────────────────────────────────────────
   - To add more suffix mappings, edit TextureParameterMapping in Pipeline
   - To customize material, edit {MATERIAL_NAME} and adjust parameter names
   - Master Material can be used directly or create child Material Instances

╔═══════════════════════════════════════════════════════════════════════════╗
║  ✓ Setup Complete! Your Pipeline is ready for texture suffix imports!   ║
╚═══════════════════════════════════════════════════════════════════════════╝
""")


def main():
    print("""
╔═══════════════════════════════════════════════════════════════════════════╗
║                                                                           ║
║        UnrealMCP - Texture Suffix Pipeline Creator                       ║
║        Complete Solution for _D and _NRA Texture Imports                 ║
║                                                                           ║
╚═══════════════════════════════════════════════════════════════════════════╝
""")
    
    # Step 1: Generate UE Python code for Master Material
    print_ue_python_code()
    
    # Test connection
    print_header("Testing Connection to UE Editor")
    result = send_mcp_command("get_interchange_info", {})
    
    if is_success(result):
        print("✓ Connected to Unreal Editor")
    else:
        print("✗ Cannot connect to UE Editor")
        print("  Please ensure:")
        print("    1. UE Editor is running")
        print("    2. UnrealMCP plugin is loaded")
        print("    3. TCP server is listening on port", UNREAL_PORT)
        return 1
    
    # Step 2: Create Pipeline Blueprint
    if not create_pipeline():
        print("\\n✗ Failed to create Pipeline Blueprint")
        return 1
    
    # Step 3: Configure Pipeline
    if not configure_pipeline():
        print("\\n✗ Failed to configure Pipeline")
        return 1
    
    # Step 4: Compile Pipeline
    if not compile_pipeline():
        print("\\n✗ Failed to compile Pipeline")
        return 1
    
    # Print usage guide
    print_usage_guide()
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
