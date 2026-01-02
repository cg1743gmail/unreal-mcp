"""
Test script for creating an Interchange Pipeline Blueprint with node connections.
This demonstrates MCP's ability to:
1. Create Pipeline Blueprint
2. Add function override (ExecutePipeline)
3. Add nodes (FunctionCall, ParentCall)
4. Connect nodes together
5. Compile the Blueprint
"""

import socket
import json
import time
import random

# Configuration
UE_HOST = '127.0.0.1'
UE_PORT = 55557
TIMEOUT = 10

# Use unique name to avoid conflicts with existing blueprints
PIPELINE_NAME = f"BP_TestNodeConn_{random.randint(1000, 9999)}"
PIPELINE_PATH = "/Game/Interchange/Pipelines/"
OVERRIDE_FUNCTION = "ScriptedExecutePostImportPipeline"  # Correct function name for Interchange Pipeline


def send_mcp_command(command_type: str, params: dict) -> dict:
    """Send command to UE MCP Bridge and return response."""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(TIMEOUT)
        sock.connect((UE_HOST, UE_PORT))
        
        message = json.dumps({
            "type": command_type,
            "params": params
        })
        
        sock.sendall(message.encode('utf-8'))
        
        # Receive response
        response_data = b""
        while True:
            try:
                chunk = sock.recv(8192)
                if not chunk:
                    break
                response_data += chunk
                # Try to parse JSON to see if complete
                try:
                    json.loads(response_data.decode('utf-8'))
                    break  # Valid JSON, done receiving
                except json.JSONDecodeError:
                    continue  # Keep receiving
            except socket.timeout:
                break
        
        sock.close()
        
        if response_data:
            return json.loads(response_data.decode('utf-8'))
        return {"status": "error", "error": "No response received"}
        
    except socket.timeout:
        return {"status": "error", "error": "Connection timeout"}
    except ConnectionRefusedError:
        return {"status": "error", "error": "Connection refused - is UE running?"}
    except Exception as e:
        return {"status": "error", "error": str(e)}


def get_result(response: dict) -> dict:
    """Extract result from response."""
    if response.get("status") == "success":
        return response.get("result", response)
    return response


def is_success(response: dict) -> bool:
    """Check if response indicates success."""
    return response.get("status") == "success" or response.get("success") == True


def get_error(response: dict) -> str:
    """Extract error message from response."""
    if response.get("status") == "error":
        return response.get("error", "Unknown error")
    return response.get("message") or response.get("error", "Unknown error")


