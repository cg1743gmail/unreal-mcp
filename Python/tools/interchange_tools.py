"""
Interchange Tools for Unreal MCP.

This module provides tools for Unreal Engine Interchange system operations,
including model import, asset management, and Blueprint creation from imported assets.

Supports UE 5.5+ Interchange formats: FBX, glTF, USD, Alembic, OBJ, PLY
"""

import logging
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

# Get logger
logger = logging.getLogger("UnrealMCP")


def register_interchange_tools(mcp: FastMCP):
    """Register Interchange tools with the MCP server."""
    
    @mcp.tool()
    def import_model(
        ctx: Context,
        file_path: str,
        destination_path: str = "/Game/Imported",
        import_mesh: bool = True,
        import_material: bool = True,
        import_texture: bool = True,
        import_skeleton: bool = True,
        create_physics_asset: bool = False
    ) -> Dict[str, Any]:
        """
        Import a 3D model file using Unreal Interchange system.
        
        Supported formats: FBX, glTF/GLB, USD/USDA/USDZ, Alembic (ABC), OBJ, PLY
        
        Args:
            file_path: Absolute path to the source model file
            destination_path: UE content path for imported assets (default: /Game/Imported)
            import_mesh: Import mesh data (default: True)
            import_material: Import materials (default: True)
            import_texture: Import textures (default: True)
            import_skeleton: Import skeleton for skeletal meshes (default: True)
            create_physics_asset: Create physics asset for skeletal meshes (default: False)
            
        Returns:
            Import configuration and validation result
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "file_path": file_path,
                "destination_path": destination_path,
                "import_mesh": import_mesh,
                "import_material": import_material,
                "import_texture": import_texture,
                "import_skeleton": import_skeleton,
                "create_physics_asset": create_physics_asset
            }
            
            logger.info(f"Importing model with params: {params}")
            response = unreal.send_command("import_model", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Import model response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error importing model: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def create_interchange_blueprint(
        ctx: Context,
        name: str,
        mesh_path: str
    ) -> Dict[str, Any]:
        """
        Create a Blueprint from an imported mesh asset.
        
        Automatically determines the appropriate parent class and component type
        based on the mesh type (StaticMesh -> Actor, SkeletalMesh -> Pawn).
        
        Args:
            name: Name for the new Blueprint
            mesh_path: UE content path to the mesh asset (e.g., /Game/Imported/MyMesh)
            
        Returns:
            Created Blueprint information including path and component type
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "name": name,
                "mesh_path": mesh_path
            }
            
            logger.info(f"Creating interchange blueprint with params: {params}")
            response = unreal.send_command("create_interchange_blueprint", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Create interchange blueprint response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error creating interchange blueprint: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def create_custom_interchange_blueprint(
        ctx: Context,
        name: str,
        package_path: str = "/Game/Blueprints/",
        parent_class: str = "Actor",
        mesh_path: str = "",
        components: List[Dict[str, str]] = []
    ) -> Dict[str, Any]:
        """
        Create a custom Blueprint with flexible configuration for Interchange assets.
        
        Supports custom parent classes, optional mesh assignment, and multiple components.
        
        Args:
            name: Name for the new Blueprint
            package_path: UE content path for the Blueprint (default: /Game/Blueprints/)
            parent_class: Parent class name - Actor, Pawn, or Character (default: Actor)
            mesh_path: Optional UE content path to a mesh asset to add as component
            components: Optional list of additional components to add
                       Each component: {"type": "ComponentType", "name": "ComponentName"}
                       Supported types: SceneComponent, StaticMeshComponent, SkeletalMeshComponent,
                                       CapsuleComponent, BoxComponent, SphereComponent
            
        Returns:
            Created Blueprint information including path and parent class
            
        Example:
            create_custom_interchange_blueprint(
                name="MyCharacterBP",
                parent_class="Character",
                mesh_path="/Game/Imported/CharacterMesh",
                components=[
                    {"type": "CapsuleComponent", "name": "CollisionCapsule"},
                    {"type": "SceneComponent", "name": "WeaponSocket"}
                ]
            )
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "name": name,
                "package_path": package_path,
                "parent_class": parent_class
            }
            
            # Add optional mesh_path if provided
            if mesh_path:
                params["mesh_path"] = mesh_path
            
            # Add optional components if provided
            if components and len(components) > 0:
                params["components"] = components
            
            logger.info(f"Creating custom interchange blueprint with params: {params}")
            response = unreal.send_command("create_custom_interchange_blueprint", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Create custom interchange blueprint response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error creating custom interchange blueprint: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def get_interchange_assets(
        ctx: Context,
        search_path: str = "/Game/",
        asset_type: str = ""
    ) -> Dict[str, Any]:
        """
        Query imported assets in the project.
        
        Args:
            search_path: UE content path to search (default: /Game/)
            asset_type: Filter by asset type - static_mesh, skeletal_mesh, material, texture
                       Leave empty for all types
            
        Returns:
            List of matching assets with name, path, and class information
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "search_path": search_path
            }
            
            if asset_type:
                params["asset_type"] = asset_type
            
            logger.info(f"Getting interchange assets with params: {params}")
            response = unreal.send_command("get_interchange_assets", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Get interchange assets response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error getting interchange assets: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def reimport_asset(
        ctx: Context,
        asset_path: str
    ) -> Dict[str, Any]:
        """
        Trigger reimport for an existing asset.
        
        Useful for updating assets after source file modifications.
        Supports StaticMesh, SkeletalMesh, Texture2D, and Material assets.
        
        Args:
            asset_path: UE content path to the asset to reimport
            
        Returns:
            Reimport status and result
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "asset_path": asset_path
            }
            
            logger.info(f"Reimporting asset: {asset_path}")
            response = unreal.send_command("reimport_asset", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Reimport asset response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error reimporting asset: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def get_interchange_info(
        ctx: Context,
        asset_path: str = ""
    ) -> Dict[str, Any]:
        """
        Get Interchange system information and optionally asset metadata.
        
        Args:
            asset_path: Optional UE content path to get specific asset metadata
            
        Returns:
            Supported formats, engine version, and optional asset metadata
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {}
            if asset_path:
                params["asset_path"] = asset_path
            
            logger.info(f"Getting interchange info with params: {params}")
            response = unreal.send_command("get_interchange_info", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Get interchange info response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error getting interchange info: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    logger.info("Interchange tools registered successfully")
