#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Create Blueprint Pipeline - Correct Parent Class
使用正确的具体父类（InterchangeGenericMaterialPipeline）
"""
import unreal

print("=" * 70)
print("Create Blueprint Pipeline - Correct Parent Class")
print("=" * 70)

# 删除旧 BP
old_path = '/Game/Pipelines/BPI_FBXMaterial'
if unreal.EditorAssetLibrary.does_asset_exist(old_path):
    unreal.EditorAssetLibrary.delete_asset(old_path)
    print(f'Deleted old: {old_path}')

# 创建 Blueprint 使用 Factory
# 使用 InterchangeGenericMaterialPipeline 作为父类（具体类，非抽象）
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
blueprint_factory = unreal.BlueprintFactory()
blueprint_factory.set_editor_property('parent_class', unreal.InterchangeGenericMaterialPipeline)

# 创建 Blueprint
bp = asset_tools.create_asset(
    asset_name='BPI_FBXMaterial',
    package_path='/Game/Pipelines',
    asset_class=unreal.Blueprint,
    factory=blueprint_factory
)

if bp:
    print(f'SUCCESS: Created {bp.get_path_name()}')
    print(f'Parent Class: InterchangeGenericMaterialPipeline')
    
    # 配置默认属性
    try:
        cdo = unreal.get_default_object(bp.generated_class())
        
        # 设置 Material Import 模式
        cdo.set_editor_property('material_import', unreal.MaterialImportType.IMPORT_MATERIALS)
        
        # 设置 Parent Material
        master_mat = unreal.load_asset('/Game/Materials/M_SkeletalMesh_Master')
        if master_mat:
            cdo.set_editor_property('parent_material', master_mat)
            print('Parent Material configured: /Game/Materials/M_SkeletalMesh_Master')
        
        # 设置 Texture Import
        cdo.set_editor_property('texture_import', unreal.MaterialTextureImportType.IMPORT_TEXTURES)
        
        print('Default properties configured')
    except Exception as e:
        print(f'Note: Could not configure properties: {e}')
    
    # 保存
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    print('Saved!')
    
    print("\n" + "=" * 70)
    print("Blueprint Pipeline created successfully!")
    print("=" * 70)
    print(f"Asset: {bp.get_path_name()}")
    print("Parent: InterchangeGenericMaterialPipeline")
    print("\nYou can now:")
    print("  1. Open it in Blueprint Editor")
    print("  2. Edit properties in Details panel")
    print("  3. Add custom logic in Event Graph")
else:
    print('FAILED to create Blueprint')

print("\n" + "=" * 70)
