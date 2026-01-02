@echo off
setlocal

echo Publishing UnrealMCP.Sidecar (framework-dependent, single-file) ...
dotnet publish "%~dp0UnrealMCP.Sidecar\UnrealMCP.Sidecar.csproj" -c Release -r win-x64 --self-contained false -o "%~dp0publish\win-x64" /p:PublishSingleFile=true /p:IncludeNativeLibrariesForSelfExtract=true --nologo

if errorlevel 1 (
  echo Publish failed.
  exit /b 1
)

echo Done. Output:
echo   %~dp0publish\win-x64\UnrealMCP.Sidecar.exe
endlocal
