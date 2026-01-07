# Texture Suffix Pipeline - Complete Implementation Guide

> **自动材质实例创建系统：根据纹理后缀 `_D` 和 `_NRA` 自动创建并配置材质实例**

---

## 📋 目标

创建 Interchange Blueprint Pipeline，实现导入 SkeletalMesh FBX 时：
- 识别 `_D` 后缀纹理（Diffuse/BaseColor）
- 识别 `_NRA` 后缀纹理（Packed: Normal+Roughness+AO）
- 自动创建材质实例
- 自动赋予纹理参数
- 自动链接到 FBX 材质插槽

---

## 🏗️ 架构方案

### 方案概述

**混合方案（C++ 基础 + Blueprint 配置）**

```
UUnrealMCPFBXMaterialPipeline (C++)
  ↓ 继承
BP_SkeletalMesh_TextureSuffix_Pipeline (Blueprint)
  ↓ 使用
M_SkeletalMesh_Master (Material)
  ↓ 实例化
MI_YourModel (Material Instance) ← 自动创建
```

**关键组件**：
1. ✅ **C++ Pipeline 基类**：`UUnrealMCPFBXMaterialPipeline` - 已实现，提供自动化框架
2. 🎨 **Master Material**：`M_SkeletalMesh_Master` - 支持 `_D` 和 `_NRA` 纹理参数
3. ⚙️ **Blueprint Pipeline**：`BP_SkeletalMesh_TextureSuffix_Pipeline` - 配置后缀映射
4. 🔗 **TextureParameterMapping**：`{"_D": "BaseColorTexture", "_NRA": "PackedTexture"}`

---

## 🚀 实施步骤

### STEP 1: 创建 Master Material 🎨

#### 方法 A：使用 UE Python Console（推荐）

1. 打开 UE Editor
2. **Tools** > **Execute Python Script**
3. 粘贴并执行以下代码：

```python
import unreal

# ============================================================================
# Create Master Material: M_SkeletalMesh_Master
# Supports: _D (BaseColor) and _NRA (Normal+Roughness+AO packed) textures
# ============================================================================

material_name = "M_SkeletalMesh_Master"
package_path = "/Game/Materials"
full_path = f"{package_path}/{material_name}"

# Create or load material
if unreal.EditorAssetLibrary.does_asset_exist(full_path):
    print(f"Material already exists, loading: {full_path}")
    material = unreal.EditorAssetLibrary.load_asset(full_path)
else:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material_factory = unreal.MaterialFactoryNew()
    material = asset_tools.create_asset(
        material_name, package_path, unreal.Material, material_factory
    )
    print(f"✓ Created Material: {full_path}")

# Create texture sample parameters
print("Creating texture parameters...")

# BaseColorTexture parameter (_D suffix)
base_color_param = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionTextureSampleParameter2D, -600, -300
)
base_color_param.set_editor_property("parameter_name", "BaseColorTexture")
base_color_param.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)

# PackedTexture parameter (_NRA suffix)
packed_param = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionTextureSampleParameter2D, -600, 100
)
packed_param.set_editor_property("parameter_name", "PackedTexture")
packed_param.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)

print("✓ Texture parameters created")

# Connect BaseColor directly
print("Connecting BaseColor...")
unreal.MaterialEditingLibrary.connect_material_property(
    base_color_param, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
)

# Extract Normal (R,G channels) from PackedTexture
print("Setting up Normal extraction (R,G channels)...")
component_mask_normal = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionComponentMask, -300, 100
)
component_mask_normal.set_editor_property("r", True)
component_mask_normal.set_editor_property("g", True)
component_mask_normal.set_editor_property("b", False)
component_mask_normal.set_editor_property("a", False)

unreal.MaterialEditingLibrary.connect_material_expressions(
    packed_param, "RGB", component_mask_normal, ""
)

# Add DeriveNormalZ (reconstruct Z from X,Y)
derive_normal_z = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionDeriveNormalZ, -100, 100
)

unreal.MaterialEditingLibrary.connect_material_expressions(
    component_mask_normal, "", derive_normal_z, ""
)

unreal.MaterialEditingLibrary.connect_material_property(
    derive_normal_z, "", unreal.MaterialProperty.MP_NORMAL
)

# Extract Roughness (B channel)
print("Setting up Roughness extraction (B channel)...")
component_mask_roughness = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionComponentMask, -300, 300
)
component_mask_roughness.set_editor_property("r", False)
component_mask_roughness.set_editor_property("g", False)
component_mask_roughness.set_editor_property("b", True)
component_mask_roughness.set_editor_property("a", False)

unreal.MaterialEditingLibrary.connect_material_expressions(
    packed_param, "RGB", component_mask_roughness, ""
)

unreal.MaterialEditingLibrary.connect_material_property(
    component_mask_roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
)

# Extract AO (A channel)
print("Setting up AO extraction (A channel)...")
component_mask_ao = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionComponentMask, -300, 500
)
component_mask_ao.set_editor_property("r", False)
component_mask_ao.set_editor_property("g", False)
component_mask_ao.set_editor_property("b", False)
component_mask_ao.set_editor_property("a", True)

unreal.MaterialEditingLibrary.connect_material_expressions(
    packed_param, "RGBA", component_mask_ao, ""
)

unreal.MaterialEditingLibrary.connect_material_property(
    component_mask_ao, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION
)

# Recompile and save
print("Compiling and saving material...")
unreal.MaterialEditingLibrary.recompile_material(material)
unreal.MaterialEditingLibrary.layout_material_expressions(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)

print("=" * 80)
print(f"✓✓✓ Master Material Created Successfully! ✓✓✓")
print("=" * 80)
print(f"Path: {full_path}")
print(f"Material Parameters:")
print(f"  - BaseColorTexture (for _D textures)")
print(f"  - PackedTexture (for _NRA textures)")
```