def main():
    print("=" * 60)
    print("Interchange Pipeline Node Connection Test")
    print("=" * 60)
    
    full_path = f"{PIPELINE_PATH}{PIPELINE_NAME}"
    
    # Step 1: Test connection
    print("\n[Step 1] Testing connection to UE Editor...")
    result = send_mcp_command("get_interchange_info", {})
    
    if not is_success(result):
        print(f"✗ Cannot connect: {get_error(result)}")
        return 1
    
    data = get_result(result)
    print(f"✓ Connected to UE {data.get('engine_version', 'Unknown')}")
    
    # Step 2: Create Pipeline Blueprint
    print(f"\n[Step 2] Creating Pipeline Blueprint: {PIPELINE_NAME}")
    result = send_mcp_command("create_interchange_pipeline_blueprint", {
        "name": PIPELINE_NAME,
        "package_path": PIPELINE_PATH,
        "parent_class": "GenericAssetsPipeline"  # Use standard parent for testing
    })
    
    if not is_success(result):
        error = get_error(result)
        if "already exists" in error.lower():
            print(f"  ⚠ Pipeline already exists, continuing...")
        else:
            print(f"  ✗ Failed: {error}")
            return 1
    else:
        data = get_result(result)
        print(f"  ✓ Created: {data.get('path')}")
    
    # Step 3: Get Pipeline Graph Info
    print(f"\n[Step 3] Getting Pipeline graph structure...")
    result = send_mcp_command("get_interchange_pipeline_graph", {
        "pipeline_path": full_path
    })
    
    if is_success(result):
        data = get_result(result)
        print(f"  ✓ Blueprint: {data.get('blueprint_name')}")
        print(f"  ✓ Parent: {data.get('parent_class')}")
        
        overridable = data.get("overridable_functions", [])
        print(f"  ✓ Overridable Functions: {len(overridable)}")
        for func in overridable[:5]:
            print(f"      - {func.get('name')}")
        if len(overridable) > 5:
            print(f"      ... and {len(overridable) - 5} more")
    else:
        print(f"  ⚠ {get_error(result)}")
    
    # Step 4: Add function override - ScriptedExecutePostImportPipeline
    print(f"\n[Step 4] Adding {OVERRIDE_FUNCTION} function override...")
    result = send_mcp_command("add_interchange_pipeline_function_override", {
        "pipeline_path": full_path,
        "function_name": OVERRIDE_FUNCTION
    })
    
    entry_node_id = None
    if is_success(result):
        data = get_result(result)
        print(f"  ✓ Function: {data.get('function_name')}")
        print(f"  ✓ Graph: {data.get('graph_name')}")
        entry_node_id = data.get("entry_node_id")
        print(f"  ✓ Entry Node ID: {entry_node_id}")
        
        pins = data.get("entry_pins", [])
        if pins:
            print(f"  ✓ Entry Pins: {len(pins)}")
            for pin in pins[:5]:
                print(f"      - {pin.get('name')} ({pin.get('type')})")
    else:
        print(f"  ⚠ {get_error(result)}")
    
    # Step 5: Add a CallParentFunction node
    print(f"\n[Step 5] Adding CallParentFunction node...")
    result = send_mcp_command("add_interchange_pipeline_node", {
        "pipeline_path": full_path,
        "graph_name": OVERRIDE_FUNCTION,
        "node_type": "ParentCall",
        "function_name": OVERRIDE_FUNCTION,
        "position_x": 400,
        "position_y": 0
    })
    
    parent_call_node_id = None
    if is_success(result):
        data = get_result(result)
        parent_call_node_id = data.get("node_id")
        print(f"  ✓ Node ID: {parent_call_node_id}")
        print(f"  ✓ Node Class: {data.get('node_class')}")
        
        pins = data.get("pins", [])
        if pins:
            print(f"  ✓ Pins: {len(pins)}")
            for pin in pins[:6]:
                direction = "→" if pin.get("direction") == "Output" else "←"
                print(f"      {direction} {pin.get('name')} ({pin.get('type')})")
    else:
        print(f"  ⚠ {get_error(result)}")
    
    # Step 6: Add a FunctionCall node (PrintString for demo)
    print(f"\n[Step 6] Adding PrintString FunctionCall node...")
    result = send_mcp_command("add_interchange_pipeline_node", {
        "pipeline_path": full_path,
        "graph_name": OVERRIDE_FUNCTION,
        "node_type": "FunctionCall",
        "function_name": "PrintString",
        "function_class": "KismetSystemLibrary",
        "position_x": 700,
        "position_y": 0
    })
    
    print_node_id = None
    if is_success(result):
        data = get_result(result)
        print_node_id = data.get("node_id")
        print(f"  ✓ Node ID: {print_node_id}")
        print(f"  ✓ Node Class: {data.get('node_class')}")
        
        pins = data.get("pins", [])
        if pins:
            print(f"  ✓ Pins: {len(pins)}")
            for pin in pins[:8]:
                direction = "→" if pin.get("direction") == "Output" else "←"
                print(f"      {direction} {pin.get('name')} ({pin.get('type')})")
    else:
        print(f"  ⚠ {get_error(result)}")
    
    # Step 7: Connect Entry node to ParentCall
    if entry_node_id and parent_call_node_id:
        print(f"\n[Step 7] Connecting Entry → ParentCall (execution flow)...")
        result = send_mcp_command("connect_interchange_pipeline_nodes", {
            "pipeline_path": full_path,
            "graph_name": OVERRIDE_FUNCTION,
            "source_node_id": entry_node_id,
            "source_pin": "then",  # Execution output pin
            "target_node_id": parent_call_node_id,
            "target_pin": "execute"  # Execution input pin
        })
        
        if is_success(result):
            data = get_result(result)
            print(f"  ✓ Connected: {data.get('source_pin')} → {data.get('target_pin')}")
        else:
            print(f"  ⚠ {get_error(result)}")
    
    # Step 8: Connect ParentCall to PrintString
    if parent_call_node_id and print_node_id:
        print(f"\n[Step 8] Connecting ParentCall → PrintString (execution flow)...")
        result = send_mcp_command("connect_interchange_pipeline_nodes", {
            "pipeline_path": full_path,
            "graph_name": OVERRIDE_FUNCTION,
            "source_node_id": parent_call_node_id,
            "source_pin": "then",
            "target_node_id": print_node_id,
            "target_pin": "execute"
        })
        
        if is_success(result):
            data = get_result(result)
            print(f"  ✓ Connected: {data.get('source_pin')} → {data.get('target_pin')}")
        else:
            print(f"  ⚠ {get_error(result)}")
    
    # Step 9: Find all nodes in the graph
    print(f"\n[Step 9] Finding all nodes in {OVERRIDE_FUNCTION} graph...")
    result = send_mcp_command("find_interchange_pipeline_nodes", {
        "pipeline_path": full_path,
        "graph_name": OVERRIDE_FUNCTION
    })
    
    if is_success(result):
        data = get_result(result)
        nodes = data.get("nodes", [])
        print(f"  ✓ Found {len(nodes)} nodes:")
        for node in nodes:
            print(f"      - [{node.get('node_id')[:8]}...] {node.get('node_class')} @ ({node.get('position_x')}, {node.get('position_y')})")
    else:
        print(f"  ⚠ {get_error(result)}")
    
    # Step 10: Compile the Blueprint
    print(f"\n[Step 10] Compiling Pipeline Blueprint...")
    result = send_mcp_command("compile_interchange_pipeline", {
        "pipeline_path": full_path
    })
    
    if is_success(result):
        data = get_result(result)
        print(f"  ✓ Status: {data.get('status', 'OK')}")
        if data.get("warnings"):
            print(f"  ⚠ Warnings: {data.get('warnings')}")
    else:
        print(f"  ⚠ {get_error(result)}")
    
    # Summary
    print("\n" + "=" * 60)
    print("Test Complete!")
    print("=" * 60)
    print(f"\nPipeline created at: {full_path}")
    print("\nNode structure:")
    print("  [Entry] ExecutePipeline")
    print("     ↓ (then → execute)")
    print("  [ParentCall] Super::ExecutePipeline")
    print("     ↓ (then → execute)")
    print("  [FunctionCall] PrintString")
    print("\nOpen the Blueprint in UE Editor to verify the node connections!")
    
    return 0


if __name__ == "__main__":
    exit(main())
