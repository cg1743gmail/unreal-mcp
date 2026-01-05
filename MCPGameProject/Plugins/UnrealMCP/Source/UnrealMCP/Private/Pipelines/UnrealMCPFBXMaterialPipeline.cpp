// Copyright Epic Games, Inc. All Rights Reserved.
// UnrealMCP FBX Material Instance Pipeline Implementation

#include "Pipelines/UnrealMCPFBXMaterialPipeline.h"

// Interchange Core headers
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "InterchangeSourceData.h"

// Interchange Factory Nodes
#include "InterchangeMaterialFactoryNode.h"
#include "InterchangeTextureFactoryNode.h"

// Interchange Nodes (from InterchangeNodes module)
#include "InterchangeShaderGraphNode.h"

#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "EditorAssetLibrary.h"
#include "UObject/SavePackage.h"

UUnrealMCPFBXMaterialPipeline::UUnrealMCPFBXMaterialPipeline()
{
	// Initialize default settings
	bAutoCreateMaterialInstances = true;
	bSearchExistingMaterials = true;
	bAutoAssignTextures = true;
	MaterialInstanceSubFolder = TEXT("MaterialInstances");

	// Initialize default texture parameter mappings
	InitializeDefaultTextureMappings();
}

void UUnrealMCPFBXMaterialPipeline::InitializeDefaultTextureMappings()
{
	// Standard PBR texture parameter mappings
	TextureParameterMapping.Add(TEXT("BaseColor"), TEXT("BaseColorTexture"));
	TextureParameterMapping.Add(TEXT("Diffuse"), TEXT("BaseColorTexture"));
	TextureParameterMapping.Add(TEXT("Albedo"), TEXT("BaseColorTexture"));
	TextureParameterMapping.Add(TEXT("Normal"), TEXT("NormalTexture"));
	TextureParameterMapping.Add(TEXT("Roughness"), TEXT("RoughnessTexture"));
	TextureParameterMapping.Add(TEXT("Metallic"), TEXT("MetallicTexture"));
	TextureParameterMapping.Add(TEXT("AO"), TEXT("AmbientOcclusionTexture"));
	TextureParameterMapping.Add(TEXT("AmbientOcclusion"), TEXT("AmbientOcclusionTexture"));
	TextureParameterMapping.Add(TEXT("Emissive"), TEXT("EmissiveTexture"));
	TextureParameterMapping.Add(TEXT("Opacity"), TEXT("OpacityTexture"));
	TextureParameterMapping.Add(TEXT("Height"), TEXT("HeightTexture"));
	TextureParameterMapping.Add(TEXT("Displacement"), TEXT("HeightTexture"));

	// Texture suffix mappings (e.g., Character_D.png, Character_NRA.png)
	TextureParameterMapping.Add(TEXT("_D"), TEXT("BaseColorTexture"));
	TextureParameterMapping.Add(TEXT("_NRA"), TEXT("PackedTexture"));
	TextureParameterMapping.Add(TEXT("_N"), TEXT("NormalTexture"));
	TextureParameterMapping.Add(TEXT("_R"), TEXT("RoughnessTexture"));
	TextureParameterMapping.Add(TEXT("_M"), TEXT("MetallicTexture"));
	TextureParameterMapping.Add(TEXT("_AO"), TEXT("AmbientOcclusionTexture"));
}

