"""
Interchange Tools for Unreal MCP.

This module provides tools for Unreal Engine Interchange system operations,
including model import, asset management, Blueprint creation from imported assets,
and Interchange Pipeline Blueprint creation and configuration.

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
    
    @mcp.tool()
    def create_interchange_pipeline_blueprint(
        ctx: Context,
        name: str,
        package_path: str = "/Game/Interchange/Pipelines/",
        parent_class: str = "GenericAssetsPipeline"
    ) -> Dict[str, Any]:
        """
        Create an Interchange Pipeline Blueprint for customizing asset import behavior.
        
        Interchange Pipeline Blueprints allow you to configure how assets are imported,
        including mesh settings, material settings, texture settings, and more.
        
        Args:
            name: Name for the new Pipeline Blueprint
            package_path: UE content path for the Blueprint (default: /Game/Interchange/Pipelines/)
            parent_class: Parent pipeline class to inherit from. Options:
                         - GenericAssetsPipeline (default, general purpose)
                         - GenericMeshPipeline (for mesh-specific settings)
                         - GenericMaterialPipeline (for material-specific settings)
                         - GenericTexturePipeline (for texture-specific settings)
                         - PipelineBase (minimal base class)
                         - FBXMaterialPipeline (custom UnrealMCP pipeline for FBX material instance auto-setup)
            
        Returns:
            Created Pipeline Blueprint information including path and parent class
            
        Example:
            create_interchange_pipeline_blueprint(
                name="MyFBXMaterialPipeline",
                package_path="/Game/INTERCHANGE BP/",
                parent_class="FBXMaterialPipeline"
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
            
            logger.info(f"Creating interchange pipeline blueprint: {params}")
            response = unreal.send_command("create_interchange_pipeline_blueprint", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Create interchange pipeline blueprint response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error creating interchange pipeline blueprint: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def get_interchange_pipelines(
        ctx: Context,
        search_path: str = "/Game/"
    ) -> Dict[str, Any]:
        """
        Get available Interchange Pipeline Blueprints and native pipelines.
        
        Returns both user-created Pipeline Blueprints and available native pipeline classes
        that can be used as parent classes for new Pipeline Blueprints.
        
        Args:
            search_path: UE content path to search for Pipeline Blueprints (default: /Game/)
            
        Returns:
            Lists of blueprint pipelines and native pipeline classes
            
        Example:
            get_interchange_pipelines("/Game/Interchange/")
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
            
            logger.info(f"Getting interchange pipelines: {params}")
            response = unreal.send_command("get_interchange_pipelines", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Get interchange pipelines response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error getting interchange pipelines: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def configure_interchange_pipeline(
        ctx: Context,
        pipeline_path: str,
        settings: Dict[str, Any] = {}
    ) -> Dict[str, Any]:
        """
        Configure an Interchange Pipeline Blueprint's settings.
        
        Allows setting properties on a Pipeline Blueprint to customize import behavior.
        Common settings depend on the parent pipeline class.
        
        Args:
            pipeline_path: UE content path to the Pipeline Blueprint
            settings: Dictionary of property names and values to set
                     Example: {"bImportMeshes": True, "bImportMaterials": False}
            
        Returns:
            Configuration result with list of properties that were set
            
        Example:
            configure_interchange_pipeline(
                pipeline_path="/Game/Interchange/Pipelines/MyPipeline",
                settings={
                    "bImportMeshes": True,
                    "bImportMaterials": True,
                    "bImportTextures": False
                }
            )
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "pipeline_path": pipeline_path,
                "settings": settings
            }
            
            logger.info(f"Configuring interchange pipeline: {params}")
            response = unreal.send_command("configure_interchange_pipeline", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Configure interchange pipeline response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error configuring interchange pipeline: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    # ============================================================================
    # Interchange Pipeline Blueprint Node Operations
    # ============================================================================
    
    @mcp.tool()
    def get_interchange_pipeline_graph(
        ctx: Context,
        pipeline_path: str
    ) -> Dict[str, Any]:
        """
        Get the graph structure of an Interchange Pipeline Blueprint.
        
        Returns information about the pipeline's event graph, including available
        overridable functions and existing nodes.
        
        Args:
            pipeline_path: UE content path to the Pipeline Blueprint
            
        Returns:
            Graph structure including nodes, overridable functions, and connections
            
        Example:
            get_interchange_pipeline_graph("/Game/Interchange/Pipelines/MyPipeline")
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "pipeline_path": pipeline_path
            }
            
            logger.info(f"Getting interchange pipeline graph: {params}")
            response = unreal.send_command("get_interchange_pipeline_graph", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Get interchange pipeline graph response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error getting interchange pipeline graph: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def add_interchange_pipeline_function_override(
        ctx: Context,
        pipeline_path: str,
        function_name: str,
        node_position: List[int] = None
    ) -> Dict[str, Any]:
        """
        Add a function override to an Interchange Pipeline Blueprint.
        
        This creates an override for pipeline virtual functions like ExecutePipeline,
        ExecutePostImportPipeline, etc.
        
        Args:
            pipeline_path: UE content path to the Pipeline Blueprint
            function_name: Name of the function to override. Common options:
                          - ExecutePipeline (called at import start)
                          - ExecutePostFactoryPipeline (called after asset creation)
                          - ExecutePostImportPipeline (called after import complete)
                          - CanExecuteOnAnyThread
                          - SetReimportSourceIndex
            node_position: Optional [X, Y] position in the graph
            
        Returns:
            Created function override node information including node_id
            
        Example:
            add_interchange_pipeline_function_override(
                pipeline_path="/Game/Interchange/Pipelines/MyPipeline",
                function_name="ExecutePostImportPipeline"
            )
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            if node_position is None:
                node_position = [0, 0]
            
            params = {
                "pipeline_path": pipeline_path,
                "function_name": function_name,
                "node_position": node_position
            }
            
            logger.info(f"Adding interchange pipeline function override: {params}")
            response = unreal.send_command("add_interchange_pipeline_function_override", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Add function override response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding interchange pipeline function override: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def add_interchange_pipeline_node(
        ctx: Context,
        pipeline_path: str,
        node_type: str,
        function_name: str = "",
        target_class: str = "",
        params: Dict[str, Any] = None,
        node_position: List[int] = None
    ) -> Dict[str, Any]:
        """
        Add a node to an Interchange Pipeline Blueprint's graph.
        
        Supports adding function calls, variable nodes, and Interchange-specific nodes.
        
        Args:
            pipeline_path: UE content path to the Pipeline Blueprint
            node_type: Type of node to add. Options:
                      - FunctionCall: Call a function on a target
                      - IterateNodes: Iterate over Interchange nodes
                      - GetFactoryNode: Get a factory node by type
                      - SetProperty: Set a property on a node
                      - ParentCall: Call parent implementation
                      - Variable: Get/Set a variable
            function_name: Function name (for FunctionCall type)
            target_class: Target class for the function (e.g., "UInterchangeBaseNodeContainer")
            params: Additional parameters for the node
            node_position: Optional [X, Y] position in the graph
            
        Returns:
            Created node information including node_id
            
        Example:
            add_interchange_pipeline_node(
                pipeline_path="/Game/Interchange/Pipelines/MyPipeline",
                node_type="FunctionCall",
                function_name="IterateNodesOfType",
                target_class="UInterchangeBaseNodeContainer"
            )
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            if node_position is None:
                node_position = [0, 0]
            if params is None:
                params = {}
            
            command_params = {
                "pipeline_path": pipeline_path,
                "node_type": node_type,
                "function_name": function_name,
                "target_class": target_class,
                "params": params,
                "node_position": node_position
            }
            
            logger.info(f"Adding interchange pipeline node: {command_params}")
            response = unreal.send_command("add_interchange_pipeline_node", command_params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Add pipeline node response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding interchange pipeline node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def connect_interchange_pipeline_nodes(
        ctx: Context,
        pipeline_path: str,
        source_node_id: str,
        source_pin: str,
        target_node_id: str,
        target_pin: str
    ) -> Dict[str, Any]:
        """
        Connect two nodes in an Interchange Pipeline Blueprint's graph.
        
        Args:
            pipeline_path: UE content path to the Pipeline Blueprint
            source_node_id: ID of the source node (from add_interchange_pipeline_node)
            source_pin: Name of the output pin on the source node
            target_node_id: ID of the target node
            target_pin: Name of the input pin on the target node
            
        Returns:
            Connection result with success status
            
        Example:
            connect_interchange_pipeline_nodes(
                pipeline_path="/Game/Interchange/Pipelines/MyPipeline",
                source_node_id="abc123",
                source_pin="then",
                target_node_id="def456",
                target_pin="execute"
            )
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "pipeline_path": pipeline_path,
                "source_node_id": source_node_id,
                "source_pin": source_pin,
                "target_node_id": target_node_id,
                "target_pin": target_pin
            }
            
            logger.info(f"Connecting interchange pipeline nodes: {params}")
            response = unreal.send_command("connect_interchange_pipeline_nodes", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Connect pipeline nodes response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error connecting interchange pipeline nodes: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def find_interchange_pipeline_nodes(
        ctx: Context,
        pipeline_path: str,
        node_type: str = "",
        function_name: str = ""
    ) -> Dict[str, Any]:
        """
        Find nodes in an Interchange Pipeline Blueprint's graph.
        
        Args:
            pipeline_path: UE content path to the Pipeline Blueprint
            node_type: Optional filter by node type (Event, Function, Variable, etc.)
            function_name: Optional filter by function name
            
        Returns:
            List of matching nodes with their IDs, types, and pin information
            
        Example:
            find_interchange_pipeline_nodes(
                pipeline_path="/Game/Interchange/Pipelines/MyPipeline",
                node_type="Function",
                function_name="ExecutePipeline"
            )
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "pipeline_path": pipeline_path,
                "node_type": node_type,
                "function_name": function_name
            }
            
            logger.info(f"Finding interchange pipeline nodes: {params}")
            response = unreal.send_command("find_interchange_pipeline_nodes", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Find pipeline nodes response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error finding interchange pipeline nodes: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def add_interchange_iterate_nodes_block(
        ctx: Context,
        pipeline_path: str,
        node_class: str,
        graph_name: str = "ExecutePipeline",
        node_position: List[int] = None
    ) -> Dict[str, Any]:
        """
        Add an IterateNodesOfType block to an Interchange Pipeline Blueprint.
        
        This is a common pattern in Interchange Pipelines for processing nodes
        during import. Creates the iterate function call and a lambda/delegate
        for processing each node.
        
        Args:
            pipeline_path: UE content path to the Pipeline Blueprint
            node_class: The Interchange node class to iterate. Common options:
                       - UInterchangeMaterialFactoryNode
                       - UInterchangeTextureFactoryNode
                       - UInterchangeStaticMeshFactoryNode
                       - UInterchangeSkeletalMeshFactoryNode
                       - UInterchangeSceneNode
                       - UInterchangeShaderGraphNode
            graph_name: Name of the function graph to add to (default: ExecutePipeline)
            node_position: Optional [X, Y] position in the graph
            
        Returns:
            Created nodes information including iterate_node_id and lambda_node_id
            
        Example:
            add_interchange_iterate_nodes_block(
                pipeline_path="/Game/Interchange/Pipelines/MyPipeline",
                node_class="UInterchangeMaterialFactoryNode",
                graph_name="ExecutePipeline"
            )
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            if node_position is None:
                node_position = [0, 0]
            
            params = {
                "pipeline_path": pipeline_path,
                "node_class": node_class,
                "graph_name": graph_name,
                "node_position": node_position
            }
            
            logger.info(f"Adding interchange iterate nodes block: {params}")
            response = unreal.send_command("add_interchange_iterate_nodes_block", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Add iterate nodes block response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding interchange iterate nodes block: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def compile_interchange_pipeline(
        ctx: Context,
        pipeline_path: str
    ) -> Dict[str, Any]:
        """
        Compile an Interchange Pipeline Blueprint.
        
        After making changes to the pipeline graph, call this to compile
        and validate the changes.
        
        Args:
            pipeline_path: UE content path to the Pipeline Blueprint
            
        Returns:
            Compilation result with success status and any errors
            
        Example:
            compile_interchange_pipeline("/Game/Interchange/Pipelines/MyPipeline")
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "pipeline_path": pipeline_path
            }
            
            logger.info(f"Compiling interchange pipeline: {params}")
            response = unreal.send_command("compile_interchange_pipeline", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Compile pipeline response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error compiling interchange pipeline: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