#### 方法 B：手动创建（可视化）

<details>
<summary>点击展开手动创建步骤</summary>

1. **创建新材质**：
   - Content Browser > 右键 `/Game/Materials/`
   - Create > Material
   - 命名：`M_SkeletalMesh_Master`

2. **添加纹理参数**：

   a) **BaseColorTexture** (for `_D` suffix):
   ```
   右键 Material Graph > Parameters > Texture Sample Parameter
   - Name: "BaseColorTexture"
   - 连接 RGB pin 到 Base Color 输入
   ```

   b) **PackedTexture** (for `_NRA` suffix):
   ```
   添加另一个 Texture Sample Parameter
   - Name: "PackedTexture"
   ```

3. **从 PackedTexture 提取通道**：

   a) **Normal (R,G channels)**:
   ```
   Component Mask 节点:
   - 连接 PackedTexture.RGB 到 Component Mask
   - 启用 R 和 G 通道
   
   DeriveNormalZ 节点:
   - 连接 Component Mask 到 DeriveNormalZ
   - 连接 DeriveNormalZ 到 Normal 输入
   ```

   b) **Roughness (B channel)**:
   ```
   Component Mask 节点:
   - 连接 PackedTexture.RGB 到 Component Mask
   - 仅启用 B 通道
   - 连接到 Roughness 输入
   ```

   c) **Ambient Occlusion (A channel)**:
   ```
   Component Mask 节点:
   - 连接 PackedTexture.RGBA 到 Component Mask
   - 仅启用 A 通道
   - 连接到 Ambient Occlusion 输入
   ```

4. **应用并保存**

</details>

#### 材质节点结构图