void UUnrealMCPFBXMaterialPipeline::ExecutePipeline(
	UInterchangeBaseNodeContainer* InBaseNodeContainer,
	const TArray<UInterchangeSourceData*>& InSourceDatas,
	const FString& ContentBasePath)
{
	// Store content base path for later use
	CurrentContentBasePath = ContentBasePath;

	// Clear caches for new import
	CreatedMaterialInstances.Empty();
	ImportedTextures.Empty();

	// Call parent implementation first
	Super::ExecutePipeline(InBaseNodeContainer, InSourceDatas, ContentBasePath);

	UE_LOG(LogTemp, Log, TEXT("UnrealMCPFBXMaterialPipeline: ExecutePipeline called. ContentBasePath: %s"), *ContentBasePath);

	// If auto-create material instances is enabled, configure material factory nodes
	if (bAutoCreateMaterialInstances)
	{
		// Iterate through all material factory nodes
		InBaseNodeContainer->IterateNodesOfType<UInterchangeMaterialFactoryNode>(
			[this](const FString& NodeUid, UInterchangeMaterialFactoryNode* MaterialFactoryNode)
			{
				if (MaterialFactoryNode)
				{
					UE_LOG(LogTemp, Log, TEXT("UnrealMCPFBXMaterialPipeline: Processing material factory node: %s"), *MaterialFactoryNode->GetDisplayLabel());
					
					// Enable material import
					MaterialFactoryNode->SetEnabled(true);
				}
			}
		);

		// Cache texture factory nodes for later use
		InBaseNodeContainer->IterateNodesOfType<UInterchangeTextureFactoryNode>(
			[this](const FString& NodeUid, UInterchangeTextureFactoryNode* TextureFactoryNode)
			{
				if (TextureFactoryNode)
				{
					UE_LOG(LogTemp, Log, TEXT("UnrealMCPFBXMaterialPipeline: Found texture factory node: %s"), *TextureFactoryNode->GetDisplayLabel());
				}
			}
		);
	}
}

void UUnrealMCPFBXMaterialPipeline::ExecutePostImportPipeline(
	const UInterchangeBaseNodeContainer* InBaseNodeContainer,
	const FString& NodeKey,
	UObject* CreatedAsset,
	bool bIsAReimport)
{
	// Call parent implementation
	Super::ExecutePostImportPipeline(InBaseNodeContainer, NodeKey, CreatedAsset, bIsAReimport);

	if (!CreatedAsset)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("UnrealMCPFBXMaterialPipeline: ExecutePostImportPipeline for asset: %s (Class: %s)"),
		*CreatedAsset->GetName(), *CreatedAsset->GetClass()->GetName());

	// Handle imported textures - cache them for material instance configuration
	if (UTexture* ImportedTexture = Cast<UTexture>(CreatedAsset))
	{
		FString TextureName = ImportedTexture->GetName();
		ImportedTextures.Add(TextureName, ImportedTexture);
		UE_LOG(LogTemp, Log, TEXT("UnrealMCPFBXMaterialPipeline: Cached imported texture: %s"), *TextureName);
	}

	// Handle imported materials - create material instances
	if (UMaterialInterface* ImportedMaterial = Cast<UMaterialInterface>(CreatedAsset))
	{
		if (bAutoCreateMaterialInstances)
		{
			// Determine package path for material instance
			FString MaterialPath = ImportedMaterial->GetPathName();
			FString PackagePath = FPaths::GetPath(MaterialPath);
			
			if (!MaterialInstanceSubFolder.IsEmpty())
			{
				PackagePath = FPaths::Combine(PackagePath, MaterialInstanceSubFolder);
			}

			// Create material instance name
			FString InstanceName = FString::Printf(TEXT("MI_%s"), *ImportedMaterial->GetName());

			// Create material instance
			UMaterialInstanceConstant* NewInstance = CreateMaterialInstanceFromMaterial(
				ImportedMaterial,
				InstanceName,
				PackagePath
			);

			if (NewInstance)
			{
				CreatedMaterialInstances.Add(ImportedMaterial->GetName(), NewInstance);
				UE_LOG(LogTemp, Log, TEXT("UnrealMCPFBXMaterialPipeline: Created material instance: %s"), *NewInstance->GetPathName());

				// Configure textures if enabled
				if (bAutoAssignTextures && ImportedTextures.Num() > 0)
				{
					ConfigureMaterialInstanceTextures(NewInstance, ImportedTextures);
				}
			}
		}
	}
}

