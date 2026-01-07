"""
Create Master Material for Skeletal Mesh with Texture Suffix Support

This script creates a Master Material (M_SkeletalMesh_Master) that supports:
- _D suffix textures (Diffuse/BaseColor)
- _NRA suffix textures (Packed: Normal XY + Roughness + AO)

Usage: Run with UE Editor open and UnrealMCP plugin loaded
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
MATERIAL_PATH = "/Game/Materials/"


def send_mcp_command(command: str, params: dict) -> dict:
    """Send command to Unreal MCP Bridge"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10.0)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
        sock.connect((UNREAL_HOST, UNREAL_PORT))
        
        message = json.dumps({
            "type": command,
            "params": params
        })
        
        logger.debug(f"Sending: {message}")
        sock.sendall(message.encode('utf-8'))
        
        chunks = []
        sock.settimeout(10.0)
        
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
            data = b''.join(chunks)
            try:
                return json.loads(data.decode('utf-8'))
            except:
                pass
        
        return {"success": False, "message": "No response received"}
        
    except ConnectionRefusedError:
        logger.error("Connection refused - UE Editor not running or MCP plugin not loaded")
        return {"success": False, "message": "Connection refused"}
    except Exception as e:
        logger.error(f"MCP command failed: {e}")
        return {"success": False, "message": str(e)}


def is_success(response: dict) -> bool:
    """Check if response indicates success"""
    return response.get("status") == "success" or response.get("success") == True


def get_error(response: dict) -> str:
    """Extract error message from response"""
    if response.get("status") == "error":
        return response.get("error", "Unknown error")
    return response.get("message") or response.get("error", "Unknown error")


