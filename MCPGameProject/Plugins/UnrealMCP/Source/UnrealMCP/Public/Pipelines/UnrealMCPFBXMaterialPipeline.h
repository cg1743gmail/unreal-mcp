// Copyright Epic Games, Inc. All Rights Reserved.
// UnrealMCP FBX Material Instance Pipeline

#pragma once

#include "CoreMinimal.h"
#include "InterchangePipelineBase.h"
#include "InterchangeGenericMaterialPipeline.h"
#include "UnrealMCPFBXMaterialPipeline.generated.h"

class UMaterialInterface;
class UMaterialInstanceConstant;

/**
 * Custom Interchange Pipeline for automatic FBX Material Instance setup.
 * 
 * This pipeline extends the GenericMaterialPipeline to automatically:
 * 1. Create Material Instances from imported materials
 * 2. Configure material instance parameters based on FBX data
 * 3. Apply parent material templates
 * 
 * Best Practices for Interchange Pipeline:
 * - Override ScriptedExecutePostImportPipeline for post-import processing
 * - Use Factory Nodes to configure import settings before asset creation
 * - Access BaseNodeContainer to iterate through imported nodes
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, meta=(DisplayName="FBX Material Instance Pipeline"))
class UNREALMCP_API UUnrealMCPFBXMaterialPipeline : public UInterchangeGenericMaterialPipeline
{
	GENERATED_BODY()

public:
	UUnrealMCPFBXMaterialPipeline();

	//~ Begin UInterchangePipelineBase Interface
	virtual void ExecutePipeline(UInterchangeBaseNodeContainer* InBaseNodeContainer, const TArray<UInterchangeSourceData*>& InSourceDatas, const FString& ContentBasePath) override;
	virtual void ExecutePostImportPipeline(const UInterchangeBaseNodeContainer* InBaseNodeContainer, const FString& NodeKey, UObject* CreatedAsset, bool bIsAReimport) override;
	//~ End UInterchangePipelineBase Interface

	// NOTE: ParentMaterial is inherited from UInterchangeGenericMaterialPipeline

	/** If true, automatically create material instances for all imported materials */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Instance Settings")
	bool bAutoCreateMaterialInstances = true;

	/** If true, search for existing materials before creating new ones */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Instance Settings")
	bool bSearchExistingMaterials = true;

	/** Folder path for created material instances (relative to import destination) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Instance Settings")
	FString MaterialInstanceSubFolder = TEXT("MaterialInstances");

	/** If true, automatically assign textures to material instance parameters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Instance Settings")
	bool bAutoAssignTextures = true;

	/** Mapping of texture types to material parameter names */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Instance Settings")
	TMap<FString, FString> TextureParameterMapping;

protected:
	/** Create a material instance from the given material */
	UFUNCTION(BlueprintCallable, Category = "Material Instance")
	UMaterialInstanceConstant* CreateMaterialInstanceFromMaterial(UMaterialInterface* SourceMaterial, const FString& InstanceName, const FString& PackagePath);

	/** Configure material instance parameters based on imported textures */
	UFUNCTION(BlueprintCallable, Category = "Material Instance")
	void ConfigureMaterialInstanceTextures(UMaterialInstanceConstant* MaterialInstance, const TMap<FString, UTexture*>& TextureMap);

	/** Find or create parent material for material instances */
	UFUNCTION(BlueprintCallable, Category = "Material Instance")
	UMaterialInterface* GetParentMaterialForInstance() const;

private:
	/** Initialize default texture parameter mappings */
	void InitializeDefaultTextureMappings();

	/** Cache for created material instances during import */
	TMap<FString, UMaterialInstanceConstant*> CreatedMaterialInstances;

	/** Cache for imported textures */
	TMap<FString, UTexture*> ImportedTextures;

	/** Content base path for current import operation */
	FString CurrentContentBasePath;
};