```
                                                          ┌──────────┐
┌──────────────────────┐                            ┌───►│BaseColor │
│ BaseColorTexture     ├──RGB─────────────────────►│    └──────────┘
│ (Texture Parameter)  │                            │    
└──────────────────────┘                            │    ┌──────────┐
                                                    │    │ Normal   │
┌──────────────────────┐     ┌───────────────┐     │    └──────────┘
│ PackedTexture        ├─RG─►│ComponentMask  │     │         ▲
│ (Texture Parameter)  │     │(R=1,G=1)      ├─────┤         │
│                      │     └───────────────┘     │    ┌─────────┐
│                      │                           ├───►│DeriveNZ │
│                      │     ┌───────────────┐     │    └─────────┘
│                      ├─B──►│ComponentMask  ├─────┤
│                      │     │(B=1)          │     │    ┌──────────┐
│                      │     └───────────────┘     ├───►│Roughness │
│                      │                           │    └──────────┘
│                      │     ┌───────────────┐     │
│                      ├─A──►│ComponentMask  ├─────┤    ┌──────────┐
│                      │     │(A=1)          │     └───►│   AO     │
└──────────────────────┘     └───────────────┘          └──────────┘
```

---

### STEP 2: 创建 Pipeline Blueprint ⚙️

使用 MCP 自然语言指令或 Python 脚本：

#### 选项 A：通过 MCP 自然语言

在 Codebuddy/Cursor/Windsurf 中输入：

```
Create an Interchange Pipeline Blueprint named "BP_SkeletalMesh_TextureSuffix_Pipeline" 
in /Game/Interchange/Pipelines/ with parent class "FBXMaterialPipeline"
```

#### 选项 B：使用 Python 脚本

运行脚本：
```bash
python e:/Work/UGit/UEMCP/unreal-mcp/Python/scripts/create_texture_suffix_pipeline_complete.py
```

---

### STEP 3: 配置 Pipeline 映射 🔗

#### 自动配置（通过 MCP）

```python
# 使用 MCP 工具配置
configure_interchange_pipeline(
    pipeline_path="/Game/Interchange/Pipelines/BP_SkeletalMesh_TextureSuffix_Pipeline",
    settings={
        "bAutoCreateMaterialInstances": True,
        "bAutoAssignTextures": True,
        "bSearchExistingMaterials": True,
        "MaterialInstanceSubFolder": "MaterialInstances",
        "ParentMaterial": "/Game/Materials/M_SkeletalMesh_Master.M_SkeletalMesh_Master",
        "TextureParameterMapping": {
            "_D": "BaseColorTexture",
            "_NRA": "PackedTexture"
        }
    }
)
```

#### 手动配置（UE Editor）

1. 打开 Pipeline Blueprint：`/Game/Interchange/Pipelines/BP_SkeletalMesh_TextureSuffix_Pipeline`
2. Details 面板中找到：
   - **Parent Material** → 设置为 `/Game/Materials/M_SkeletalMesh_Master`
   - **Texture Parameter Mapping** → 添加：
     ```
     Key: "_D"    Value: "BaseColorTexture"
     Key: "_NRA"  Value: "PackedTexture"
     ```
3. 编译并保存

---

### STEP 4: 编译并验证 ✅

```python
# 编译 Pipeline
compile_interchange_pipeline(
    pipeline_path="/Game/Interchange/Pipelines/BP_SkeletalMesh_TextureSuffix_Pipeline"
)
```

---

## 🧪 测试验证

### 1. 准备测试资产

创建测试 FBX 模型：
```
TestCharacter.fbx
TestCharacter_D.png      (Base Color/Diffuse)
TestCharacter_NRA.png    (Packed: Normal XY + Roughness + AO)
```

### 2. 导入 FBX

1. 拖拽 `TestCharacter.fbx` 到 Content Browser
2. Interchange Import Dialog 出现
3. **Add Pipeline** → 选择 `BP_SkeletalMesh_TextureSuffix_Pipeline`
4. 点击 **Import**

### 3. 验证结果

自动生成的资产：
```
✓ TestCharacter (SkeletalMesh)
✓ MaterialInstances/MI_TestCharacter (Material Instance)
    - BaseColorTexture = TestCharacter_D
    - PackedTexture = TestCharacter_NRA
✓ TestCharacter_D (Texture2D)
✓ TestCharacter_NRA (Texture2D)
```

