namespace UnrealMCP.Sidecar;

/// <summary>
/// Comprehensive usage guide for Unreal MCP tools (converted from Python server's info prompt).
/// </summary>
public static class UnrealMcpUsageGuide
{
    public static string GetGuideText() => @"
# Unreal MCP Server Tools and Best Practices

## Interchange Tools (UE 5.5+ Asset Import System)
- `import_model(file_path, destination_path=""/Game/Imported"", import_mesh=True, import_material=True, import_texture=True, import_skeleton=True, create_physics_asset=False)`
  Import 3D model files (FBX, glTF, USD, Alembic, OBJ, PLY)
- `create_interchange_blueprint(name, mesh_path)`
  Create a Blueprint from an imported mesh (auto-detects StaticMesh/SkeletalMesh)
- `create_custom_interchange_blueprint(name, package_path=""/Game/Blueprints/"", parent_class=""Actor"", mesh_path="""", components=[])`
  Create a custom Blueprint with flexible parent class and component configuration
- `get_interchange_assets(search_path=""/Game/"", asset_type="""")`
  Query imported assets (filter by: static_mesh, skeletal_mesh, material, texture)
- `reimport_asset(asset_path)`
  Trigger reimport for existing assets after source file changes
- `get_interchange_info(asset_path="""")`
  Get supported formats, engine version, and optional asset metadata

## UMG (Widget Blueprint) Tools
- `create_umg_widget_blueprint(widget_name, parent_class=""UserWidget"", path=""/Game/UI"")` 
  Create a new UMG Widget Blueprint
- `add_text_block_to_widget(widget_name, text_block_name, text="""", position=[0,0], size=[200,50], font_size=12, color=[1,1,1,1])`
  Add a Text Block widget with customizable properties
- `add_button_to_widget(widget_name, button_name, text="""", position=[0,0], size=[200,50], font_size=12, color=[1,1,1,1], background_color=[0.1,0.1,0.1,1])`
  Add a Button widget with text and styling
- `bind_widget_event(widget_name, widget_component_name, event_name, function_name="""")`
  Bind events like OnClicked to functions
- `add_widget_to_viewport(widget_name, z_order=0)`
  Add widget instance to game viewport
- `set_text_block_binding(widget_name, text_block_name, binding_property, binding_type=""Text"")`
  Set up dynamic property binding for text blocks

## Editor Tools
### Viewport and Screenshots
- `focus_viewport(target, location, distance, orientation)` - Focus viewport
- `take_screenshot(filename, show_ui, resolution)` - Capture screenshots

### Actor Management
- `get_actors_in_level()` - List all actors in current level
- `find_actors_by_name(pattern)` - Find actors by name pattern
- `spawn_actor(name, type, location=[0,0,0], rotation=[0,0,0])` - Create actors
- `delete_actor(name)` - Remove actors
- `set_actor_transform(name, location, rotation, scale)` - Modify actor transform
- `get_actor_properties(name)` - Get actor properties

## Blueprint Management
- `create_blueprint(name, parent_class)` - Create new Blueprint classes
- `add_component_to_blueprint(blueprint_name, component_type, component_name)` - Add components
- `set_static_mesh_properties(blueprint_name, component_name, static_mesh)` - Configure meshes
- `set_physics_properties(blueprint_name, component_name)` - Configure physics
- `compile_blueprint(blueprint_name)` - Compile Blueprint changes
- `set_blueprint_property(blueprint_name, property_name, property_value)` - Set properties
- `spawn_blueprint_actor(blueprint_name, actor_name)` - Spawn Blueprint actors

## Blueprint Node Management
- `add_blueprint_event_node(blueprint_name, event_name)` - Add event nodes (e.g., ReceiveBeginPlay, ReceiveTick)
- `add_blueprint_input_action_node(blueprint_name, action_name)` - Add input nodes
- `add_blueprint_function_node(blueprint_name, target, function_name)` - Add function nodes
- `connect_blueprint_nodes(blueprint_name, source_node_id, source_pin, target_node_id, target_pin)` - Connect nodes
- `add_blueprint_variable(blueprint_name, variable_name, variable_type)` - Add variables
- `add_blueprint_get_self_component_reference(blueprint_name, component_name)` - Add component refs
- `add_blueprint_self_reference(blueprint_name)` - Add self references
- `find_blueprint_nodes(blueprint_name, node_type, event_type)` - Find nodes

## Project Tools
- `create_input_mapping(action_name, key, input_type)` - Create input mappings

## Best Practices

### Interchange Asset Workflow
- Use `get_interchange_info()` to check supported formats before importing
- Import models with `import_model()` specifying appropriate destination paths
- Query imported assets with `get_interchange_assets()` to verify import success
- Create Blueprints from meshes using `create_interchange_blueprint()` for simple cases
- Use `create_custom_interchange_blueprint()` for complex setups with custom parent classes
- Reimport assets with `reimport_asset()` after modifying source files
- Organize imported assets in logical folder structures (/Game/Imported/, /Game/Characters/, etc.)

### UMG Widget Development
- Create widgets with descriptive names that reflect their purpose
- Use consistent naming conventions for widget components
- Organize widget hierarchy logically
- Set appropriate anchors and alignment for responsive layouts
- Use property bindings for dynamic updates instead of direct setting
- Handle widget events appropriately with meaningful function names
- Clean up widgets when no longer needed
- Test widget layouts at different resolutions

### Editor and Actor Management
- Use unique names for actors to avoid conflicts
- Clean up temporary actors
- Validate transforms before applying
- Check actor existence before modifications
- Take regular viewport screenshots during development
- Keep the viewport focused on relevant actors during operations

### Blueprint Development
- Compile Blueprints after changes (ALWAYS call compile_blueprint after graph edits)
- Use meaningful names for variables and functions
- Organize nodes logically
- Test functionality in isolation
- Consider performance implications
- Document complex setups

### Blueprint Node Workflow
1. Add event nodes first (e.g., ReceiveBeginPlay)
2. Add necessary component/variable reference nodes
3. Add function call nodes
4. Connect nodes from event → function calls
5. Always compile the Blueprint after graph changes

### Error Handling
- Check command responses for success field
- Handle errors gracefully
- Log important operations
- Validate parameters before calling
- Clean up resources on errors
";
}
