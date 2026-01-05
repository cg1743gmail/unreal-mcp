@echo off
echo ================================================================================
echo UnrealMCP Pipeline Setup (C++ Pipeline - No Blueprint)
echo ================================================================================
echo.
echo This will create Master Material only.
echo UnrealMCPFBXMaterialPipeline is a C++ class, ready to use!
echo.
pause

cd /d "%~dp0"
"C:\Users\ronal\AppData\Local\Programs\Python\Python312\python.exe" create_cpp_pipeline_instance.py

echo.
pause
