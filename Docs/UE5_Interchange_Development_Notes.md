# UE5 Interchange Pipeline 开发关键知识点

> 本文档记录了在开发 UnrealMCP Interchange Pipeline 过程中遇到的关键问题和解决方案。
> 这些知识点是 Claude 4.5 模型尚不完全具备的 UE5 领域专业知识。

---

## 1. Interchange 模块结构与头文件路径

### 模块依赖关系

```
InterchangeCore (引擎 Runtime 模块)
    ↑
InterchangeFactoryNodes (插件模块)
    ↑
InterchangePipelines (插件模块)
    ↑
InterchangeNodes (插件模块)
```

### 正确的 Include 路径

| 类名 | 头文件位置 | 正确的 #include |
|------|-----------|-----------------|
| `UInterchangeBaseNodeContainer` | `Engine/Source/Runtime/Interchange/Core/Public/Nodes/` | `#include "Nodes/InterchangeBaseNodeContainer.h"` |
| `UInterchangeMaterialFactoryNode` | `Engine/Plugins/Interchange/.../FactoryNodes/Public/` | `#include "InterchangeMaterialFactoryNode.h"` |
| `UInterchangeTextureFactoryNode` | `Engine/Plugins/Interchange/.../FactoryNodes/Public/` | `#include "InterchangeTextureFactoryNode.h"` |
| `UInterchangeShaderGraphNode` | `Engine/Plugins/Interchange/.../Nodes/Public/` | `#include "InterchangeShaderGraphNode.h"` |
| `UInterchangeGenericMaterialPipeline` | `Engine/Plugins/Interchange/.../Pipelines/Public/` | `#include "InterchangeGenericMaterialPipeline.h"` |

**注意：** `InterchangeBaseNodeContainer.h` 需要 `Nodes/` 前缀，而 `InterchangeShaderGraphNode.h` 不需要。

### Build.cs 模块依赖

```csharp
PrivateDependencyModuleNames.AddRange(
    new string[]
    {
        "InterchangeCore",          // UInterchangeBaseNodeContainer
        "InterchangeEngine",        // UInterchangeManager
        "InterchangePipelines",     // UInterchangeGenericMaterialPipeline
        "InterchangeFactoryNodes",  // UInterchangeMaterialFactoryNode
        "InterchangeNodes"          // UInterchangeShaderGraphNode
    }
);
```

---

## 2. UInterchangeGenericMaterialPipeline 的 ParentMaterial 类型

### 问题

继承 `UInterchangeGenericMaterialPipeline` 时，`ParentMaterial` 属性的类型是 `FSoftObjectPath`，**不是** `TSoftObjectPtr<UMaterialInterface>`。

### 父类定义 (UE 5.5)

```cpp
// InterchangeGenericMaterialPipeline.h
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials", 
    Meta=(EditCondition="bImportMaterials && MaterialImport==EInterchangeMaterialImportOption::ImportAsMaterialInstances", 
          AllowedClasses="/Script/Engine.MaterialInterface"))
FSoftObjectPath ParentMaterial;
```

### 正确用法

```cpp
// ❌ 错误 - TSoftObjectPtr API
UMaterialInterface* LoadedMaterial = ParentMaterial.LoadSynchronous();

// ✅ 正确 - FSoftObjectPath API
UMaterialInterface* LoadedMaterial = Cast<UMaterialInterface>(ParentMaterial.TryLoad());
```

### FSoftObjectPath vs TSoftObjectPtr 对比

| 特性 | FSoftObjectPath | TSoftObjectPtr<T> |
|------|-----------------|-------------------|
| 类型安全 | 无类型限制 | 模板类型安全 |
| 加载方法 | `TryLoad()`, `ResolveObject()` | `LoadSynchronous()`, `Get()` |
| 空值检查 | `IsNull()`, `IsValid()` | `IsNull()`, `IsValid()` |
| 使用场景 | UPROPERTY 反射兼容 | C++ 类型安全场景 |

---

## 3. 插件依赖声明

### 问题

如果模块依赖了 Interchange 相关模块，但 `.uplugin` 文件没有声明插件依赖，会产生警告：

```
Warning: Plugin 'UnrealMCP' does not list plugin 'Interchange' as a dependency, 
but module 'UnrealMCP' depends on module 'InterchangePipelines'.
```

### 解决方案

在 `.uplugin` 文件中添加 Interchange 插件依赖：

```json
{
    "Plugins": [
        {
            "Name": "Interchange",
            "Enabled": true
        }
    ]
}
```

---

## 4. UPROPERTY 变量 Shadowing 限制

### 问题

UE 的 UHT (Unreal Header Tool) 不允许子类定义与父类同名的 UPROPERTY 变量：

```
Error: Member variable declaration: 'ParentMaterial' cannot be defined in 
'UUnrealMCPFBXMaterialPipeline' as it is already defined in scope 
'UInterchangeGenericMaterialPipeline' (shadowing is not allowed)
```

### 解决方案

1. **不要重新定义父类已有的 UPROPERTY**
2. 直接使用继承的属性
3. 如需扩展，使用不同的变量名

```cpp
// ❌ 错误 - 重复定义父类属性
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Instance Settings")
TSoftObjectPtr<UMaterialInterface> ParentMaterial;  // 父类已有！

// ✅ 正确 - 使用继承的属性或定义新名称
// 直接使用 ParentMaterial (继承自父类)
// 或定义新属性名
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Instance Settings")
TSoftObjectPtr<UMaterialInterface> CustomParentMaterial;
```

