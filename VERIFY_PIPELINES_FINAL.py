"""
验证修复后的 get_interchange_pipelines 能否识别 Blueprint
通过 UE Editor 执行此脚本
"""
import unreal

print("=" * 60)
print("验证 Interchange Pipeline Blueprints")
print("=" * 60)

# 1. 检查手动创建的 Blueprint 是否存在
bp_paths = [
    '/Game/Pipelines/BPI_FBXMaterial',  # MCP 创建的
]

print("\n[步骤 1] 检查 Blueprint 资产")
for bp_path in bp_paths:
    exists = unreal.EditorAssetLibrary.does_asset_exist(bp_path)
    print(f"  {bp_path}: {'存在' if exists else '不存在'}")
    
    if exists:
        bp = unreal.load_asset(bp_path)
        if bp:
            gen_class = bp.generated_class()
            parent = gen_class.get_super_struct()
            print(f"    类型: {type(bp).__name__}")
            print(f"    Generated Class: {gen_class.get_name()}")
            print(f"    父类: {parent.get_name() if parent else 'None'}")
            
            # 检查是否继承自 InterchangePipelineBase
            base_class = unreal.InterchangePipelineBase
            is_pipeline = gen_class.is_child_of(base_class)
            print(f"    是 InterchangePipelineBase 子类: {is_pipeline}")

# 2. 搜索所有 Interchange Pipeline Blueprints
print("\n[步骤 2] 搜索所有 Interchange Pipeline Blueprints")
asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()

# 搜索 Blueprint 类型，父类为 InterchangePipelineBase
filter_options = unreal.ARFilter(
    class_names=['Blueprint'],
    recursive_paths=True,
    package_paths=['/Game/']
)

assets = asset_registry.get_assets(filter_options)
print(f"  找到 {len(assets)} 个 Blueprint 资产")

interchange_blueprints = []
for asset in assets:
    # 加载 Blueprint 检查父类
    bp = unreal.load_asset(asset.get_full_name())
    if bp and hasattr(bp, 'generated_class'):
        gen_class = bp.generated_class()
        if gen_class and gen_class.is_child_of(unreal.InterchangePipelineBase):
            interchange_blueprints.append(asset)
            print(f"  ✓ {asset.asset_name} - 路径: {asset.package_name}")

print(f"\n[结果] 找到 {len(interchange_blueprints)} 个 Interchange Pipeline Blueprint")

# 3. 测试 MCP 命令（如果可用）
print("\n[步骤 3] 建议通过 MCP 测试 get_interchange_pipelines 命令")
print("  命令: get_interchange_pipelines")
print("  参数: {\"search_path\": \"/Game/\"}")

print("\n" + "=" * 60)
