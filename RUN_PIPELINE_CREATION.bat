@echo off
echo ================================================================================
echo Pipeline Creation via MCP
echo ================================================================================
echo.
echo This script will:
echo 1. Create Master Material (M_SkeletalMesh_Master) with Normal Remap
echo 2. Create Pipeline Blueprint (BP_SkeletalMesh_TextureSuffix_Pipeline)
echo 3. Configure Pipeline settings
echo 4. Compile Pipeline Blueprint
echo.
echo Make sure UE Editor is running with UnrealMCP plugin loaded!
echo.
pause

cd /d "%~dp0"
"C:\Users\ronal\AppData\Local\Programs\Python\Python312\python.exe" create_pipeline_complete.py

echo.
echo ================================================================================
echo Press any key to close...
pause >nul