UMaterialInstanceConstant* UUnrealMCPFBXMaterialPipeline::CreateMaterialInstanceFromMaterial(
	UMaterialInterface* SourceMaterial,
	const FString& InstanceName,
	const FString& PackagePath)
{
	if (!SourceMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealMCPFBXMaterialPipeline: Cannot create material instance - source material is null"));
		return nullptr;
	}

	// Determine parent material
	UMaterialInterface* ParentMat = GetParentMaterialForInstance();
	if (!ParentMat)
	{
		// Use the source material as parent if no custom parent is set
		ParentMat = SourceMaterial;
	}

	// Create package path
	FString FullPath = FPaths::Combine(PackagePath, InstanceName);
	
	// Check if already exists
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		UE_LOG(LogTemp, Log, TEXT("UnrealMCPFBXMaterialPipeline: Material instance already exists: %s"), *FullPath);
		return Cast<UMaterialInstanceConstant>(UEditorAssetLibrary::LoadAsset(FullPath));
	}

	// Create the package
	UPackage* Package = CreatePackage(*FullPath);
	if (!Package)
	{
		UE_LOG(LogTemp, Error, TEXT("UnrealMCPFBXMaterialPipeline: Failed to create package: %s"), *FullPath);
		return nullptr;
	}

	// Create material instance using factory
	UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
	Factory->InitialParent = ParentMat;

	UMaterialInstanceConstant* NewInstance = Cast<UMaterialInstanceConstant>(
		Factory->FactoryCreateNew(
			UMaterialInstanceConstant::StaticClass(),
			Package,
			*InstanceName,
			RF_Standalone | RF_Public,
			nullptr,
			GWarn
		)
	);

	if (NewInstance)
	{
		// Mark package dirty
		Package->MarkPackageDirty();

		// Notify asset registry
		FAssetRegistryModule::AssetCreated(NewInstance);

		UE_LOG(LogTemp, Log, TEXT("UnrealMCPFBXMaterialPipeline: Successfully created material instance: %s"), *FullPath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UnrealMCPFBXMaterialPipeline: Failed to create material instance: %s"), *InstanceName);
	}

	return NewInstance;
}

void UUnrealMCPFBXMaterialPipeline::ConfigureMaterialInstanceTextures(
	UMaterialInstanceConstant* MaterialInstance,
	const TMap<FString, UTexture*>& TextureMap)
{
	if (!MaterialInstance)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("UnrealMCPFBXMaterialPipeline: Configuring textures for material instance: %s"), *MaterialInstance->GetName());

	// Iterate through texture map and try to assign to material parameters
	for (const auto& TexturePair : TextureMap)
	{
		const FString& TextureName = TexturePair.Key;
		UTexture* Texture = TexturePair.Value;

		if (!Texture)
		{
			continue;
		}

		// Try to find matching parameter name based on texture name
		for (const auto& Mapping : TextureParameterMapping)
		{
			if (TextureName.Contains(Mapping.Key, ESearchCase::IgnoreCase))
			{
				FName ParameterName(*Mapping.Value);
				
				// Check if the material has this parameter
				UTexture* ExistingTexture = nullptr;
				if (MaterialInstance->GetTextureParameterValue(ParameterName, ExistingTexture))
				{
					// Set the texture parameter
					MaterialInstance->SetTextureParameterValueEditorOnly(ParameterName, Texture);
					UE_LOG(LogTemp, Log, TEXT("UnrealMCPFBXMaterialPipeline: Set texture parameter %s = %s"),
						*Mapping.Value, *TextureName);
					break;
				}
			}
		}
	}

	// Mark as modified
	MaterialInstance->MarkPackageDirty();
}

UMaterialInterface* UUnrealMCPFBXMaterialPipeline::GetParentMaterialForInstance() const
{
	// Try to load the configured parent material (FSoftObjectPath from parent class)
	if (!ParentMaterial.IsNull())
	{
		UMaterialInterface* LoadedMaterial = Cast<UMaterialInterface>(ParentMaterial.TryLoad());
		if (LoadedMaterial)
		{
			return LoadedMaterial;
		}
	}

	// Return nullptr to use source material as parent
	return nullptr;
}
