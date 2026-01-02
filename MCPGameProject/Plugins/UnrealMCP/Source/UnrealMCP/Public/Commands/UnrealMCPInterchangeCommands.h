#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Interchange-related MCP commands
 * Supports UE 5.5+ Interchange system for importing and creating assets
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
	
	// Interchange Pipeline Blueprint commands
	TSharedPtr<FJsonObject> HandleCreateInterchangePipelineBlueprint(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetInterchangePipelines(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleConfigureInterchangePipeline(const TSharedPtr<FJsonObject>& Params);
	
	// Interchange Pipeline Graph Node Operations
	TSharedPtr<FJsonObject> HandleGetInterchangePipelineGraph(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddInterchangePipelineFunctionOverride(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddInterchangePipelineNode(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleConnectInterchangePipelineNodes(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleFindInterchangePipelineNodes(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddInterchangeIterateNodesBlock(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCompileInterchangePipeline(const TSharedPtr<FJsonObject>& Params);

	// Helper functions for Interchange operations
	bool IsValidInterchangeFile(const FString& FilePath) const;
	TArray<FString> GetSupportedInterchangeFormats() const;
	TSharedPtr<FJsonObject> GetAssetMetadata(const FString& AssetPath) const;
	
	// Helper functions for Pipeline graph operations
	UBlueprint* LoadPipelineBlueprint(const FString& PipelinePath) const;
	class UEdGraph* FindOrCreateFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName) const;
};
