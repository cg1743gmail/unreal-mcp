#!/usr/bin/env python
"""
Script to create 10 box actors in Unreal Engine via MCP.
"""

import sys
import os
import socket
import json
import logging
from typing import Dict, Any, Optional

# Add the parent directory to the path
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Set up logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger("CreateTenBoxes")

def send_command(command: str, params: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    """Send a command to the Unreal MCP server and get the response."""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        try:
            command_obj = {
                "type": command,
                "params": params
            }
            
            command_json = json.dumps(command_obj)
            logger.info(f"Sending command: {command_json}")
            sock.sendall(command_json.encode('utf-8'))
            
            chunks = []
            while True:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                chunks.append(chunk)
                
                try:
                    data = b''.join(chunks)
                    json.loads(data.decode('utf-8'))
                    break
                except json.JSONDecodeError:
                    continue
            
            data = b''.join(chunks)
            response = json.loads(data.decode('utf-8'))
            logger.info(f"Received response: {response}")
            return response
            
        finally:
            sock.close()
            
    except Exception as e:
        logger.error(f"Error sending command: {e}")
        return None

def create_box(name: str, location: list[float]) -> Optional[Dict[str, Any]]:
    """Create a box actor with the specified name and location."""
    cube_params = {
        "name": name,
        "type": "StaticMeshActor",
        "location": location,
        "rotation": [0.0, 0.0, 0.0],
        "scale": [1.0, 1.0, 1.0]
    }
    
    response = send_command("create_actor", cube_params)
    if not response or response.get("status") != "success":
        logger.error(f"Failed to create box: {name}, response: {response}")
        return None
        
    logger.info(f"Created box '{name}' successfully at location {location}")
    return response

def main():
    """Create 10 boxes in Unreal Engine."""
    try:
        logger.info("Starting to create 10 boxes in Unreal Engine...")
        
        # Create 10 boxes with positions along the x-axis
        for i in range(10):
            box_name = f"Box_{i:02d}"
            # Position boxes along x-axis with 50 units spacing
            location = [i * 50.0, 0.0, 50.0]  # x, y, z
            
            box = create_box(box_name, location)
            if not box:
                logger.error(f"Failed to create box {i+1} of 10")
                return
            
        logger.info("Successfully created all 10 boxes!")
        logger.info("Boxes are positioned along the x-axis with 50 units spacing")
        
    except Exception as e:
        logger.error(f"Error in main: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
