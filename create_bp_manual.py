#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Create Blueprint Pipeline using UE Python API (Manual Method)
使用 Blueprint Factory 手动创建
"""
import unreal

print("=" * 70)
print("Create Blueprint Pipeline - Manual Method")
print("=" * 70)

# 删除旧 BP
old_path = '/Game/Pipelines/BPI_FBXMaterial'
if unreal.EditorAssetLibrary.does_asset_exist(old_path):
    unreal.EditorAssetLibrary.delete_asset(old_path)
    print(f'Deleted old: {old_path}')

# 创建 Blueprint 使用 Factory
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
blueprint_factory = unreal.BlueprintFactory()
blueprint_factory.set_editor_property('parent_class', unreal.InterchangeBlueprintPipelineBase)

# 创建 Blueprint
bp = asset_tools.create_asset(
    asset_name='BPI_FBXMaterial',
    package_path='/Game/Pipelines',
    asset_class=unreal.Blueprint,
    factory=blueprint_factory
)

if bp:
    print(f'SUCCESS: Created {bp.get_path_name()}')
    
    # 保存
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    print('Saved!')
    
    print("\n" + "=" * 70)
    print("Blueprint created successfully!")
    print("=" * 70)
    print(f"Asset: {bp.get_path_name()}")
    print("You can now open it in Rider/Blueprint Editor!")
else:
    print('FAILED to create Blueprint')