---

## 5. Interchange Pipeline 蓝图最佳实践

### Pipeline 类继承层次

```
UInterchangePipelineBase
    ├── UInterchangeGenericAssetsPipeline (通用资产)
    ├── UInterchangeGenericMaterialPipeline (材质)
    ├── UInterchangeGenericMeshPipeline (网格)
    ├── UInterchangeGenericTexturePipeline (纹理)
    └── UInterchangeBlueprintPipelineBase (蓝图扩展)
```

### 关键虚函数

| 函数 | 调用时机 | 用途 |
|------|---------|------|
| `ExecutePipeline` | 导入开始时 | 配置 Factory Nodes |
| `ExecutePostFactoryPipeline` | 资产创建后 | 后处理工厂节点 |
| `ExecutePostImportPipeline` | 资产导入完成后 | 最终处理（如创建材质实例） |

### 示例实现

```cpp
void UMyPipeline::ExecutePipeline(
    UInterchangeBaseNodeContainer* InBaseNodeContainer,
    const TArray<UInterchangeSourceData*>& InSourceDatas,
    const FString& ContentBasePath)
{
    Super::ExecutePipeline(InBaseNodeContainer, InSourceDatas, ContentBasePath);
    
    // 遍历材质工厂节点
    InBaseNodeContainer->IterateNodesOfType<UInterchangeMaterialFactoryNode>(
        [this](const FString& NodeUid, UInterchangeMaterialFactoryNode* Node)
        {
            // 配置节点...
        }
    );
}

void UMyPipeline::ExecutePostImportPipeline(
    const UInterchangeBaseNodeContainer* InBaseNodeContainer,
    const FString& NodeKey,
    UObject* CreatedAsset,
    bool bIsAReimport)
{
    Super::ExecutePostImportPipeline(InBaseNodeContainer, NodeKey, CreatedAsset, bIsAReimport);
    
    if (UMaterialInterface* Material = Cast<UMaterialInterface>(CreatedAsset))
    {
        // 创建材质实例...
    }
}
```

---

## 6. 版本兼容性说明

| UE 版本 | Interchange 状态 | 注意事项 |
|---------|-----------------|----------|
| 5.0 - 5.2 | 实验性 | API 可能变化 |
| 5.3+ | 正式发布 | API 稳定 |
| 5.5 | 推荐版本 | 本文档基于此版本 |

---

## 7. 编辑器工具异步与 GC 安全

### 问题背景

编辑器工具（包括 Interchange Pipeline）在异步操作中可能遇到 GC 导致的崩溃：
- GC 在游戏线程运行，异步任务在其他线程执行
- UObject 可能在异步任务执行期间被 GC 回收
- 编辑器环境 GC 更频繁（资产加载/卸载、PIE 切换等）

### 防止 GC 回收的推荐方法

#### 1. TStrongObjectPtr (推荐)

```cpp
#include "UObject/StrongObjectPtrTemplates.h"

// 强引用，防止 GC 回收
TStrongObjectPtr<UObject> StrongRef = TStrongObjectPtr<UObject>(SomeUObject);

// 异步操作中安全使用
AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [StrongRef]()
{
    if (StrongRef.IsValid())
    {
        // 安全访问
    }
});
```

#### 2. FGCObject (非 UObject 类)

```cpp
#include "UObject/GCObject.h"

class FMyHelper : public FGCObject
{
    TArray<UObject*> ReferencedObjects;
    
    virtual void AddReferencedObjects(FReferenceCollector& Collector) override
    {
        Collector.AddReferencedObjects(ReferencedObjects);
    }
    
    virtual FString GetReferencerName() const override
    {
        return TEXT("FMyHelper");
    }
};
```

#### 3. UPROPERTY() (UObject 成员)

```cpp
UPROPERTY()
TArray<UObject*> ProcessingObjects;  // 自动 GC 追踪
```

### 方法选择指南

| 场景 | 推荐方案 |
|-----|---------|
| 异步任务持有 UObject | `TStrongObjectPtr` |
| 非 UObject 类持有引用 | `FGCObject` 继承 |
| UObject 成员变量 | `UPROPERTY()` |
| 可选/观察引用 | `TWeakObjectPtr` |

### Interchange Pipeline 中的 GC 注意事项

```cpp
void UMyPipeline::ExecutePostImportPipeline(
    const UInterchangeBaseNodeContainer* InBaseNodeContainer,
    const FString& NodeKey,
    UObject* CreatedAsset,
    bool bIsAReimport)
{
    // CreatedAsset 由引擎管理，无需额外保护
    // 但如果需要异步处理，应使用 TStrongObjectPtr
    
    if (UMaterialInterface* Material = Cast<UMaterialInterface>(CreatedAsset))
    {
        // 同步操作 - 安全
        CreateMaterialInstance(Material);
    }
}
```

---

## 参考资源

- [UE5 Interchange 官方文档](https://dev.epicgames.com/documentation/en-us/unreal-engine/interchange-framework-in-unreal-engine)
- [Interchange Pipeline 教程](https://dev.epicgames.com/community/learning/tutorials/dp77/unreal-engine-import-customization-with-interchange-5-4-5-5)
- 引擎源码路径: `Engine/Plugins/Interchange/` 和 `Engine/Source/Runtime/Interchange/`

---

*文档版本: 1.1 | 更新日期: 2025-12-17 | 基于 UE 5.5*
*v1.1: 新增编辑器工具异步与 GC 安全章节*
