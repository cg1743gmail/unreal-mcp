#!/usr/bin/env pwsh
# Execute Pipeline Creation via MCP
# This script sends MCP commands directly to the running UnrealMCP server

Write-Host "=" -NoNewline
Write-Host ("=" * 79)
Write-Host "Executing Pipeline Creation via MCP"
Write-Host ("=" * 80)

# Change to script directory
$scriptPath = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $scriptPath

# Execute the Python script
Write-Host "`nLaunching Pipeline creation script..."
$pythonPath = "C:\Users\ronal\AppData\Local\Programs\Python\Python312\python.exe"
$scriptFile = Join-Path $scriptPath "create_pipeline_auto.py"

if (Test-Path $pythonPath) {
    if (Test-Path $scriptFile) {
        & $pythonPath $scriptFile
        $exitCode = $LASTEXITCODE
        
        if ($exitCode -eq 0) {
            Write-Host "`n✓ Pipeline creation completed successfully!" -ForegroundColor Green
        } else {
            Write-Host "`n✗ Pipeline creation failed with exit code: $exitCode" -ForegroundColor Red
        }
    } else {
        Write-Host "Error: Script file not found: $scriptFile" -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "Error: Python not found at: $pythonPath" -ForegroundColor Red
    exit 1
}
