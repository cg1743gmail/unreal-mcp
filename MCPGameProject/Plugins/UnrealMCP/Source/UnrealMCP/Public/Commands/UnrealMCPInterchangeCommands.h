#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Interchange-related MCP commands
 * Supports UE 5.6+ Interchange system for importing and creating assets
 * Based on Epic Games best practices for Interchange pipelines
 */
class UNREALMCP_API FUnrealMCPInterchangeCommands
{
public:
	FUnrealMCPInterchangeCommands();
	~FUnrealMCPInterchangeCommands();

	// Handle interchange commands
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// Specific interchange command handlers
	TSharedPtr<FJsonObject> HandleImportModel(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCreateInterchangeBlueprint(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCreateCustomInterchangeBlueprint(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetInterchangeAssets(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleReimportAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetInterchangeInfo(const TSharedPtr<FJsonObject>& Params);

	// Helper functions for Interchange operations
	bool IsValidInterchangeFile(const FString& FilePath) const;
	TArray<FString> GetSupportedInterchangeFormats() const;
	TSharedPtr<FJsonObject> GetAssetMetadata(const FString& AssetPath) const;
	
	// Interchange Blueprint utilities
	class UInterchangeGenericAsset* CreateInterchangeAsset(const FString& AssetName, const FString& PackagePath);
	bool ConfigureInterchangeAssetWithMesh(class UInterchangeGenericAsset* Asset, const FString& MeshPath);
	bool SetupInterchangeBlueprintPipeline(const FString& BlueprintPath, const TSharedPtr<FJsonObject>& PipelineConfig);
};
