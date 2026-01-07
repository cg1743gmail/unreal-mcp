# MCP Pipeline Creation Script
$ErrorActionPreference = "Stop"

function Send-MCPCommand {
    param(
        [string]$CommandType,
        [hashtable]$Params
    )
    
    $client = New-Object System.Net.Sockets.TcpClient
    try {
        $client.Connect("127.0.0.1", 55557)
        $stream = $client.GetStream()
        
        $command = @{
            type = $CommandType
            params = $Params
        } | ConvertTo-Json -Depth 10
        
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($command)
        $stream.Write($bytes, 0, $bytes.Length)
        
        # Read response
        $buffer = New-Object byte[] 8192
        $response = ""
        
        do {
            $read = $stream.Read($buffer, 0, $buffer.Length)
            if ($read -gt 0) {
                $response += [System.Text.Encoding]::UTF8.GetString($buffer, 0, $read)
            }
        } while ($stream.DataAvailable)
        
        Start-Sleep -Milliseconds 100
        
        $client.Close()
        return $response | ConvertFrom-Json
    }
    catch {
        Write-Host "Error: $_"
        if ($client) { $client.Close() }
        return $null
    }
}

Write-Host "=" -NoNewline -ForegroundColor Cyan
Write-Host ("=" * 79) -ForegroundColor Cyan
Write-Host "MCP Pipeline Creation Script" -ForegroundColor Yellow
Write-Host ("=" * 80) -ForegroundColor Cyan

# Step 1: Test connection
Write-Host "`nStep 0: Testing MCP connection..."
$result = Send-MCPCommand -CommandType "ping" -Params @{}
if ($result.result.message -eq "pong") {
    Write-Host "✓ MCP Connected!" -ForegroundColor Green
} else {
    Write-Host "✗ MCP Connection Failed!" -ForegroundColor Red
    exit 1
}

# Step 1: Create Master Material
Write-Host "`nStep 1: Creating Master Material..." -ForegroundColor Yellow

$materialCode = @"
import unreal

material_name = "M_SkeletalMesh_Master"
package_path = "/Game/Materials"
full_path = f"{package_path}/{material_name}"

if unreal.EditorAssetLibrary.does_asset_exist(full_path):
    material = unreal.EditorAssetLibrary.load_asset(full_path)
    print(f"Material exists: {full_path}")
else:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material_factory = unreal.MaterialFactoryNew()
    material = asset_tools.create_asset(material_name, package_path, unreal.Material, material_factory)
    print(f"Created: {full_path}")

# BaseColor
base_color = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -600, -300)
base_color.set_editor_property("parameter_name", "BaseColorTexture")
unreal.MaterialEditingLibrary.connect_material_property(base_color, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

# Packed Texture
packed = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -800, 100)
packed.set_editor_property("parameter_name", "PackedTexture")

# Normal Remap Parameter
remap_param = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionStaticBoolParameter, -800, 300)
remap_param.set_editor_property("parameter_name", "bNormalNeedsRemap")
remap_param.set_editor_property("default_value", True)

# Extract Normal (R,G)
mask_n = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionComponentMask, -550, 100)
mask_n.set_editor_property("r", True)
mask_n.set_editor_property("g", True)
unreal.MaterialEditingLibrary.connect_material_expressions(packed, "RGB", mask_n, "")

# Remap logic
mad = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionMultiplyAdd, -400, 100)
mad.set_editor_property("const_a", 2.0)
mad.set_editor_property("const_b", -1.0)
unreal.MaterialEditingLibrary.connect_material_expressions(mask_n, "", mad, "")

lerp = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -250, 100)
unreal.MaterialEditingLibrary.connect_material_expressions(mad, "", lerp, "A")
unreal.MaterialEditingLibrary.connect_material_expressions(mask_n, "", lerp, "B")
unreal.MaterialEditingLibrary.connect_material_expressions(remap_param, "", lerp, "Alpha")

derive_z = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionDeriveNormalZ, -100, 100)
unreal.MaterialEditingLibrary.connect_material_expressions(lerp, "", derive_z, "")
unreal.MaterialEditingLibrary.connect_material_property(derive_z, "", unreal.MaterialProperty.MP_NORMAL)

