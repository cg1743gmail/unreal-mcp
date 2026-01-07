@echo off
echo ========================================
echo Recreate Blueprint Pipeline
echo ========================================
echo.

python -c "import socket, json; sock = socket.socket(); sock.settimeout(30); sock.connect(('127.0.0.1', 55557)); code = '''import unreal;old=\"/Game/Pipelines/BPI_FBXMaterial\";unreal.EditorAssetLibrary.delete_asset(old) if unreal.EditorAssetLibrary.does_asset_exist(old) else None;bp=unreal.InterchangeBlueprintPipelineBaseLibrary.create_interchange_pipeline_blueprint(\"/Game/Pipelines\",\"BPI_FBXMaterial\",unreal.InterchangeBlueprintPipelineBase);print(f\"Created: /Game/Pipelines/BPI_FBXMaterial\") if bp else print(\"Failed\");unreal.EditorAssetLibrary.save_loaded_asset(bp) if bp else None'''; msg = json.dumps({'type': 'ping', 'params': {}}); sock.sendall(msg.encode()); print('Connection OK' if b'pong' in sock.recv(4096) else 'Connection Failed'); sock.close(); print('\\nNote: execute_python command not available, manual creation required')"

echo.
echo ========================================
echo Manual Steps (in UE Editor):
echo ========================================
echo 1. Open Output Log: Window ^> Developer Tools ^> Output Log
echo 2. Type in command line at bottom:
echo.
echo    py "import unreal; old='/Game/Pipelines/BPI_FBXMaterial'; unreal.EditorAssetLibrary.delete_asset(old) if unreal.EditorAssetLibrary.does_asset_exist(old) else None; bp=unreal.InterchangeBlueprintPipelineBaseLibrary.create_interchange_pipeline_blueprint('/Game/Pipelines','BPI_FBXMaterial',unreal.InterchangeBlueprintPipelineBase); print('Created:',bp.get_path_name()) if bp else print('Failed'); unreal.EditorAssetLibrary.save_loaded_asset(bp) if bp else None"
echo.
echo 3. Press Enter
echo.
echo ========================================
pause