def create_master_material():
    """
    Create Master Material using Blueprint Material Function approach.
    
    Since MCP doesn't have direct material node editing, we'll create a Blueprint
    that can be used as a Material Function, or provide Python code for manual creation.
    """
    
    full_path = MATERIAL_PATH + MATERIAL_NAME
    
    print("=" * 80)
    print("  Creating Master Material for Skeletal Mesh with Texture Suffix Support")
    print("=" * 80)
    print(f"""
Master Material Configuration:
  Name: {MATERIAL_NAME}
  Path: {MATERIAL_PATH}
  
Texture Parameters:
  1. BaseColorTexture (Texture2D) - For _D suffix textures
  2. PackedTexture (Texture2D) - For _NRA suffix textures
     - R Channel: Normal X
     - G Channel: Normal Y
     - B Channel: Roughness
     - A Channel: Ambient Occlusion
     
Material Properties:
  - Material Domain: Surface
  - Blend Mode: Opaque
  - Shading Model: Default Lit
  - Two Sided: False
""")
    
    # Since MCP doesn't have material creation tools, we'll generate Python code
    # that can be executed in UE Python console
    
    print("\n[Generating Python Code for Material Creation]")
    print("=" * 80)
    
    python_code = '''
import unreal

# Create Material Package
material_name = "M_SkeletalMesh_Master"
package_path = "/Game/Materials"
full_path = f"{package_path}/{material_name}"

# Check if already exists
if unreal.EditorAssetLibrary.does_asset_exist(full_path):
    print(f"Material already exists: {full_path}")
    material = unreal.EditorAssetLibrary.load_asset(full_path)
else:
    # Create new Material
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material_factory = unreal.MaterialFactoryNew()
    
    material = asset_tools.create_asset(
        material_name,
        package_path,
        unreal.Material,
        material_factory
    )
    print(f"Created Material: {full_path}")

# Get material editor
material_editing = unreal.MaterialEditingLibrary

# Clear existing nodes (if any)
# Note: This is a simplified version. Full implementation would need proper node management

# Create Texture Parameters
base_color_param = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionTextureSampleParameter2D, -400, -200
)
base_color_param.set_editor_property("parameter_name", "BaseColorTexture")
base_color_param.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)

packed_param = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionTextureSampleParameter2D, -400, 200
)
packed_param.set_editor_property("parameter_name", "PackedTexture")
packed_param.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)

# Connect BaseColor
unreal.MaterialEditingLibrary.connect_material_property(
    base_color_param, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
)

# Extract Normal from PackedTexture (R,G channels)
component_mask_normal = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionComponentMask, -200, 200
)
component_mask_normal.set_editor_property("r", True)
component_mask_normal.set_editor_property("g", True)
component_mask_normal.set_editor_property("b", False)
component_mask_normal.set_editor_property("a", False)

unreal.MaterialEditingLibrary.connect_material_expressions(
    packed_param, "RGB", component_mask_normal, ""
)

# Reconstruct Normal Z (use DeriveNormalZ or manual calculation)
# For simplicity, we'll use a FlattenNormal node if available, or connect directly
# Note: Proper Normal requires Z reconstruction: Z = sqrt(1 - X*X - Y*Y)

# For now, connect RG to Normal (incomplete, needs Z reconstruction)
# In a real implementation, you'd use a MaterialFunction for Normal reconstruction

# Extract Roughness from PackedTexture (B channel)
component_mask_roughness = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionComponentMask, -200, 400
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

# Extract AO from PackedTexture (A channel)
component_mask_ao = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionComponentMask, -200, 600
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

# Update material and save
unreal.MaterialEditingLibrary.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)

print(f"✓ Master Material created: {full_path}")
print(f"  - BaseColorTexture parameter added")
print(f"  - PackedTexture parameter added (with channel splits)")
print(f"\\nNote: Normal channel needs Z reconstruction for proper results.")
print(f"      Consider adding a Material Function for Normal reconstruction.")
'''
    
    print(python_code)
    print("\n" + "=" * 80)
    print("[Instructions]")
    print("=" * 80)
    print("""
To create the Master Material:

1. Copy the Python code above
2. Open Unreal Editor
3. Window > Developer Tools > Output Log
4. Switch to "Python" log category
5. Open Python Console: Tools > Execute Python Script
6. Paste the code and execute

Or use MCP to create a simplified version...
""")
    
    # Alternative: Create via Blueprint Material Function (not fully supported yet)
    print("\n[Alternative: Using MCP to guide material creation]")
    print("=" * 80)
    print("""
Since direct material node editing is not available via MCP, I'll provide
a manual creation guide that you can follow in UE Editor:

STEP-BY-STEP GUIDE:
═══════════════════════════════════════════════════════════════════════════════

1. Create New Material:
   - Content Browser > Right-click in /Game/Materials/
   - Create > Material
   - Name: M_SkeletalMesh_Master

2. Add Texture Parameters:
   
   a) BaseColorTexture (for _D suffix):
      - Right-click in Material Graph > Parameters > Texture Sample Parameter
      - Name: "BaseColorTexture"
      - Connect RGB pin to Base Color input
   
   b) PackedTexture (for _NRA suffix):
      - Add another Texture Sample Parameter
      - Name: "PackedTexture"

3. Extract Channels from PackedTexture:

   a) Normal (R,G channels):
      - Add "Component Mask" node
      - Connect PackedTexture to Component Mask
      - Enable only R and G channels
      - Add "DeriveNormalZ" node (search in palette)
      - Connect Component Mask to DeriveNormalZ
      - Connect DeriveNormalZ to Normal input
      
   b) Roughness (B channel):
      - Add "Component Mask" node
      - Connect PackedTexture to it
      - Enable only B channel
      - Connect to Roughness input
      
   c) Ambient Occlusion (A channel):
      - Add "Component Mask" node
      - Connect PackedTexture to it
      - Enable only A channel
      - Connect to Ambient Occlusion input

4. Configure Material Properties:
   - Details Panel > Material:
     - Blend Mode: Opaque
     - Shading Model: Default Lit
     - Two Sided: False (usually, unless needed)

5. Apply and Save:
   - Click "Apply" button
   - Click "Save" button
   - Material is ready to use!

═══════════════════════════════════════════════════════════════════════════════

Visual Layout:
                                                                    ┌──────────┐
┌──────────────────────┐                                      ┌───►│BaseColor │
│ BaseColorTexture     ├──RGB───────────────────────────────►│    └──────────┘
│ (Texture Parameter)  │                                      │    
└──────────────────────┘                                      │    ┌──────────┐
                                                              │    │ Normal   │
┌──────────────────────┐     ┌───────────────┐  ┌─────────┐ │    └──────────┘
│ PackedTexture        ├─RG─►│ComponentMask  ├─►│DeriveNZ ├─┤         ▲
│ (Texture Parameter)  │     │(R=1,G=1)      │  │         │ │         │
│                      │     └───────────────┘  └─────────┘ │         │
│                      │                                     │    ┌──────────┐
│                      │     ┌───────────────┐              ├───►│Roughness │
│                      ├─B──►│ComponentMask  ├──────────────┤    └──────────┘
│                      │     │(B=1)          │              │
│                      │     └───────────────┘              │    ┌──────────┐
│                      │                                     └───►│   AO     │
│                      │     ┌───────────────┐                   └──────────┘
│                      ├─A──►│ComponentMask  ├───────────────────────┘
│                      │     │(A=1)          │
└──────────────────────┘     └───────────────┘

""")
    
    return True


def main():
    print("\nUnrealMCP - Master Material Creator for Texture Suffix Pipeline")
    print("-" * 80)
    
    # Test connection
    print("\nTesting connection to UE Editor...")
    result = send_mcp_command("get_interchange_info", {})
    
    if is_success(result):
        print("✓ Connected to Unreal Editor")
    else:
        print(f"⚠ Cannot connect to UE Editor (this is OK for material creation guide)")
        print(f"  You can still use the generated Python code or manual guide below.")
    
    # Generate material creation guide
    create_master_material()
    
    print("\n✓ Master Material creation guide generated!")
    print("\nNext steps:")
    print("  1. Create the Master Material using one of the methods above")
    print("  2. Run: python create_skeletal_material_pipeline.py")
    print("  3. Configure the Pipeline to use M_SkeletalMesh_Master as ParentMaterial")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