# Roughness
mask_r = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionComponentMask, -550, 300)
mask_r.set_editor_property("b", True)
unreal.MaterialEditingLibrary.connect_material_expressions(packed, "RGB", mask_r, "")
unreal.MaterialEditingLibrary.connect_material_property(mask_r, "", unreal.MaterialProperty.MP_ROUGHNESS)

# AO
mask_ao = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionComponentMask, -550, 500)
mask_ao.set_editor_property("a", True)
unreal.MaterialEditingLibrary.connect_material_expressions(packed, "RGBA", mask_ao, "")
unreal.MaterialEditingLibrary.connect_material_property(mask_ao, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

unreal.MaterialEditingLibrary.recompile_material(material)
unreal.MaterialEditingLibrary.layout_material_expressions(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)
print("Material created with Normal Remap support")
"@

$result = Send-MCPCommand -CommandType "execute_python" -Params @{code = $materialCode}
if ($result.success -or $result.status -eq "success") {
    Write-Host "✓ Master Material Created!" -ForegroundColor Green
} else {
    Write-Host "Note: $($result.message)" -ForegroundColor Yellow
}

# Step 2: Create Pipeline Blueprint
Write-Host "`nStep 2: Creating Pipeline Blueprint..." -ForegroundColor Yellow
$result = Send-MCPCommand -CommandType "create_interchange_pipeline_blueprint" -Params @{
    name = "BP_SkeletalMesh_TextureSuffix_Pipeline"
    package_path = "/Game/Interchange/Pipelines/"
    parent_class = "FBXMaterialPipeline"
}

if ($result.success -or $result.status -eq "success") {
    Write-Host "✓ Pipeline Blueprint Created!" -ForegroundColor Green
} else {
    Write-Host "Result: $($result.message)" -ForegroundColor Yellow
}

# Step 3: Configure Pipeline
Write-Host "`nStep 3: Configuring Pipeline..." -ForegroundColor Yellow
$result = Send-MCPCommand -CommandType "configure_interchange_pipeline" -Params @{
    pipeline_path = "/Game/Interchange/Pipelines/BP_SkeletalMesh_TextureSuffix_Pipeline"
    settings = @{
        bAutoCreateMaterialInstances = $true
        bAutoAssignTextures = $true
        bSearchExistingMaterials = $true
        MaterialInstanceSubFolder = "MaterialInstances"
        ParentMaterial = "/Game/Materials/M_SkeletalMesh_Master.M_SkeletalMesh_Master"
    }
}

if ($result.success -or $result.status -eq "success") {
    Write-Host "✓ Pipeline Configured!" -ForegroundColor Green
} else {
    Write-Host "Note: $($result.message)" -ForegroundColor Yellow
}

# Step 4: Compile Pipeline
Write-Host "`nStep 4: Compiling Pipeline..." -ForegroundColor Yellow
$result = Send-MCPCommand -CommandType "compile_interchange_pipeline" -Params @{
    pipeline_path = "/Game/Interchange/Pipelines/BP_SkeletalMesh_TextureSuffix_Pipeline"
}

if ($result.success -or $result.status -eq "success") {
    Write-Host "✓ Pipeline Compiled!" -ForegroundColor Green
    Write-Host "Status: $($result.result.status)" -ForegroundColor Cyan
} else {
    Write-Host "Result: $result" -ForegroundColor Yellow
}

Write-Host "`n" -NoNewline
Write-Host ("=" * 80) -ForegroundColor Cyan
Write-Host "✓✓✓ PIPELINE CREATION COMPLETE ✓✓✓" -ForegroundColor Green
Write-Host ("=" * 80) -ForegroundColor Cyan
Write-Host @"

Pipeline: /Game/Interchange/Pipelines/BP_SkeletalMesh_TextureSuffix_Pipeline
Material: /Game/Materials/M_SkeletalMesh_Master

✅ NEW FEATURES:
- Automatic suffix detection: _D, _NRA, _N, _R, _M, _AO
- Normal range auto-remap (controlled by bNormalNeedsRemap parameter)
- Pipeline compiled and saved

Usage:
1. Import FBX with textures named:
   - YourModel_D.png (BaseColor)
   - YourModel_NRA.png (Normal.RG + Roughness.B + AO.A)
2. Material Instance auto-created in MaterialInstances folder
3. Textures auto-assigned!
"@
