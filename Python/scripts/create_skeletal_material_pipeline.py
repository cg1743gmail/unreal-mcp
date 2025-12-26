"""
Create Skeletal Mesh Material Instance Pipeline via MCP

This script creates an Interchange Pipeline Blueprint based on 
UUnrealMCPFBXMaterialPipeline that:
1. Imports Skeletal Mesh assets
2. Automatically creates Material Instances from imported materials
3. Connects textures (BaseColor, Normal, Roughness, Metallic, AO, etc.)

Usage: Run this script with UE Editor running and UnrealMCP plugin loaded
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

PIPELINE_NAME = "BP_SkeletalMeshMaterialPipeline"
PIPELINE_PATH = "/Game/Interchange/Pipelines/"
PARENT_CLASS = "FBXMaterialPipeline"


def send_mcp_command(command: str, params: dict) -> dict:
    """Send command to Unreal MCP Bridge (matching unreal_mcp_server.py protocol)"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10.0)
        
        # Set socket options
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
        
        sock.connect((UNREAL_HOST, UNREAL_PORT))
        
        # Send command as JSON (no length prefix, matching UE bridge protocol)
        message = json.dumps({
            "type": command,
            "params": params
        })
        
        logger.debug(f"Sending: {message}")
        sock.sendall(message.encode('utf-8'))
        
        # Receive response (read until complete JSON)
        chunks = []
        sock.settimeout(10.0)
        
        while True:
            try:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                chunks.append(chunk)
                
                # Try to parse as complete JSON
                data = b''.join(chunks)
                try:
                    result = json.loads(data.decode('utf-8'))
                    sock.close()
                    return result
                except json.JSONDecodeError:
                    # Not complete yet, continue reading
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
    except socket.timeout:
        logger.error("Connection timed out")
        return {"success": False, "message": "Connection timed out"}
    except Exception as e:
        logger.error(f"MCP command failed: {e}")
        return {"success": False, "message": str(e)}


def get_result(response: dict) -> dict:
    """Extract result from response, handling both formats"""
    if response.get("status") == "success":
        return response.get("result", response)
    return response


def is_success(response: dict) -> bool:
    """Check if response indicates success"""
    return response.get("status") == "success" or response.get("success") == True


def get_error(response: dict) -> str:
    """Extract error message from response"""
    if response.get("status") == "error":
        return response.get("error", "Unknown error")
    return response.get("message") or response.get("error", "Unknown error")


def create_skeletal_material_pipeline():
    """Create the Skeletal Mesh Material Instance Pipeline Blueprint"""
    
    full_path = PIPELINE_PATH + PIPELINE_NAME
    
    print("=" * 70)
    print("  Creating Skeletal Mesh Material Instance Pipeline")
    print("  Base Class: UUnrealMCPFBXMaterialPipeline (C++)")
    print("=" * 70)
    
    # Step 1: Create Pipeline Blueprint
    print("\n[Step 1] Creating Pipeline Blueprint...")
    print(f"  Name: {PIPELINE_NAME}")
    print(f"  Path: {PIPELINE_PATH}")
    print(f"  Parent: {PARENT_CLASS}")
    
    result = send_mcp_command("create_interchange_pipeline_blueprint", {
        "name": PIPELINE_NAME,
        "package_path": PIPELINE_PATH,
        "parent_class": PARENT_CLASS
    })
    
    if not is_success(result):
        error_msg = get_error(result)
        if "already exists" in error_msg.lower():
            print(f"  ⚠ Pipeline already exists: {full_path}")
        else:
            print(f"  ✗ Failed: {error_msg}")
            return False
    else:
        data = get_result(result)
        print(f"  ✓ Created: {data.get('path')}")
        print(f"  Parent Class: {data.get('parent_class')}")
    
    # Step 2: Verify pipeline structure
    print("\n[Step 2] Verifying pipeline structure...")
    result = send_mcp_command("get_interchange_pipeline_graph", {
        "pipeline_path": full_path
    })
    
    if is_success(result):
        data = get_result(result)
        print(f"  ✓ Blueprint: {data.get('blueprint_name')}")
        print(f"  ✓ Parent Class: {data.get('parent_class')}")
        
        overridable = data.get("overridable_functions", [])
        if overridable:
            print(f"  ✓ Overridable Functions: {len(overridable)}")
            for func in overridable[:5]:
                print(f"      - {func.get('name')}")
    else:
        print(f"  ⚠ Could not verify: {get_error(result)}")
    
    # Step 3: Configure pipeline settings
    print("\n[Step 3] Configuring pipeline settings...")
    result = send_mcp_command("configure_interchange_pipeline", {
        "pipeline_path": full_path,
        "settings": {
            "bAutoCreateMaterialInstances": True,
            "bAutoAssignTextures": True,
            "bSearchExistingMaterials": True,
            "MaterialInstanceSubFolder": "MaterialInstances"
        }
    })
    
    if is_success(result):
        data = get_result(result)
        configured = data.get("configured_properties", [])
        print(f"  ✓ Configured {len(configured)} properties")
    else:
        print(f"  ⚠ Note: {get_error(result)}")
    
    # Step 4: Compile
    print("\n[Step 4] Compiling pipeline...")
    result = send_mcp_command("compile_interchange_pipeline", {
        "pipeline_path": full_path
    })
    
    if is_success(result):
        data = get_result(result)
        print(f"  ✓ Status: {data.get('status', 'OK')}")
    else:
        print(f"  ⚠ {get_error(result)}")
    
    # Summary
    print("\n" + "=" * 70)
    print("  Pipeline Creation Complete!")
    print("=" * 70)
    print(f"""
Pipeline Path: {full_path}

Features (from UUnrealMCPFBXMaterialPipeline):
  ✓ Auto-create Material Instances
  ✓ Auto-assign textures by naming convention
  ✓ PBR texture mapping:
      BaseColor/Diffuse/Albedo → BaseColorTexture
      Normal → NormalTexture
      Roughness → RoughnessTexture
      Metallic → MetallicTexture
      AO → AmbientOcclusionTexture

Usage:
  1. Open {full_path} in UE Editor
  2. Set 'Parent Material' to your master material
  3. Add to Interchange import pipeline stack
  4. Import FBX/glTF - Material Instances auto-created!
""")
    
    return True


def main():
    print("\nUnrealMCP - Skeletal Mesh Material Pipeline Creator")
    print("-" * 50)
    
    # Test connection
    print("\nTesting connection to UE Editor...")
    result = send_mcp_command("get_interchange_info", {})
    
    # Handle response format: {"status": "success", "result": {...}}
    if result.get("status") == "success":
        inner = result.get("result", result)
        print("✓ Connected to Unreal Editor")
        if inner.get("engine_version"):
            print(f"  Engine: {inner.get('engine_version')}")
        if inner.get("supported_formats"):
            print(f"  Formats: {', '.join(inner.get('supported_formats', []))}")
    elif result.get("success") or result.get("supported_formats"):
        print("✓ Connected to Unreal Editor")
        if result.get("engine_version"):
            print(f"  Engine: {result.get('engine_version')}")
    else:
        print(f"\n✗ Cannot connect: {result.get('message', result.get('error', 'Unknown'))}")
        return 1
    
    # Create pipeline
    success = create_skeletal_material_pipeline()
    
    if success:
        print("\n✓ Pipeline created successfully!")
        return 0
    else:
        print("\n✗ Pipeline creation failed")
        return 1


if __name__ == "__main__":
    sys.exit(main())
