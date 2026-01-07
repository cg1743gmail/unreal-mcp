using System.Text.Json.Nodes;

namespace UnrealMCP.Sidecar;

/// <summary>
/// Complete tool schema definitions for all Unreal MCP proxied tools.
/// Extracted from Python MCP server to ensure AI agents get detailed parameter info.
/// </summary>
public static class ToolSchemas
{
    /// <summary>
    /// Get detailed tool schemas for all proxied UE commands.
    /// Schema format follows MCP spec: { name, description, inputSchema: { type: "object", properties: {...}, required: [...] } }
    /// </summary>
    public static JsonArray GetDetailedToolSchemas()
    {
        var tools = new JsonArray();

        // ==================== Editor Tools ====================
        tools.Add(MakeTool(
            "get_actors_in_level",
            "Get a list of all actors in the current level",
            new JsonObject(), // No parameters
            new JsonArray()
        ));

        tools.Add(MakeTool(
            "find_actors_by_name",
            "Find actors by name pattern",
            new JsonObject
            {
                ["pattern"] = new JsonObject
                {
                    ["type"] = "string",
                    ["description"] = "Name pattern to search for (supports wildcards)"
                }
            },
            new JsonArray { "pattern" }
        ));

        tools.Add(MakeTool(
            "spawn_actor",
            "Create a new actor in the current level",
            new JsonObject
            {
                ["name"] = new JsonObject
                {
                    ["type"] = "string",
                    ["description"] = "The name to give the new actor (must be unique)"
                },
                ["type"] = new JsonObject
                {
                    ["type"] = "string",
                    ["description"] = "The type of actor to create (e.g. StaticMeshActor, PointLight)"
                },
                ["location"] = new JsonObject
                {
                    ["type"] = "array",
                    ["description"] = "The [x, y, z] world location to spawn at",
                    ["items"] = new JsonObject { ["type"] = "number" },
                    ["minItems"] = 3,
                    ["maxItems"] = 3,
                    ["default"] = new JsonArray { 0.0, 0.0, 0.0 }
                },
                ["rotation"] = new JsonObject
                {
                    ["type"] = "array",
                    ["description"] = "The [pitch, yaw, roll] rotation in degrees",
                    ["items"] = new JsonObject { ["type"] = "number" },
                    ["minItems"] = 3,
                    ["maxItems"] = 3,
                    ["default"] = new JsonArray { 0.0, 0.0, 0.0 }
                },
                ["scale"] = new JsonObject
                {
                    ["type"] = "array",
                    ["description"] = "The [x, y, z] scale factors",
                    ["items"] = new JsonObject { ["type"] = "number" },
                    ["minItems"] = 3,
                    ["maxItems"] = 3,
                    ["default"] = new JsonArray { 1.0, 1.0, 1.0 }
                }
            },
            new JsonArray { "name", "type" }
        ));

        tools.Add(MakeTool(
            "create_actor",
            "Create a new actor in the current level (deprecated alias of spawn_actor)",
            new JsonObject
            {
                ["name"] = new JsonObject
                {
                    ["type"] = "string",
                    ["description"] = "The name to give the new actor (must be unique)"
                },
                ["type"] = new JsonObject
                {
                    ["type"] = "string",
                    ["description"] = "The type of actor to create (e.g. StaticMeshActor, PointLight)"
                },
                ["location"] = new JsonObject
                {
                    ["type"] = "array",
                    ["description"] = "The [x, y, z] world location to spawn at",
                    ["items"] = new JsonObject { ["type"] = "number" },
                    ["minItems"] = 3,
                    ["maxItems"] = 3,
                    ["default"] = new JsonArray { 0.0, 0.0, 0.0 }
                },
                ["rotation"] = new JsonObject
                {
                    ["type"] = "array",
                    ["description"] = "The [pitch, yaw, roll] rotation in degrees",
                    ["items"] = new JsonObject { ["type"] = "number" },
                    ["minItems"] = 3,
                    ["maxItems"] = 3,
                    ["default"] = new JsonArray { 0.0, 0.0, 0.0 }
                },
                ["scale"] = new JsonObject
                {
                    ["type"] = "array",
                    ["description"] = "The [x, y, z] scale factors",
                    ["items"] = new JsonObject { ["type"] = "number" },
                    ["minItems"] = 3,
                    ["maxItems"] = 3,
                    ["default"] = new JsonArray { 1.0, 1.0, 1.0 }
                }
            },
            new JsonArray { "name", "type" }
        ));

        tools.Add(MakeTool(
            "delete_actor",
            "Remove an actor from the current level",
            new JsonObject
            {
                ["name"] = new JsonObject
                {
                    ["type"] = "string",
                    ["description"] = "Name of the actor to delete"
                }
            },
            new JsonArray { "name" }
        ));

        tools.Add(MakeTool(
            "set_actor_transform",
            "Modify an actor's transform (location, rotation, scale)",
            new JsonObject
            {
                ["name"] = new JsonObject { ["type"] = "string", ["description"] = "Actor name" },
                ["location"] = new JsonObject
                {
                    ["type"] = "array",
                    ["description"] = "[X, Y, Z] world location",
                    ["items"] = new JsonObject { ["type"] = "number" },
                    ["minItems"] = 3,
                    ["maxItems"] = 3
                },
                ["rotation"] = new JsonObject
                {
                    ["type"] = "array",
                    ["description"] = "[Pitch, Yaw, Roll] rotation in degrees",
                    ["items"] = new JsonObject { ["type"] = "number" },
                    ["minItems"] = 3,
                    ["maxItems"] = 3
                },
                ["scale"] = new JsonObject
                {
                    ["type"] = "array",
                    ["description"] = "[X, Y, Z] scale factors",
                    ["items"] = new JsonObject { ["type"] = "number" },
                    ["minItems"] = 3,
                    ["maxItems"] = 3
                }
            },
            new JsonArray { "name" }
        ));

        tools.Add(MakeTool(
            "get_actor_properties",
            "Get properties of a specific actor",
            new JsonObject
            {
                ["name"] = new JsonObject { ["type"] = "string", ["description"] = "Actor name" }
            },
            new JsonArray { "name" }
        ));

        tools.Add(MakeTool(
            "set_actor_property",
            "Set a property on an actor",
            new JsonObject
            {
                ["name"] = new JsonObject { ["type"] = "string", ["description"] = "Actor name" },
                ["property_name"] = new JsonObject { ["type"] = "string", ["description"] = "Property name (e.g., bHidden, ActorLabel)" },
                ["property_value"] = new JsonObject { ["description"] = "Property value (type depends on property)" }
            },
            new JsonArray { "name", "property_name", "property_value" }
        ));

        tools.Add(MakeTool(
            "focus_viewport",
            "Focus the viewport on a specific target",
            new JsonObject
            {
                ["target"] = new JsonObject { ["type"] = "string", ["description"] = "Actor name or 'all' for all actors" },
                ["location"] = new JsonObject { ["type"] = "array", ["description"] = "[X, Y, Z] camera location", ["items"] = new JsonObject { ["type"] = "number" } },
                ["distance"] = new JsonObject { ["type"] = "number", ["description"] = "Distance from target" },
                ["orientation"] = new JsonObject { ["type"] = "array", ["description"] = "[Pitch, Yaw, Roll] camera rotation", ["items"] = new JsonObject { ["type"] = "number" } }
            },
            new JsonArray()
        ));

        tools.Add(MakeTool(
            "take_screenshot",
            "Capture a screenshot of the viewport",
            new JsonObject
            {
                ["filename"] = new JsonObject { ["type"] = "string", ["description"] = "Output filename (relative to project Saved/Screenshots)" },
                ["show_ui"] = new JsonObject { ["type"] = "boolean", ["description"] = "Include editor UI in screenshot", ["default"] = false },
                ["resolution"] = new JsonObject { ["type"] = "array", ["description"] = "[Width, Height] screenshot resolution", ["items"] = new JsonObject { ["type"] = "integer" } }
            },
            new JsonArray()
        ));

        // ==================== Blueprint Tools ====================
        tools.Add(MakeTool(
            "create_blueprint",
            "Create a new Blueprint class",
            new JsonObject
            {
                ["name"] = new JsonObject { ["type"] = "string", ["description"] = "Blueprint name" },
                ["parent_class"] = new JsonObject { ["type"] = "string", ["description"] = "Parent class (e.g., Actor, Pawn, Character)" }
            },
            new JsonArray { "name", "parent_class" }
        ));

        tools.Add(MakeTool(
            "add_component_to_blueprint",
            "Add a component to a Blueprint",
            new JsonObject
            {
                ["blueprint_name"] = new JsonObject { ["type"] = "string", ["description"] = "Target Blueprint name" },
                ["component_type"] = new JsonObject { ["type"] = "string", ["description"] = "Component type (use class name without U prefix, e.g., StaticMeshComponent, CameraComponent)" },
                ["component_name"] = new JsonObject { ["type"] = "string", ["description"] = "Name for the new component" },
                ["location"] = new JsonObject { ["type"] = "array", ["description"] = "[X, Y, Z] relative position", ["items"] = new JsonObject { ["type"] = "number" }, ["default"] = new JsonArray { 0.0, 0.0, 0.0 } },
                ["rotation"] = new JsonObject { ["type"] = "array", ["description"] = "[Pitch, Yaw, Roll] relative rotation", ["items"] = new JsonObject { ["type"] = "number" }, ["default"] = new JsonArray { 0.0, 0.0, 0.0 } },
                ["scale"] = new JsonObject { ["type"] = "array", ["description"] = "[X, Y, Z] scale factors", ["items"] = new JsonObject { ["type"] = "number" }, ["default"] = new JsonArray { 1.0, 1.0, 1.0 } },
                ["component_properties"] = new JsonObject { ["type"] = "object", ["description"] = "Additional properties to set on the component", ["default"] = new JsonObject() }
            },
            new JsonArray { "blueprint_name", "component_type", "component_name" }
        ));

        tools.Add(MakeTool(
            "set_static_mesh_properties",
            "Set static mesh properties on a StaticMeshComponent",
            new JsonObject
            {
                ["blueprint_name"] = new JsonObject { ["type"] = "string", ["description"] = "Target Blueprint name" },
                ["component_name"] = new JsonObject { ["type"] = "string", ["description"] = "StaticMeshComponent name" },
                ["static_mesh"] = new JsonObject { ["type"] = "string", ["description"] = "Path to static mesh asset (e.g., /Engine/BasicShapes/Cube.Cube)", ["default"] = "/Engine/BasicShapes/Cube.Cube" }
            },
            new JsonArray { "blueprint_name", "component_name" }
        ));

        tools.Add(MakeTool(
            "set_component_property",
            "Set a property on a Blueprint component",
            new JsonObject
            {
                ["blueprint_name"] = new JsonObject { ["type"] = "string", ["description"] = "Target Blueprint name" },
                ["component_name"] = new JsonObject { ["type"] = "string", ["description"] = "Component name" },
                ["property_name"] = new JsonObject { ["type"] = "string", ["description"] = "Property name (e.g., bSimulatePhysics, Mass)" },
                ["property_value"] = new JsonObject { ["description"] = "Property value (type depends on property)" }
            },
            new JsonArray { "blueprint_name", "component_name", "property_name", "property_value" }
        ));

        tools.Add(MakeTool(
            "set_physics_properties",
            "Set physics properties on a component (convenience wrapper)",
            new JsonObject
            {
                ["blueprint_name"] = new JsonObject { ["type"] = "string", ["description"] = "Target Blueprint name" },
                ["component_name"] = new JsonObject { ["type"] = "string", ["description"] = "Component name" },
                ["simulate_physics"] = new JsonObject { ["type"] = "boolean", ["description"] = "Enable physics simulation", ["default"] = true },
                ["gravity_enabled"] = new JsonObject { ["type"] = "boolean", ["description"] = "Enable gravity", ["default"] = true },
                ["mass"] = new JsonObject { ["type"] = "number", ["description"] = "Mass in kilograms", ["default"] = 1.0 },
                ["linear_damping"] = new JsonObject { ["type"] = "number", ["description"] = "Linear damping factor", ["default"] = 0.01 },
                ["angular_damping"] = new JsonObject { ["type"] = "number", ["description"] = "Angular damping factor", ["default"] = 0.0 }
            },
            new JsonArray { "blueprint_name", "component_name" }
        ));

        tools.Add(MakeTool(
            "compile_blueprint",
            "Compile a Blueprint (required after graph changes)",
            new JsonObject
            {
                ["blueprint_name"] = new JsonObject { ["type"] = "string", ["description"] = "Blueprint name" }
            },
            new JsonArray { "blueprint_name" }
        ));

        tools.Add(MakeTool(
            "set_blueprint_property",
            "Set a property on a Blueprint class default object",
            new JsonObject
            {
                ["blueprint_name"] = new JsonObject { ["type"] = "string", ["description"] = "Blueprint name" },
                ["blueprint_path"] = new JsonObject { ["type"] = "string", ["description"] = "Optional canonical asset path for disambiguation (e.g., /Game/Foo/BP_Bar)" },
                ["property_name"] = new JsonObject { ["type"] = "string", ["description"] = "Property name" },
                ["property_value"] = new JsonObject { ["description"] = "Property value" }
            },
            new JsonArray { "blueprint_name", "property_name", "property_value" }
        ));

        tools.Add(MakeTool(
            "get_blueprint_property",
            "Get a property from a Blueprint class default object (CDO)",
            new JsonObject
            {
                ["blueprint_name"] = new JsonObject { ["type"] = "string", ["description"] = "Blueprint name" },
                ["blueprint_path"] = new JsonObject { ["type"] = "string", ["description"] = "Optional canonical asset path for disambiguation (e.g., /Game/Foo/BP_Bar)" },
                ["property_name"] = new JsonObject { ["type"] = "string", ["description"] = "Property name" }
            },
            new JsonArray { "blueprint_name", "property_name" }
        ));

        tools.Add(MakeTool(
            "list_blueprint_components",
            "List SCS components in a Blueprint (name + class)",
            new JsonObject
            {
                ["blueprint_name"] = new JsonObject { ["type"] = "string", ["description"] = "Blueprint name" },
                ["blueprint_path"] = new JsonObject { ["type"] = "string", ["description"] = "Optional canonical asset path for disambiguation (e.g., /Game/Foo/BP_Bar)" }
            },
            new JsonArray { "blueprint_name" }
        ));

        tools.Add(MakeTool(
            "get_component_property",
            "Get a property from a Blueprint component template (SCS)",
            new JsonObject
            {
                ["blueprint_name"] = new JsonObject { ["type"] = "string", ["description"] = "Blueprint name" },
                ["blueprint_path"] = new JsonObject { ["type"] = "string", ["description"] = "Optional canonical asset path for disambiguation (e.g., /Game/Foo/BP_Bar)" },
                ["component_name"] = new JsonObject { ["type"] = "string", ["description"] = "Component name in Blueprint (e.g., Mesh, RotatingMovement)" },
                ["property_name"] = new JsonObject { ["type"] = "string", ["description"] = "Property name on the component" }
            },
            new JsonArray { "blueprint_name", "component_name", "property_name" }
        ));


        tools.Add(MakeTool(
            "set_pawn_properties",
            "Set common Pawn properties on a Blueprint (utility wrapper)",
            new JsonObject
            {
                ["blueprint_name"] = new JsonObject { ["type"] = "string", ["description"] = "Target Pawn/Character Blueprint name" },
                ["auto_possess_player"] = new JsonObject { ["type"] = "string", ["description"] = "Auto possess player setting (e.g., Player0, Disabled)", ["default"] = "" },
                ["use_controller_rotation_yaw"] = new JsonObject { ["type"] = "boolean", ["description"] = "Whether to use controller yaw rotation" },
                ["use_controller_rotation_pitch"] = new JsonObject { ["type"] = "boolean", ["description"] = "Whether to use controller pitch rotation" },
                ["use_controller_rotation_roll"] = new JsonObject { ["type"] = "boolean", ["description"] = "Whether to use controller roll rotation" },
                ["can_be_damaged"] = new JsonObject { ["type"] = "boolean", ["description"] = "Whether the pawn can be damaged" }
            },
            new JsonArray { "blueprint_name" }
        ));

        tools.Add(MakeTool(
            "spawn_blueprint_actor",
            "Spawn an instance of a Blueprint into the level",
            new JsonObject
            {
                ["blueprint_name"] = new JsonObject { ["type"] = "string", ["description"] = "Blueprint to instantiate" },
                ["actor_name"] = new JsonObject { ["type"] = "string", ["description"] = "Name for the spawned actor instance" },
                ["location"] = new JsonObject { ["type"] = "array", ["description"] = "[X, Y, Z] spawn location", ["items"] = new JsonObject { ["type"] = "number" }, ["default"] = new JsonArray { 0.0, 0.0, 0.0 } },
                ["rotation"] = new JsonObject { ["type"] = "array", ["description"] = "[Pitch, Yaw, Roll] spawn rotation", ["items"] = new JsonObject { ["type"] = "number" }, ["default"] = new JsonArray { 0.0, 0.0, 0.0 } }
            },
            new JsonArray { "blueprint_name", "actor_name" }
        ));

        // ==================== Blueprint Node Tools ====================
        tools.Add(MakeTool(
            "add_blueprint_event_node",
            "Add an event node to a Blueprint's event graph",
            new JsonObject
            {
                ["blueprint_name"] = new JsonObject { ["type"] = "string", ["description"] = "Blueprint name" },
                ["event_name"] = new JsonObject { ["type"] = "string", ["description"] = "Event name (e.g., ReceiveBeginPlay, ReceiveTick)" },
                ["node_position"] = new JsonObject { ["type"] = "array", ["description"] = "[X, Y] position in graph", ["items"] = new JsonObject { ["type"] = "number" }, ["default"] = new JsonArray { 0, 0 } }
            },
            new JsonArray { "blueprint_name", "event_name" }
        ));

        tools.Add(MakeTool(
            "add_blueprint_input_action_node",
            "Add an input action event node to a Blueprint's event graph",
            new JsonObject
            {
                ["blueprint_name"] = new JsonObject { ["type"] = "string", ["description"] = "Blueprint name" },
                ["action_name"] = new JsonObject { ["type"] = "string", ["description"] = "Input action name (must exist in project settings)" },
                ["node_position"] = new JsonObject { ["type"] = "array", ["description"] = "[X, Y] position in graph", ["items"] = new JsonObject { ["type"] = "number" }, ["default"] = new JsonArray { 0, 0 } }
            },
            new JsonArray { "blueprint_name", "action_name" }
        ));

        tools.Add(MakeTool(
            "add_blueprint_function_node",
            "Add a function call node to a Blueprint's event graph",
            new JsonObject
            {
                ["blueprint_name"] = new JsonObject { ["type"] = "string", ["description"] = "Blueprint name" },
                ["target"] = new JsonObject { ["type"] = "string", ["description"] = "Target object (component name, variable name, or 'self')" },
                ["function_name"] = new JsonObject { ["type"] = "string", ["description"] = "Function to call (e.g., SetActorLocation, AddImpulse)" },
                ["node_position"] = new JsonObject { ["type"] = "array", ["description"] = "[X, Y] position in graph", ["items"] = new JsonObject { ["type"] = "number" }, ["default"] = new JsonArray { 0, 0 } }
            },
            new JsonArray { "blueprint_name", "target", "function_name" }
        ));

        tools.Add(MakeTool(
            "connect_blueprint_nodes",
            "Connect two Blueprint nodes via their pins",
            new JsonObject
            {
                ["blueprint_name"] = new JsonObject { ["type"] = "string", ["description"] = "Blueprint name" },
                ["source_node_id"] = new JsonObject { ["type"] = "string", ["description"] = "Source node ID (returned by add_* functions)" },
                ["source_pin"] = new JsonObject { ["type"] = "string", ["description"] = "Source pin name (e.g., 'then', 'Pressed')" },
                ["target_node_id"] = new JsonObject { ["type"] = "string", ["description"] = "Target node ID" },
                ["target_pin"] = new JsonObject { ["type"] = "string", ["description"] = "Target pin name (e.g., 'execute', 'NewLocation')" }
            },
            new JsonArray { "blueprint_name", "source_node_id", "source_pin", "target_node_id", "target_pin" }
        ));

        tools.Add(MakeTool(
            "add_blueprint_variable",
            "Add a variable to a Blueprint",
            new JsonObject
            {
                ["blueprint_name"] = new JsonObject { ["type"] = "string", ["description"] = "Blueprint name" },
                ["variable_name"] = new JsonObject { ["type"] = "string", ["description"] = "Variable name" },
                ["variable_type"] = new JsonObject { ["type"] = "string", ["description"] = "Variable type (e.g., Float, Vector, Boolean, Object)" }
            },
            new JsonArray { "blueprint_name", "variable_name", "variable_type" }
        ));

        tools.Add(MakeTool(
            "add_blueprint_get_self_component_reference",
            "Add a 'Get Component' reference node to access a Blueprint component",
            new JsonObject
            {
                ["blueprint_name"] = new JsonObject { ["type"] = "string", ["description"] = "Blueprint name" },
                ["component_name"] = new JsonObject { ["type"] = "string", ["description"] = "Component name to reference" },
                ["node_position"] = new JsonObject { ["type"] = "array", ["description"] = "[X, Y] position in graph", ["items"] = new JsonObject { ["type"] = "number" }, ["default"] = new JsonArray { 0, 0 } }
            },
            new JsonArray { "blueprint_name", "component_name" }
        ));

        tools.Add(MakeTool(
            "add_blueprint_get_component_node",
            "Alias of add_blueprint_get_self_component_reference (compat)",
            new JsonObject
            {
                ["blueprint_name"] = new JsonObject { ["type"] = "string", ["description"] = "Blueprint name" },
                ["component_name"] = new JsonObject { ["type"] = "string", ["description"] = "Component name to reference" },
                ["node_position"] = new JsonObject { ["type"] = "array", ["description"] = "[X, Y] position in graph", ["items"] = new JsonObject { ["type"] = "number" }, ["default"] = new JsonArray { 0, 0 } }
            },
            new JsonArray { "blueprint_name", "component_name" }
        ));

        tools.Add(MakeTool(
            "add_blueprint_self_reference",
            "Add a 'Self' reference node to access the Blueprint instance",
            new JsonObject
            {
                ["blueprint_name"] = new JsonObject { ["type"] = "string", ["description"] = "Blueprint name" },
                ["node_position"] = new JsonObject { ["type"] = "array", ["description"] = "[X, Y] position in graph", ["items"] = new JsonObject { ["type"] = "number" }, ["default"] = new JsonArray { 0, 0 } }
            },
            new JsonArray { "blueprint_name" }
        ));

        tools.Add(MakeTool(
            "find_blueprint_nodes",
            "Find nodes in a Blueprint event graph",
            new JsonObject
            {
                ["blueprint_name"] = new JsonObject { ["type"] = "string", ["description"] = "Blueprint name" },
                ["node_type"] = new JsonObject { ["type"] = "string", ["description"] = "Node type to search for (optional)" },
                ["event_type"] = new JsonObject { ["type"] = "string", ["description"] = "Event type to search for (optional)" }
            },
            new JsonArray { "blueprint_name" }
        ));

        // ==================== Construction Script Tools ====================
        tools.Add(MakeTool(
            "get_construction_script_graph",
            "Get the Construction Script graph of an Actor-based Blueprint. Returns all nodes, entry node ID, and pin information. Construction Script runs in editor when properties change.",
            new JsonObject
            {
                ["blueprint_name"] = new JsonObject { ["type"] = "string", ["description"] = "Blueprint name" },
                ["blueprint_path"] = new JsonObject { ["type"] = "string", ["description"] = "Optional: full asset path for disambiguation" }
            },
            new JsonArray { "blueprint_name" }
        ));

        tools.Add(MakeTool(
            "add_construction_script_node",
            "Add a node to the Construction Script graph. Supported node_type: FunctionCall, VariableGet, VariableSet, Self, GetComponent. Use connect_blueprint_nodes to wire nodes.",
            new JsonObject
            {
                ["blueprint_name"] = new JsonObject { ["type"] = "string", ["description"] = "Blueprint name" },
                ["node_type"] = new JsonObject { ["type"] = "string", ["description"] = "Node type: FunctionCall, VariableGet, VariableSet, Self, GetComponent" },
                ["function_name"] = new JsonObject { ["type"] = "string", ["description"] = "For FunctionCall: function name (e.g., SetRelativeLocation)" },
                ["target"] = new JsonObject { ["type"] = "string", ["description"] = "For FunctionCall: target class (e.g., SceneComponent, Actor)" },
                ["variable_name"] = new JsonObject { ["type"] = "string", ["description"] = "For VariableGet/VariableSet: variable name" },
                ["component_name"] = new JsonObject { ["type"] = "string", ["description"] = "For GetComponent: component name to reference" },
                ["node_position"] = new JsonObject { ["type"] = "array", ["description"] = "[X, Y] position in graph", ["items"] = new JsonObject { ["type"] = "number" }, ["default"] = new JsonArray { 0, 0 } },
                ["blueprint_path"] = new JsonObject { ["type"] = "string", ["description"] = "Optional: full asset path for disambiguation" }
            },
            new JsonArray { "blueprint_name", "node_type" }
        ));

        // ==================== Project Tools ====================
        tools.Add(MakeTool(
            "create_input_mapping",
            "Create an input mapping in project settings",
            new JsonObject
            {
                ["action_name"] = new JsonObject { ["type"] = "string", ["description"] = "Input action name" },
                ["key"] = new JsonObject { ["type"] = "string", ["description"] = "Key name (e.g., SpaceBar, LeftMouseButton, W)" },
                ["input_type"] = new JsonObject { ["type"] = "string", ["description"] = "Input type: 'Action' or 'Axis'", ["enum"] = new JsonArray { "Action", "Axis" } }
            },
            new JsonArray { "action_name", "key", "input_type" }
        ));

        // ==================== UMG Tools ====================
        tools.Add(MakeTool(
            "create_umg_widget_blueprint",
            "Create a new UMG Widget Blueprint",
            new JsonObject
            {
                ["widget_name"] = new JsonObject { ["type"] = "string", ["description"] = "Widget Blueprint name" },
                ["parent_class"] = new JsonObject { ["type"] = "string", ["description"] = "Parent class (default: UserWidget)", ["default"] = "UserWidget" },
                ["path"] = new JsonObject { ["type"] = "string", ["description"] = "Content browser path (default: /Game/UI)", ["default"] = "/Game/UI" }
            },
            new JsonArray { "widget_name" }
        ));

        tools.Add(MakeTool(
            "add_text_block_to_widget",
            "Add a Text Block widget to a UMG Widget Blueprint",
            new JsonObject
            {
                ["widget_name"] = new JsonObject { ["type"] = "string", ["description"] = "Target Widget Blueprint name" },
                ["text_block_name"] = new JsonObject { ["type"] = "string", ["description"] = "Name for the text block" },
                ["text"] = new JsonObject { ["type"] = "string", ["description"] = "Initial text content", ["default"] = "" },
                ["position"] = new JsonObject { ["type"] = "array", ["description"] = "[X, Y] position in canvas", ["items"] = new JsonObject { ["type"] = "number" }, ["default"] = new JsonArray { 0.0, 0.0 } },
                ["size"] = new JsonObject { ["type"] = "array", ["description"] = "[Width, Height]", ["items"] = new JsonObject { ["type"] = "number" }, ["default"] = new JsonArray { 200.0, 50.0 } },
                ["font_size"] = new JsonObject { ["type"] = "integer", ["description"] = "Font size in points", ["default"] = 12 },
                ["color"] = new JsonObject { ["type"] = "array", ["description"] = "[R, G, B, A] color (0.0-1.0)", ["items"] = new JsonObject { ["type"] = "number" }, ["default"] = new JsonArray { 1.0, 1.0, 1.0, 1.0 } }
            },
            new JsonArray { "widget_name", "text_block_name" }
        ));

        tools.Add(MakeTool(
            "add_button_to_widget",
            "Add a Button widget to a UMG Widget Blueprint",
            new JsonObject
            {
                ["widget_name"] = new JsonObject { ["type"] = "string", ["description"] = "Target Widget Blueprint name" },
                ["button_name"] = new JsonObject { ["type"] = "string", ["description"] = "Name for the button" },
                ["text"] = new JsonObject { ["type"] = "string", ["description"] = "Button text", ["default"] = "" },
                ["position"] = new JsonObject { ["type"] = "array", ["description"] = "[X, Y] position", ["items"] = new JsonObject { ["type"] = "number" }, ["default"] = new JsonArray { 0.0, 0.0 } },
                ["size"] = new JsonObject { ["type"] = "array", ["description"] = "[Width, Height]", ["items"] = new JsonObject { ["type"] = "number" }, ["default"] = new JsonArray { 200.0, 50.0 } },
                ["font_size"] = new JsonObject { ["type"] = "integer", ["description"] = "Font size", ["default"] = 12 },
                ["color"] = new JsonObject { ["type"] = "array", ["description"] = "[R, G, B, A] text color", ["items"] = new JsonObject { ["type"] = "number" }, ["default"] = new JsonArray { 1.0, 1.0, 1.0, 1.0 } },
                ["background_color"] = new JsonObject { ["type"] = "array", ["description"] = "[R, G, B, A] background color", ["items"] = new JsonObject { ["type"] = "number" }, ["default"] = new JsonArray { 0.1, 0.1, 0.1, 1.0 } }
            },
            new JsonArray { "widget_name", "button_name" }
        ));

        tools.Add(MakeTool(
            "bind_widget_event",
            "Bind a widget event (e.g., OnClicked) to a function",
            new JsonObject
            {
                ["widget_name"] = new JsonObject { ["type"] = "string", ["description"] = "Widget Blueprint name" },
                ["widget_component_name"] = new JsonObject { ["type"] = "string", ["description"] = "Widget component name (e.g., button name)" },
                ["event_name"] = new JsonObject { ["type"] = "string", ["description"] = "Event name (e.g., OnClicked, OnHovered)" },
                ["function_name"] = new JsonObject { ["type"] = "string", ["description"] = "Function name to call (optional, auto-generated if empty)", ["default"] = "" }
            },
            new JsonArray { "widget_name", "widget_component_name", "event_name" }
        ));

        tools.Add(MakeTool(
            "add_widget_to_viewport",
            "Add a widget instance to the game viewport",
            new JsonObject
            {
                ["widget_name"] = new JsonObject { ["type"] = "string", ["description"] = "Widget Blueprint name" },
                ["z_order"] = new JsonObject { ["type"] = "integer", ["description"] = "Z-order (higher values draw on top)", ["default"] = 0 }
            },
            new JsonArray { "widget_name" }
        ));

        tools.Add(MakeTool(
            "set_text_block_binding",
            "Set up dynamic property binding for a text block",
            new JsonObject
            {
                ["widget_name"] = new JsonObject { ["type"] = "string", ["description"] = "Widget Blueprint name" },
                ["text_block_name"] = new JsonObject { ["type"] = "string", ["description"] = "Text block component name" },
                ["binding_property"] = new JsonObject { ["type"] = "string", ["description"] = "Property/variable name to bind" },
                ["binding_type"] = new JsonObject { ["type"] = "string", ["description"] = "Binding type (default: Text)", ["default"] = "Text" }
            },
            new JsonArray { "widget_name", "text_block_name", "binding_property" }
        ));

        // ==================== Interchange Tools ====================
        tools.Add(MakeTool(
            "import_model",
            "Import a 3D model file using Unreal Interchange system (FBX, glTF, USD, Alembic, OBJ, PLY)",
            new JsonObject
            {
                ["file_path"] = new JsonObject { ["type"] = "string", ["description"] = "Absolute path to source model file" },
                ["destination_path"] = new JsonObject { ["type"] = "string", ["description"] = "UE content path for imported assets", ["default"] = "/Game/Imported" },
                ["import_mesh"] = new JsonObject { ["type"] = "boolean", ["description"] = "Import mesh data", ["default"] = true },
                ["import_material"] = new JsonObject { ["type"] = "boolean", ["description"] = "Import materials", ["default"] = true },
                ["import_texture"] = new JsonObject { ["type"] = "boolean", ["description"] = "Import textures", ["default"] = true },
                ["import_skeleton"] = new JsonObject { ["type"] = "boolean", ["description"] = "Import skeleton for skeletal meshes", ["default"] = true },
                ["create_physics_asset"] = new JsonObject { ["type"] = "boolean", ["description"] = "Create physics asset for skeletal meshes", ["default"] = false }
            },
            new JsonArray { "file_path" }
        ));

        tools.Add(MakeTool(
            "create_interchange_blueprint",
            "Create a Blueprint from an imported mesh asset (auto-detects StaticMesh/SkeletalMesh)",
            new JsonObject
            {
                ["name"] = new JsonObject { ["type"] = "string", ["description"] = "Blueprint name" },
                ["mesh_path"] = new JsonObject { ["type"] = "string", ["description"] = "UE content path to mesh asset (e.g., /Game/Imported/MyMesh)" }
            },
            new JsonArray { "name", "mesh_path" }
        ));

        tools.Add(MakeTool(
            "create_custom_interchange_blueprint",
            "Create a custom Blueprint with flexible parent class and component configuration",
            new JsonObject
            {
                ["name"] = new JsonObject { ["type"] = "string", ["description"] = "Blueprint name" },
                ["package_path"] = new JsonObject { ["type"] = "string", ["description"] = "Package path", ["default"] = "/Game/Blueprints/" },
                ["parent_class"] = new JsonObject { ["type"] = "string", ["description"] = "Parent class name", ["default"] = "Actor" },
                ["mesh_path"] = new JsonObject { ["type"] = "string", ["description"] = "Mesh asset path (optional)", ["default"] = "" },
                ["components"] = new JsonObject { ["type"] = "array", ["description"] = "Array of component definitions", ["items"] = new JsonObject { ["type"] = "object" }, ["default"] = new JsonArray() }
            },
            new JsonArray { "name" }
        ));

        tools.Add(MakeTool(
            "get_interchange_assets",
            "Query imported assets (filter by static_mesh, skeletal_mesh, material, texture)",
            new JsonObject
            {
                ["search_path"] = new JsonObject { ["type"] = "string", ["description"] = "Content path to search", ["default"] = "/Game/" },
                ["asset_type"] = new JsonObject { ["type"] = "string", ["description"] = "Asset type filter (optional): static_mesh, skeletal_mesh, material, texture", ["default"] = "" }
            },
            new JsonArray()
        ));

        tools.Add(MakeTool(
            "reimport_asset",
            "Trigger reimport for existing assets after source file changes",
            new JsonObject
            {
                ["asset_path"] = new JsonObject { ["type"] = "string", ["description"] = "UE content path of asset to reimport" }
            },
            new JsonArray { "asset_path" }
        ));

        tools.Add(MakeTool(
            "get_interchange_info",
            "Get supported formats, engine version, and optional asset metadata",
            new JsonObject
            {
                ["asset_path"] = new JsonObject { ["type"] = "string", ["description"] = "Optional asset path to get metadata for", ["default"] = "" }
            },
            new JsonArray()
        ));

        tools.Add(MakeTool(
            "ping",
            "Ping Unreal MCP plugin",
            new JsonObject(),
            new JsonArray()
        ));

        return tools;
    }

    private static JsonObject MakeTool(string name, string description, JsonObject properties, JsonArray required)
    {
        return new JsonObject
        {
            ["name"] = name,
            ["description"] = description,
            ["inputSchema"] = new JsonObject
            {
                ["type"] = "object",
                ["properties"] = properties,
                ["required"] = required
            }
        };
    }
}