检查点：
- [ ] 材质实例已自动创建
- [ ] `_D` 纹理已赋予 BaseColorTexture 参数
- [ ] `_NRA` 纹理已赋予 PackedTexture 参数
- [ ] 材质实例已自动应用到 SkeletalMesh
- [ ] 在视口中模型显示正确

---

## 📚 高级配置

### 扩展更多后缀

修改 `TextureParameterMapping`：

```python
"TextureParameterMapping": {
    "_D": "BaseColorTexture",
    "_NRA": "PackedTexture",
    "_M": "MetallicTexture",        # 添加 Metallic 后缀
    "_E": "EmissiveTexture",        # 添加 Emissive 后缀
    "_H": "HeightTexture"           # 添加 Height 后缀
}
```

### 自定义 Master Material

1. 复制 `M_SkeletalMesh_Master` 创建变体
2. 添加更多纹理参数
3. 调整材质逻辑（例如：Subsurface Scattering）
4. 更新 Pipeline 的 ParentMaterial 指向新材质

### 不同项目的 Packed Texture 格式

如果你的 `_NRA` 格式不同（例如 `_RMA` = Roughness + Metallic + AO）：

1. 修改 Master Material 的 channel 映射
2. 更新参数名称
3. 更新 Pipeline 配置

---

## 🛠️ 故障排查

### 问题：纹理没有自动赋值

**原因**：`TextureParameterMapping` 使用 `Contains` 匹配，可能后缀不匹配。

**解决**：
1. 检查纹理文件名是否包含 `_D` 或 `_NRA`
2. 检查 Pipeline 的 `TextureParameterMapping` 配置
3. 检查 Master Material 是否有对应的参数名称

### 问题：材质实例没有创建

**原因**：`bAutoCreateMaterialInstances` 未启用。

**解决**：
```python
configure_interchange_pipeline(
    ...,
    settings={"bAutoCreateMaterialInstances": True}
)
```

### 问题：Normal 贴图显示不正确

**原因**：`_NRA` 的 R,G 通道未正确重建 Z。

**解决**：
1. 确保 Master Material 使用了 `DeriveNormalZ` 节点
2. 检查 Normal 纹理的压缩设置（应为 `NormalMap`）
3. 验证 `_NRA` 纹理的 R,G 通道确实是 Normal XY

---

## 📖 参考

### C++ 源码

- **Pipeline 基类**：`MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Public/Pipelines/UnrealMCPFBXMaterialPipeline.h`
- **实现**：`MCPGameProject/Plugins/UnrealMCP/Private/Pipelines/UnrealMCPFBXMaterialPipeline.cpp`

### Python 脚本

- **完整脚本**：`Python/scripts/create_texture_suffix_pipeline_complete.py`
- **工具库**：`Python/tools/interchange_tools.py`

### UE 文档

- [Interchange Framework](https://docs.unrealengine.com/5.3/en-US/interchange-framework-in-unreal-engine/)
- [Material Instances](https://docs.unrealengine.com/5.3/en-US/material-instances-in-unreal-engine/)

---

## ✅ 总结

**实施完成后，你将拥有**：

1. ✅ **M_SkeletalMesh_Master** - 支持 `_D` 和 `_NRA` 的 Master Material
2. ✅ **BP_SkeletalMesh_TextureSuffix_Pipeline** - 自动化 Pipeline
3. ✅ **自动材质实例创建** - 导入 FBX 时自动运行
4. ✅ **纹理自动映射** - 根据后缀自动赋值
5. ✅ **完整工作流** - 从 FBX 到最终材质一键完成

**工作流示意**：

```
FBX Import
    ↓
Interchange Pipeline 识别纹理后缀
    ↓
├─ Character_D.png   → BaseColorTexture
├─ Character_NRA.png → PackedTexture
    ↓
创建 Material Instance (MI_Character)
    ↓
应用到 SkeletalMesh
    ↓
✓ 完成！
```

---

**版本**: v1.0  
**日期**: 2026-01-05  
**作者**: UnrealMCP  
**项目**: [unreal-mcp](https://github.com/chongdashu/unreal-mcp)
