#include "Commands/UnrealMCPInterchangeCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

// Asset-specific headers
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Animation/Skeleton.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"

// Blueprint support
#include "Engine/Blueprint.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"

// Editor reimport
#include "EditorReimportHandler.h"

// Interchange headers
#include "InterchangeManager.h"
#include "InterchangePipelineBase.h"
#include "InterchangeSourceData.h"
#include "InterchangeFactoryBase.h"
#include "InterchangeAssetImportData.h"
#include "InterchangeBlueprintPipelineBase.h"
#include "InterchangeGenericAssetsPipeline.h"
#include "InterchangeGenericMaterialPipeline.h"
#include "InterchangeGenericMeshPipeline.h"
#include "InterchangeGenericTexturePipeline.h"

// Custom FBX Material Pipeline
#include "Pipelines/UnrealMCPFBXMaterialPipeline.h"

// Blueprint Graph Node support
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "ScopedTransaction.h"

FUnrealMCPInterchangeCommands::FUnrealMCPInterchangeCommands()
{
}

FUnrealMCPInterchangeCommands::~FUnrealMCPInterchangeCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPInterchangeCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("import_model"))
	{
		return HandleImportModel(Params);
	}
	else if (CommandType == TEXT("create_interchange_blueprint"))
	{
		return HandleCreateInterchangeBlueprint(Params);
	}
	else if (CommandType == TEXT("create_custom_interchange_blueprint"))
	{
		return HandleCreateCustomInterchangeBlueprint(Params);
	}
	else if (CommandType == TEXT("get_interchange_assets"))
	{
		return HandleGetInterchangeAssets(Params);
	}
	else if (CommandType == TEXT("reimport_asset"))
	{
		return HandleReimportAsset(Params);
	}
	else if (CommandType == TEXT("get_interchange_info"))
	{
		return HandleGetInterchangeInfo(Params);
	}
	else if (CommandType == TEXT("create_interchange_pipeline_blueprint"))
	{
		return HandleCreateInterchangePipelineBlueprint(Params);
	}
	else if (CommandType == TEXT("get_interchange_pipelines"))
	{
		return HandleGetInterchangePipelines(Params);
	}
	else if (CommandType == TEXT("configure_interchange_pipeline"))
	{
		return HandleConfigureInterchangePipeline(Params);
	}
	// Interchange Pipeline Graph Node Operations
	else if (CommandType == TEXT("get_interchange_pipeline_graph"))
	{
		return HandleGetInterchangePipelineGraph(Params);
	}
	else if (CommandType == TEXT("add_interchange_pipeline_function_override"))
	{
		return HandleAddInterchangePipelineFunctionOverride(Params);
	}
	else if (CommandType == TEXT("add_interchange_pipeline_node"))
	{
		return HandleAddInterchangePipelineNode(Params);
	}
	else if (CommandType == TEXT("connect_interchange_pipeline_nodes"))
	{
		return HandleConnectInterchangePipelineNodes(Params);
	}
	else if (CommandType == TEXT("find_interchange_pipeline_nodes"))
	{
		return HandleFindInterchangePipelineNodes(Params);
	}
	else if (CommandType == TEXT("add_interchange_iterate_nodes_block"))
	{
		return HandleAddInterchangeIterateNodesBlock(Params);
	}
	else if (CommandType == TEXT("compile_interchange_pipeline"))
	{
		return HandleCompileInterchangePipeline(Params);
	}

	return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown interchange command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPInterchangeCommands::HandleImportModel(const TSharedPtr<FJsonObject>& Params)
{
	// Validate required parameters
	FString FilePath;
	if (!Params->TryGetStringField(TEXT("file_path"), FilePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'file_path' parameter"));
	}

	FString DestinationPath;
	if (!Params->TryGetStringField(TEXT("destination_path"), DestinationPath))
	{
		DestinationPath = TEXT("/Game/Imported");
	}

	// Validate file exists and is supported
	if (!FPaths::FileExists(FilePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("File not found: %s"), *FilePath));
	}

	if (!IsValidInterchangeFile(FilePath))
	{
		TArray<FString> SupportedFormats = GetSupportedInterchangeFormats();
		FString FormatsStr = FString::Join(SupportedFormats, TEXT(", "));
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unsupported file format. Supported formats: %s"), *FormatsStr));
	}

	// Ensure destination path starts with /Game/
	if (!DestinationPath.StartsWith(TEXT("/Game/")))
	{
		DestinationPath = TEXT("/Game/") + DestinationPath;
	}

	// Get import settings from params
	bool bImportMesh = true;
	bool bImportMaterial = true;
	bool bImportTexture = true;
	bool bImportSkeleton = true;
	bool bCreatePhysicsAsset = false;

	Params->TryGetBoolField(TEXT("import_mesh"), bImportMesh);
	Params->TryGetBoolField(TEXT("import_material"), bImportMaterial);
	Params->TryGetBoolField(TEXT("import_texture"), bImportTexture);
	Params->TryGetBoolField(TEXT("import_skeleton"), bImportSkeleton);
	Params->TryGetBoolField(TEXT("create_physics_asset"), bCreatePhysicsAsset);

	// Get file info
	FString FileExtension = FPaths::GetExtension(FilePath).ToLower();
	FString FileName = FPaths::GetBaseFilename(FilePath);

	// Note: For full Interchange import, use UE Editor's Import dialog
	// This command provides the configuration and validation
	// The actual import should be triggered via Editor or Interchange Manager
	
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("destination"), DestinationPath);
	ResultObj->SetStringField(TEXT("source_file"), FilePath);
	ResultObj->SetStringField(TEXT("message"), TEXT("Import configuration validated. Use UE Editor to complete import or call reimport_asset for existing assets."));
	
	// Return import settings
	TSharedPtr<FJsonObject> SettingsObj = MakeShared<FJsonObject>();
	SettingsObj->SetBoolField(TEXT("import_mesh"), bImportMesh);
	SettingsObj->SetBoolField(TEXT("import_material"), bImportMaterial);
	SettingsObj->SetBoolField(TEXT("import_texture"), bImportTexture);
	SettingsObj->SetBoolField(TEXT("import_skeleton"), bImportSkeleton);
	SettingsObj->SetBoolField(TEXT("create_physics_asset"), bCreatePhysicsAsset);
	ResultObj->SetObjectField(TEXT("import_settings"), SettingsObj);
	
	// Return file info
	TSharedPtr<FJsonObject> FileInfoObj = MakeShared<FJsonObject>();
	FileInfoObj->SetStringField(TEXT("filename"), FPaths::GetCleanFilename(FilePath));
	FileInfoObj->SetStringField(TEXT("extension"), FileExtension);
	FileInfoObj->SetNumberField(TEXT("size"), static_cast<double>(IFileManager::Get().FileSize(*FilePath)));
	ResultObj->SetObjectField(TEXT("file_info"), FileInfoObj);

	UE_LOG(LogTemp, Log, TEXT("Interchange import validated for: %s"), *FilePath);
	
	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPInterchangeCommands::HandleCreateInterchangeBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	FString MeshPath;
	if (!Params->TryGetStringField(TEXT("mesh_path"), MeshPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'mesh_path' parameter"));
	}

	// Destination path for the blueprint (safe + configurable)
	FString FolderPath = FUnrealMCPCommonUtils::GetDefaultBlueprintFolder();
	Params->TryGetStringField(TEXT("package_path"), FolderPath);
	Params->TryGetStringField(TEXT("folder_path"), FolderPath);

	FString RequestedAssetPath;
	Params->TryGetStringField(TEXT("asset_path"), RequestedAssetPath);
	Params->TryGetStringField(TEXT("blueprint_path"), RequestedAssetPath); // alias

	FString FullAssetPath;
	FString Err;
	if (!RequestedAssetPath.IsEmpty())
	{
		if (!FUnrealMCPCommonUtils::NormalizeLongPackageAssetPath(RequestedAssetPath, FullAssetPath, Err))
		{
			return FUnrealMCPCommonUtils::CreateErrorResponseEx(TEXT("Invalid asset_path"), TEXT("ERR_INVALID_PATH"), Err);
		}
	}
	else
	{
		if (!FUnrealMCPCommonUtils::NormalizeLongPackageFolder(FolderPath, FolderPath, Err))
		{
			return FUnrealMCPCommonUtils::CreateErrorResponseEx(TEXT("Invalid folder_path"), TEXT("ERR_INVALID_PATH"), Err);
		}
		FullAssetPath = FolderPath + BlueprintName;
	}

	if (!FUnrealMCPCommonUtils::IsWritePathAllowed(FullAssetPath, Err))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponseEx(TEXT("Write path not allowed"), TEXT("ERR_WRITE_PATH_NOT_ALLOWED"), Err);
	}

	FString ObjectPath;
	if (!FUnrealMCPCommonUtils::MakeObjectPathFromAssetPath(FullAssetPath, ObjectPath, Err))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponseEx(TEXT("Invalid destination path"), TEXT("ERR_INVALID_PATH"), Err);
	}

	// Check if blueprint already exists
	if (UEditorAssetLibrary::DoesAssetExist(ObjectPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint already exists: %s"), *BlueprintName));
	}


	// Load the mesh
	UObject* MeshObject = UEditorAssetLibrary::LoadAsset(MeshPath);
	if (!MeshObject)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Mesh not found: %s"), *MeshPath));
	}

	// Determine mesh type and parent class
	UClass* ParentClass = AActor::StaticClass();
	UClass* ComponentClass = nullptr;
	
	if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(MeshObject))
	{
		ParentClass = AActor::StaticClass();
		ComponentClass = UStaticMeshComponent::StaticClass();
	}
	else if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(MeshObject))
	{
		ParentClass = APawn::StaticClass();
		ComponentClass = USkeletalMeshComponent::StaticClass();
	}
	else
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Mesh type is not supported (must be StaticMesh or SkeletalMesh)"));
	}

	// Create Blueprint
	UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
	Factory->ParentClass = ParentClass;

	UPackage* Package = CreatePackage(*FullAssetPath);

	UBlueprint* NewBlueprint = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *BlueprintName, RF_Standalone | RF_Public, nullptr, GWarn));

	if (!NewBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create blueprint"));
	}

	// Add mesh component to blueprint
	if (ComponentClass && NewBlueprint->SimpleConstructionScript)
	{
		USimpleConstructionScript* SCS = NewBlueprint->SimpleConstructionScript;
		
		// Create component node
		USCS_Node* ComponentNode = SCS->CreateNode(ComponentClass, FName(TEXT("MeshComponent")));
		if (ComponentNode)
		{
			// Add as root node
			SCS->AddNode(ComponentNode);
			
			// Set the mesh property
			if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(ComponentNode->ComponentTemplate))
			{
				SMC->SetStaticMesh(Cast<UStaticMesh>(MeshObject));
			}
			else if (USkeletalMeshComponent* SkMC = Cast<USkeletalMeshComponent>(ComponentNode->ComponentTemplate))
			{
				SkMC->SetSkeletalMesh(Cast<USkeletalMesh>(MeshObject));
			}

			UE_LOG(LogTemp, Log, TEXT("Successfully added mesh component to blueprint"));
		}
	}

	// Mark package dirty and notify asset registry
	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(NewBlueprint);

	// Compile the blueprint
	FKismetEditorUtilities::CompileBlueprint(NewBlueprint);

	// Prepare result
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("name"), BlueprintName);
	ResultObj->SetStringField(TEXT("path"), FullAssetPath); // legacy
	ResultObj->SetStringField(TEXT("object_path"), ObjectPath); // legacy
	FUnrealMCPCommonUtils::AddResolvedAssetFields(ResultObj, FullAssetPath);

	ResultObj->SetStringField(TEXT("mesh_path"), MeshPath);

	ResultObj->SetStringField(TEXT("component_type"), ComponentClass ? ComponentClass->GetName() : TEXT("None"));

	UE_LOG(LogTemp, Log, TEXT("Successfully created Interchange Blueprint: %s"), *BlueprintName);

	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPInterchangeCommands::HandleCreateCustomInterchangeBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	// Destination path (safe + configurable)
	FString FolderPath = FUnrealMCPCommonUtils::GetDefaultBlueprintFolder();
	Params->TryGetStringField(TEXT("package_path"), FolderPath);
	Params->TryGetStringField(TEXT("folder_path"), FolderPath);

	FString RequestedAssetPath;
	Params->TryGetStringField(TEXT("asset_path"), RequestedAssetPath);
	Params->TryGetStringField(TEXT("blueprint_path"), RequestedAssetPath); // alias

	FString FullAssetPath;
	FString Err;
	if (!RequestedAssetPath.IsEmpty())
	{
		if (!FUnrealMCPCommonUtils::NormalizeLongPackageAssetPath(RequestedAssetPath, FullAssetPath, Err))
		{
			return FUnrealMCPCommonUtils::CreateErrorResponseEx(TEXT("Invalid asset_path"), TEXT("ERR_INVALID_PATH"), Err);
		}
	}
	else
	{
		if (!FUnrealMCPCommonUtils::NormalizeLongPackageFolder(FolderPath, FolderPath, Err))
		{
			return FUnrealMCPCommonUtils::CreateErrorResponseEx(TEXT("Invalid folder_path"), TEXT("ERR_INVALID_PATH"), Err);
		}
		FullAssetPath = FolderPath + BlueprintName;
	}

	if (!FUnrealMCPCommonUtils::IsWritePathAllowed(FullAssetPath, Err))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponseEx(TEXT("Write path not allowed"), TEXT("ERR_WRITE_PATH_NOT_ALLOWED"), Err);
	}

	FString ObjectPath;
	if (!FUnrealMCPCommonUtils::MakeObjectPathFromAssetPath(FullAssetPath, ObjectPath, Err))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponseEx(TEXT("Invalid destination path"), TEXT("ERR_INVALID_PATH"), Err);
	}

	if (UEditorAssetLibrary::DoesAssetExist(ObjectPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint already exists: %s"), *BlueprintName));
	}


	// Get optional parent class
	FString ParentClassName;
	Params->TryGetStringField(TEXT("parent_class"), ParentClassName);
	if (ParentClassName.IsEmpty())
	{
		ParentClassName = TEXT("Actor");
	}

	// Resolve parent class
	UClass* ParentClass = AActor::StaticClass();
	if (!ParentClassName.IsEmpty())
	{
		FString ClassName = ParentClassName;
		if (!ClassName.StartsWith(TEXT("A")))
		{
			ClassName = TEXT("A") + ClassName;
		}

		UClass* FoundClass = nullptr;
		if (ClassName == TEXT("AActor"))
		{
			FoundClass = AActor::StaticClass();
		}
		else if (ClassName == TEXT("APawn"))
		{
			FoundClass = APawn::StaticClass();
		}
		else if (ClassName == TEXT("ACharacter"))
		{
			FoundClass = ACharacter::StaticClass();
		}
		else
		{
			const FString ClassPath = FString::Printf(TEXT("/Script/Engine.%s"), *ClassName);
			FoundClass = LoadClass<AActor>(nullptr, *ClassPath);
		}

		if (FoundClass)
		{
			ParentClass = FoundClass;
		}
	}

	// Create the base Blueprint
	UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
	Factory->ParentClass = ParentClass;

	UPackage* Package = CreatePackage(*FullAssetPath);

	UBlueprint* NewBlueprint = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *BlueprintName, RF_Standalone | RF_Public, nullptr, GWarn));

	if (!NewBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create blueprint"));
	}

	USimpleConstructionScript* SCS = NewBlueprint->SimpleConstructionScript;

	// Add mesh component if mesh_path provided
	if (Params->HasField(TEXT("mesh_path")))
	{
		FString MeshPath;
		Params->TryGetStringField(TEXT("mesh_path"), MeshPath);

		if (!MeshPath.IsEmpty())
		{
			UObject* MeshObject = UEditorAssetLibrary::LoadAsset(MeshPath);
			if (MeshObject && SCS)
			{
				UClass* ComponentClass = nullptr;

				if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(MeshObject))
				{
					ComponentClass = UStaticMeshComponent::StaticClass();
				}
				else if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(MeshObject))
				{
					ComponentClass = USkeletalMeshComponent::StaticClass();
				}

				if (ComponentClass)
				{
					USCS_Node* MeshNode = SCS->CreateNode(ComponentClass, FName(TEXT("MeshComponent")));
					if (MeshNode)
					{
						SCS->AddNode(MeshNode);

						if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(MeshNode->ComponentTemplate))
						{
							SMC->SetStaticMesh(Cast<UStaticMesh>(MeshObject));
						}
						else if (USkeletalMeshComponent* SkMC = Cast<USkeletalMeshComponent>(MeshNode->ComponentTemplate))
						{
							SkMC->SetSkeletalMesh(Cast<USkeletalMesh>(MeshObject));
						}
					}
				}
			}
		}
	}

	// Add custom components if provided
	if (Params->HasField(TEXT("components")))
	{
		const TArray<TSharedPtr<FJsonValue>>* ComponentsArray = nullptr;
		if (Params->TryGetArrayField(TEXT("components"), ComponentsArray) && SCS)
		{
			for (const TSharedPtr<FJsonValue>& ComponentValue : *ComponentsArray)
			{
				const TSharedPtr<FJsonObject>* ComponentObj = nullptr;
				if (ComponentValue->TryGetObject(ComponentObj))
				{
					FString ComponentType;
					if ((*ComponentObj)->TryGetStringField(TEXT("type"), ComponentType))
					{
						FString ComponentName;
						(*ComponentObj)->TryGetStringField(TEXT("name"), ComponentName);
						if (ComponentName.IsEmpty())
						{
							ComponentName = ComponentType;
						}

						// Find component class
						UClass* FoundComponentClass = nullptr;
						if (ComponentType == TEXT("SceneComponent"))
						{
							FoundComponentClass = USceneComponent::StaticClass();
						}
						else if (ComponentType == TEXT("StaticMeshComponent"))
						{
							FoundComponentClass = UStaticMeshComponent::StaticClass();
						}
						else if (ComponentType == TEXT("SkeletalMeshComponent"))
						{
							FoundComponentClass = USkeletalMeshComponent::StaticClass();
						}
						else if (ComponentType == TEXT("CapsuleComponent"))
						{
							FoundComponentClass = UCapsuleComponent::StaticClass();
						}
						else if (ComponentType == TEXT("BoxComponent"))
						{
							FoundComponentClass = UBoxComponent::StaticClass();
						}
						else if (ComponentType == TEXT("SphereComponent"))
						{
							FoundComponentClass = USphereComponent::StaticClass();
						}

						if (FoundComponentClass && FoundComponentClass->IsChildOf<UActorComponent>())
						{
							USCS_Node* ComponentNode = SCS->CreateNode(FoundComponentClass, FName(*ComponentName));
							if (ComponentNode)
							{
								SCS->AddNode(ComponentNode);
							}
						}
					}
				}
			}
		}
	}

	// Mark package dirty and notify asset registry
	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(NewBlueprint);

	// Compile the blueprint
	FKismetEditorUtilities::CompileBlueprint(NewBlueprint);

	// Prepare result
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("name"), BlueprintName);
	ResultObj->SetStringField(TEXT("path"), FullAssetPath); // legacy
	ResultObj->SetStringField(TEXT("object_path"), ObjectPath); // legacy
	FUnrealMCPCommonUtils::AddResolvedAssetFields(ResultObj, FullAssetPath);

	ResultObj->SetStringField(TEXT("parent_class"), ParentClass->GetName());
	ResultObj->SetStringField(TEXT("type"), TEXT("interchange_blueprint"));


	UE_LOG(LogTemp, Log, TEXT("Successfully created custom Interchange Blueprint: %s"), *BlueprintName);

	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPInterchangeCommands::HandleGetInterchangeAssets(const TSharedPtr<FJsonObject>& Params)
{
	// Get optional parameters
	FString SearchPath;
	Params->TryGetStringField(TEXT("search_path"), SearchPath);
	if (SearchPath.IsEmpty())
	{
		SearchPath = TEXT("/Game/");
	}

	// Get asset type filter
	FString AssetTypeFilter;
	Params->TryGetStringField(TEXT("asset_type"), AssetTypeFilter);

	// Query asset registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(*SearchPath);

	// Add class filters based on type
	if (!AssetTypeFilter.IsEmpty())
	{
		if (AssetTypeFilter == TEXT("static_mesh"))
		{
			Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
		}
		else if (AssetTypeFilter == TEXT("skeletal_mesh"))
		{
			Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
		}
		else if (AssetTypeFilter == TEXT("material"))
		{
			Filter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
		}
		else if (AssetTypeFilter == TEXT("texture"))
		{
			Filter.ClassPaths.Add(UTexture2D::StaticClass()->GetClassPathName());
		}
	}

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	// Build result
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> AssetsArray;

	for (const FAssetData& AssetData : AssetDataList)
	{
		TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
		AssetObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		AssetObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString()); // legacy (object path)
		AssetObj->SetStringField(TEXT("resolved_asset_path"), AssetData.PackageName.ToString());
		AssetObj->SetStringField(TEXT("object_path"), AssetData.GetObjectPathString());
		AssetObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());


		AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
	}

	ResultObj->SetArrayField(TEXT("assets"), AssetsArray);
	ResultObj->SetNumberField(TEXT("count"), static_cast<double>(AssetsArray.Num()));

	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPInterchangeCommands::HandleReimportAsset(const TSharedPtr<FJsonObject>& Params)
{
	// Get required parameters
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
	}

	// Normalize asset path (accept object path; return canonical long package asset path)
	FString NormalizedAssetPath;
	FString Err;
	if (!FUnrealMCPCommonUtils::NormalizeLongPackageAssetPath(AssetPath, NormalizedAssetPath, Err))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponseEx(TEXT("Invalid asset_path"), TEXT("ERR_INVALID_PATH"), Err);
	}
	AssetPath = NormalizedAssetPath;

	// Load the asset
	UObject* Asset = FUnrealMCPCommonUtils::LoadAssetByPathSmart(AssetPath);
	if (!Asset)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponseEx(FString::Printf(TEXT("Asset not found: %s"), *AssetPath), TEXT("ERR_ASSET_NOT_FOUND"), TEXT(""));
	}


	// Check if asset has import data
	if (!Asset->IsA<UStaticMesh>() && !Asset->IsA<USkeletalMesh>() && !Asset->IsA<UTexture2D>() && !Asset->IsA<UMaterial>())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Asset type does not support reimport"));
	}

	// Trigger reimport
	bool bReimportSuccess = FReimportManager::Instance()->Reimport(Asset, true);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), bReimportSuccess);
	ResultObj->SetStringField(TEXT("asset_path"), AssetPath); // legacy
	FUnrealMCPCommonUtils::AddResolvedAssetFields(ResultObj, AssetPath);
	ResultObj->SetStringField(TEXT("message"), bReimportSuccess ? TEXT("Asset reimport triggered") : TEXT("Reimport failed"));


	UE_LOG(LogTemp, Log, TEXT("Triggered reimport for asset: %s (success: %s)"), *AssetPath, bReimportSuccess ? TEXT("true") : TEXT("false"));

	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPInterchangeCommands::HandleGetInterchangeInfo(const TSharedPtr<FJsonObject>& Params)
{
	// Get the asset path
	FString AssetPath;
	Params->TryGetStringField(TEXT("asset_path"), AssetPath);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();

	// Get supported formats
	TArray<FString> SupportedFormats = GetSupportedInterchangeFormats();
	TArray<TSharedPtr<FJsonValue>> FormatsArray;
	for (const FString& Format : SupportedFormats)
	{
		FormatsArray.Add(MakeShared<FJsonValueString>(Format));
	}
	ResultObj->SetArrayField(TEXT("supported_formats"), FormatsArray);

	// If asset path provided, get metadata
	if (!AssetPath.IsEmpty())
	{
		TSharedPtr<FJsonObject> MetadataObj = GetAssetMetadata(AssetPath);
		if (MetadataObj)
		{
			ResultObj->SetObjectField(TEXT("asset_metadata"), MetadataObj);
		}
	}

	// Add engine version info
	ResultObj->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());
	ResultObj->SetStringField(TEXT("interchange_version"), TEXT("1.0"));

	return ResultObj;
}

bool FUnrealMCPInterchangeCommands::IsValidInterchangeFile(const FString& FilePath) const
{
	FString Extension = FPaths::GetExtension(FilePath).ToLower();
	TArray<FString> SupportedFormats = GetSupportedInterchangeFormats();
	return SupportedFormats.Contains(Extension);
}

TArray<FString> FUnrealMCPInterchangeCommands::GetSupportedInterchangeFormats() const
{
	// Based on Epic's Interchange system support in UE 5.5+
	TArray<FString> Formats;
	Formats.Add(TEXT("fbx"));
	Formats.Add(TEXT("gltf"));
	Formats.Add(TEXT("glb"));
	Formats.Add(TEXT("usdz"));
	Formats.Add(TEXT("usda"));
	Formats.Add(TEXT("usd"));
	Formats.Add(TEXT("abc"));
	Formats.Add(TEXT("obj"));
	Formats.Add(TEXT("ply"));
	return Formats;
}

TSharedPtr<FJsonObject> FUnrealMCPInterchangeCommands::GetAssetMetadata(const FString& AssetPath) const
{
	UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!Asset)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> MetadataObj = MakeShared<FJsonObject>();
	MetadataObj->SetStringField(TEXT("asset_name"), Asset->GetName());
	MetadataObj->SetStringField(TEXT("asset_class"), Asset->GetClass()->GetName());
	MetadataObj->SetStringField(TEXT("outer_name"), Asset->GetOutermost()->GetName());

	// Get asset-specific data
	if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset))
	{
		MetadataObj->SetStringField(TEXT("type"), TEXT("static_mesh"));
		MetadataObj->SetNumberField(TEXT("num_materials"), static_cast<double>(StaticMesh->GetStaticMaterials().Num()));
		MetadataObj->SetNumberField(TEXT("num_lods"), static_cast<double>(StaticMesh->GetNumLODs()));
	}
	else if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Asset))
	{
		MetadataObj->SetStringField(TEXT("type"), TEXT("skeletal_mesh"));
		MetadataObj->SetNumberField(TEXT("num_materials"), static_cast<double>(SkeletalMesh->GetMaterials().Num()));
		MetadataObj->SetNumberField(TEXT("num_bones"), static_cast<double>(SkeletalMesh->GetRefSkeleton().GetNum()));
	}
	else if (UTexture2D* Texture = Cast<UTexture2D>(Asset))
	{
		MetadataObj->SetStringField(TEXT("type"), TEXT("texture"));
		MetadataObj->SetNumberField(TEXT("width"), static_cast<double>(Texture->GetSizeX()));
		MetadataObj->SetNumberField(TEXT("height"), static_cast<double>(Texture->GetSizeY()));
	}
	else if (UMaterial* Material = Cast<UMaterial>(Asset))
	{
		MetadataObj->SetStringField(TEXT("type"), TEXT("material"));
		MetadataObj->SetStringField(TEXT("blend_mode"), StaticEnum<EBlendMode>()->GetNameStringByValue(static_cast<int64>(Material->BlendMode)));
	}

	return MetadataObj;
}

TSharedPtr<FJsonObject> FUnrealMCPInterchangeCommands::HandleCreateInterchangePipelineBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	// Get required parameters
	FString PipelineName;
	if (!Params->TryGetStringField(TEXT("name"), PipelineName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	// Get optional parameters
	FString FolderPath = TEXT("/Game/UnrealMCP/Interchange/Pipelines/");
	Params->TryGetStringField(TEXT("package_path"), FolderPath);
	Params->TryGetStringField(TEXT("folder_path"), FolderPath);

	FString Err;
	if (!FUnrealMCPCommonUtils::NormalizeLongPackageFolder(FolderPath, FolderPath, Err))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponseEx(TEXT("Invalid folder_path"), TEXT("ERR_INVALID_PATH"), Err);
	}

	FString FullAssetPath = FolderPath + PipelineName;

	if (!FUnrealMCPCommonUtils::IsWritePathAllowed(FullAssetPath, Err))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponseEx(TEXT("Write path not allowed"), TEXT("ERR_WRITE_PATH_NOT_ALLOWED"), Err);
	}

	FString ObjectPath;
	if (!FUnrealMCPCommonUtils::MakeObjectPathFromAssetPath(FullAssetPath, ObjectPath, Err))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponseEx(TEXT("Invalid destination path"), TEXT("ERR_INVALID_PATH"), Err);
	}

	if (UEditorAssetLibrary::DoesAssetExist(ObjectPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Pipeline blueprint already exists: %s"), *FullAssetPath));
	}


	// Get parent pipeline class (default to UInterchangeGenericAssetsPipeline)
	FString ParentClassName;
	Params->TryGetStringField(TEXT("parent_class"), ParentClassName);
	
	UClass* ParentPipelineClass = UInterchangeGenericAssetsPipeline::StaticClass();
	if (!ParentClassName.IsEmpty())
	{
		if (ParentClassName == TEXT("GenericAssetsPipeline") || ParentClassName == TEXT("UInterchangeGenericAssetsPipeline"))
		{
			ParentPipelineClass = UInterchangeGenericAssetsPipeline::StaticClass();
		}
		else if (ParentClassName == TEXT("GenericMaterialPipeline") || ParentClassName == TEXT("UInterchangeGenericMaterialPipeline"))
		{
			ParentPipelineClass = UInterchangeGenericMaterialPipeline::StaticClass();
		}
		else if (ParentClassName == TEXT("GenericMeshPipeline") || ParentClassName == TEXT("UInterchangeGenericMeshPipeline"))
		{
			ParentPipelineClass = UInterchangeGenericMeshPipeline::StaticClass();
		}
		else if (ParentClassName == TEXT("GenericTexturePipeline") || ParentClassName == TEXT("UInterchangeGenericTexturePipeline"))
		{
			ParentPipelineClass = UInterchangeGenericTexturePipeline::StaticClass();
		}
		else if (ParentClassName == TEXT("PipelineBase") || ParentClassName == TEXT("UInterchangePipelineBase"))
		{
			ParentPipelineClass = UInterchangePipelineBase::StaticClass();
		}
		else if (ParentClassName == TEXT("FBXMaterialPipeline") || ParentClassName == TEXT("UUnrealMCPFBXMaterialPipeline"))
		{
			// Custom FBX Material Instance Pipeline
			ParentPipelineClass = UUnrealMCPFBXMaterialPipeline::StaticClass();
		}
	}

	// Create package
	UPackage* Package = CreatePackage(*FullAssetPath);

	if (!Package)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for pipeline blueprint"));
	}

	// Determine the correct Blueprint class based on parent
	// For UInterchangeBlueprintPipelineBase derivatives, use that as BlueprintClass
	// For other Pipeline classes, use standard UBlueprint
	UClass* BlueprintClass = UBlueprint::StaticClass();
	UClass* BlueprintGeneratedClass = UBlueprintGeneratedClass::StaticClass();
	
	// Check if parent is derived from UInterchangeBlueprintPipelineBase
	if (ParentPipelineClass->IsChildOf(UInterchangeBlueprintPipelineBase::StaticClass()))
	{
		// Use the scripted pipeline blueprint approach
		BlueprintClass = UBlueprint::StaticClass();
	}

	// Create the Pipeline Blueprint using FKismetEditorUtilities
	UBlueprint* NewPipelineBlueprint = FKismetEditorUtilities::CreateBlueprint(
		ParentPipelineClass,
		Package,
		*PipelineName,
		BPTYPE_Normal,
		BlueprintClass,
		BlueprintGeneratedClass,
		NAME_None
	);

	if (!NewPipelineBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Interchange Pipeline Blueprint"));
	}

	// Mark package dirty and notify asset registry
	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(NewPipelineBlueprint);

	// Compile the blueprint
	FKismetEditorUtilities::CompileBlueprint(NewPipelineBlueprint);

	// Prepare result
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("name"), PipelineName);
	ResultObj->SetStringField(TEXT("path"), FullAssetPath); // legacy
	ResultObj->SetStringField(TEXT("object_path"), ObjectPath); // legacy
	FUnrealMCPCommonUtils::AddResolvedAssetFields(ResultObj, FullAssetPath);


	ResultObj->SetStringField(TEXT("parent_class"), ParentPipelineClass->GetName());
	ResultObj->SetStringField(TEXT("type"), TEXT("InterchangePipelineBlueprint"));
	ResultObj->SetStringField(TEXT("message"), TEXT("Pipeline Blueprint created. Open in editor to configure import settings."));

	UE_LOG(LogTemp, Log, TEXT("Created Interchange Pipeline Blueprint: %s (Parent: %s)"), *PipelineName, *ParentPipelineClass->GetName());

	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPInterchangeCommands::HandleGetInterchangePipelines(const TSharedPtr<FJsonObject>& Params)
{
	// Get optional search path
	FString SearchPath = TEXT("/Game/");
	Params->TryGetStringField(TEXT("search_path"), SearchPath);

	// Query asset registry for Pipeline Blueprints
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(*SearchPath);
	Filter.ClassPaths.Add(UInterchangeBlueprintPipelineBase::StaticClass()->GetClassPathName());

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	// Also search for native pipeline classes
	TArray<TSharedPtr<FJsonValue>> PipelinesArray;

	// Add found blueprint pipelines
	for (const FAssetData& AssetData : AssetDataList)
	{
		TSharedPtr<FJsonObject> PipelineObj = MakeShared<FJsonObject>();
		PipelineObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		PipelineObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString()); // legacy (object path)
		PipelineObj->SetStringField(TEXT("resolved_asset_path"), AssetData.PackageName.ToString());
		PipelineObj->SetStringField(TEXT("object_path"), AssetData.GetObjectPathString());
		PipelineObj->SetStringField(TEXT("type"), TEXT("Blueprint"));
		PipelineObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());

		PipelinesArray.Add(MakeShared<FJsonValueObject>(PipelineObj));
	}

	// Add available native pipeline classes
	TArray<TSharedPtr<FJsonValue>> NativePipelinesArray;
	
	auto AddNativePipeline = [&NativePipelinesArray](const FString& Name, const FString& Description)
	{
		TSharedPtr<FJsonObject> PipelineObj = MakeShared<FJsonObject>();
		PipelineObj->SetStringField(TEXT("name"), Name);
		PipelineObj->SetStringField(TEXT("description"), Description);
		PipelineObj->SetStringField(TEXT("type"), TEXT("Native"));
		NativePipelinesArray.Add(MakeShared<FJsonValueObject>(PipelineObj));
	};

	AddNativePipeline(TEXT("GenericAssetsPipeline"), TEXT("Base pipeline for general asset import"));
	AddNativePipeline(TEXT("GenericMeshPipeline"), TEXT("Pipeline for mesh import (StaticMesh/SkeletalMesh)"));
	AddNativePipeline(TEXT("GenericMaterialPipeline"), TEXT("Pipeline for material import"));
	AddNativePipeline(TEXT("GenericTexturePipeline"), TEXT("Pipeline for texture import"));
	AddNativePipeline(TEXT("FBXMaterialPipeline"), TEXT("Custom pipeline for FBX material instance auto-setup (UnrealMCP)"));

	// Prepare result
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetArrayField(TEXT("blueprint_pipelines"), PipelinesArray);
	ResultObj->SetNumberField(TEXT("blueprint_count"), static_cast<double>(PipelinesArray.Num()));
	ResultObj->SetArrayField(TEXT("native_pipelines"), NativePipelinesArray);

	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPInterchangeCommands::HandleConfigureInterchangePipeline(const TSharedPtr<FJsonObject>& Params)
{
	// Get required parameters
	FString PipelinePath;
	if (!Params->TryGetStringField(TEXT("pipeline_path"), PipelinePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pipeline_path' parameter"));
	}

	// Load the pipeline blueprint
	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(PipelinePath);
	if (!LoadedAsset)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Pipeline not found: %s"), *PipelinePath));
	}

	UBlueprint* PipelineBlueprint = Cast<UBlueprint>(LoadedAsset);
	if (!PipelineBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Asset is not a Blueprint"));
	}

	// Get the CDO (Class Default Object) to configure
	UClass* GeneratedClass = PipelineBlueprint->GeneratedClass;
	if (!GeneratedClass)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint has no generated class"));
	}

	UInterchangePipelineBase* PipelineCDO = Cast<UInterchangePipelineBase>(GeneratedClass->GetDefaultObject());
	if (!PipelineCDO)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint is not an Interchange Pipeline"));
	}

	// Get configuration settings from params
	TArray<TSharedPtr<FJsonValue>> ConfiguredProperties;

	// Configure common pipeline properties if provided
	const TSharedPtr<FJsonObject>* SettingsObj = nullptr;
	if (Params->TryGetObjectField(TEXT("settings"), SettingsObj))
	{
		// Iterate through provided settings and try to set them
		for (const auto& Setting : (*SettingsObj)->Values)
		{
			FString PropertyName = Setting.Key;
			
			// Find the property on the pipeline
			FProperty* Property = GeneratedClass->FindPropertyByName(*PropertyName);
			if (Property)
			{
				TSharedPtr<FJsonObject> PropInfo = MakeShared<FJsonObject>();
				PropInfo->SetStringField(TEXT("name"), PropertyName);
				PropInfo->SetBoolField(TEXT("found"), true);
				
				// Try to set the property value based on type
				void* PropertyAddress = Property->ContainerPtrToValuePtr<void>(PipelineCDO);
				
				if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
				{
					bool Value = false;
					if (Setting.Value->TryGetBool(Value))
					{
						BoolProp->SetPropertyValue(PropertyAddress, Value);
						PropInfo->SetBoolField(TEXT("set"), true);
					}
				}
				else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
				{
					double Value = 0.0;
					if (Setting.Value->TryGetNumber(Value))
					{
						FloatProp->SetPropertyValue(PropertyAddress, static_cast<float>(Value));
						PropInfo->SetBoolField(TEXT("set"), true);
					}
				}
				else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
				{
					double Value = 0.0;
					if (Setting.Value->TryGetNumber(Value))
					{
						IntProp->SetPropertyValue(PropertyAddress, static_cast<int32>(Value));
						PropInfo->SetBoolField(TEXT("set"), true);
					}
				}
				else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
				{
					FString Value;
					if (Setting.Value->TryGetString(Value))
					{
						StrProp->SetPropertyValue(PropertyAddress, Value);
						PropInfo->SetBoolField(TEXT("set"), true);
					}
				}
				
				ConfiguredProperties.Add(MakeShared<FJsonValueObject>(PropInfo));
			}
		}
	}

	// Transaction + Modify for stable Undo/Redo
	const FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Configure Interchange Pipeline")));
	PipelineBlueprint->Modify();
	
	// Mark the blueprint as modified
	PipelineBlueprint->MarkPackageDirty();

	// Prepare result
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("pipeline_path"), PipelinePath);
	ResultObj->SetStringField(TEXT("resolved_asset_path"), PipelineBlueprint->GetPathName());
	ResultObj->SetStringField(TEXT("pipeline_class"), GeneratedClass->GetName());
	ResultObj->SetArrayField(TEXT("configured_properties"), ConfiguredProperties);
	ResultObj->SetStringField(TEXT("message"), TEXT("Pipeline configured. Save the asset to persist changes."));

	UE_LOG(LogTemp, Log, TEXT("Configured Interchange Pipeline: %s"), *PipelinePath);

	return ResultObj;
}

// ============================================================================
// Helper Functions for Pipeline Graph Operations
// ============================================================================

UBlueprint* FUnrealMCPInterchangeCommands::LoadPipelineBlueprint(const FString& PipelinePath) const
{
	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(PipelinePath);
	if (!LoadedAsset)
	{
		return nullptr;
	}
	return Cast<UBlueprint>(LoadedAsset);
}

UEdGraph* FUnrealMCPInterchangeCommands::FindOrCreateFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName) const
{
	if (!Blueprint)
	{
		return nullptr;
	}

	// First, try to find an existing function graph
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetFName().ToString() == FunctionName)
		{
			return Graph;
		}
	}

	// Check UbergraphPages (event graphs)
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && Graph->GetFName().ToString() == FunctionName)
		{
			return Graph;
		}
	}

	return nullptr;
}

// ============================================================================
// Interchange Pipeline Graph Node Operations
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPInterchangeCommands::HandleGetInterchangePipelineGraph(const TSharedPtr<FJsonObject>& Params)
{
	FString PipelinePath;
	if (!Params->TryGetStringField(TEXT("pipeline_path"), PipelinePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pipeline_path' parameter"));
	}

	UBlueprint* PipelineBlueprint = LoadPipelineBlueprint(PipelinePath);
	if (!PipelineBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Pipeline not found: %s"), *PipelinePath));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("pipeline_path"), PipelinePath);
	ResultObj->SetStringField(TEXT("resolved_asset_path"), PipelineBlueprint->GetPathName());
	ResultObj->SetStringField(TEXT("blueprint_name"), PipelineBlueprint->GetName());

	// Get parent class info
	if (PipelineBlueprint->ParentClass)
	{
		ResultObj->SetStringField(TEXT("parent_class"), PipelineBlueprint->ParentClass->GetName());
	}

	// List function graphs
	TArray<TSharedPtr<FJsonValue>> FunctionGraphsArray;
	for (UEdGraph* Graph : PipelineBlueprint->FunctionGraphs)
	{
		if (Graph)
		{
			TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
			GraphObj->SetStringField(TEXT("name"), Graph->GetName());
			GraphObj->SetNumberField(TEXT("node_count"), static_cast<double>(Graph->Nodes.Num()));
			FunctionGraphsArray.Add(MakeShared<FJsonValueObject>(GraphObj));
		}
	}
	ResultObj->SetArrayField(TEXT("function_graphs"), FunctionGraphsArray);

	// List event graphs (UbergraphPages)
	TArray<TSharedPtr<FJsonValue>> EventGraphsArray;
	for (UEdGraph* Graph : PipelineBlueprint->UbergraphPages)
	{
		if (Graph)
		{
			TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
			GraphObj->SetStringField(TEXT("name"), Graph->GetName());
			GraphObj->SetNumberField(TEXT("node_count"), static_cast<double>(Graph->Nodes.Num()));
			
			// List nodes in this graph
			TArray<TSharedPtr<FJsonValue>> NodesArray;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node)
				{
					TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
					NodeObj->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
					NodeObj->SetStringField(TEXT("node_class"), Node->GetClass()->GetName());
					NodeObj->SetStringField(TEXT("node_title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
					NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
				}
			}
			GraphObj->SetArrayField(TEXT("nodes"), NodesArray);
			
			EventGraphsArray.Add(MakeShared<FJsonValueObject>(GraphObj));
		}
	}
	ResultObj->SetArrayField(TEXT("event_graphs"), EventGraphsArray);

	// List overridable functions from parent class
	TArray<TSharedPtr<FJsonValue>> OverridableFunctionsArray;
	if (PipelineBlueprint->ParentClass)
	{
		for (TFieldIterator<UFunction> FuncIt(PipelineBlueprint->ParentClass, EFieldIteratorFlags::IncludeSuper); FuncIt; ++FuncIt)
		{
			UFunction* Function = *FuncIt;
			if (Function && Function->HasAnyFunctionFlags(FUNC_BlueprintEvent))
			{
				TSharedPtr<FJsonObject> FuncObj = MakeShared<FJsonObject>();
				FuncObj->SetStringField(TEXT("name"), Function->GetName());
				FuncObj->SetBoolField(TEXT("is_native"), Function->HasAnyFunctionFlags(FUNC_Native));
				OverridableFunctionsArray.Add(MakeShared<FJsonValueObject>(FuncObj));
			}
		}
	}
	ResultObj->SetArrayField(TEXT("overridable_functions"), OverridableFunctionsArray);

	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPInterchangeCommands::HandleAddInterchangePipelineFunctionOverride(const TSharedPtr<FJsonObject>& Params)
{
	FString PipelinePath;
	if (!Params->TryGetStringField(TEXT("pipeline_path"), PipelinePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pipeline_path' parameter"));
	}

	FString FunctionName;
	if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
	}

	UBlueprint* PipelineBlueprint = LoadPipelineBlueprint(PipelinePath);
	if (!PipelineBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Pipeline not found: %s"), *PipelinePath));
	}

	// Get node position
	FVector2D NodePosition(0.0f, 0.0f);
	if (Params->HasField(TEXT("node_position")))
	{
		NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
	}

	// Find the function in parent class
	UFunction* FunctionToOverride = nullptr;
	if (PipelineBlueprint->ParentClass)
	{
		FunctionToOverride = PipelineBlueprint->ParentClass->FindFunctionByName(*FunctionName);
	}

	if (!FunctionToOverride)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Function not found in parent class: %s"), *FunctionName));
	}

	// Check if already overridden
	UEdGraph* ExistingGraph = FindOrCreateFunctionGraph(PipelineBlueprint, FunctionName);
	if (ExistingGraph)
	{
		// Function already overridden, return existing graph info
		TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
		ResultObj->SetBoolField(TEXT("success"), true);
		ResultObj->SetStringField(TEXT("function_name"), FunctionName);
		ResultObj->SetStringField(TEXT("graph_name"), ExistingGraph->GetName());
		ResultObj->SetBoolField(TEXT("already_exists"), true);
		ResultObj->SetStringField(TEXT("message"), TEXT("Function override already exists"));
		
		// Find entry node
		for (UEdGraphNode* Node : ExistingGraph->Nodes)
		{
			if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
			{
				ResultObj->SetStringField(TEXT("entry_node_id"), EntryNode->NodeGuid.ToString());
				break;
			}
		}
		
		return ResultObj;
	}

	// Create the function override graph using the proper UE API
	// IMPORTANT: Pass UClass* (ParentClass) instead of UFunction* to get proper override behavior
	// This triggers CreateFunctionGraphTerminators(Graph, UClass*) which:
	// 1. Creates Entry node with correct function reference
	// 2. Creates ParentCall node automatically
	// 3. Connects Entry -> ParentCall
	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		PipelineBlueprint,
		*FunctionName,
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);

	if (!NewGraph)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create function graph"));
	}

	// Add the graph to the blueprint - pass ParentClass (UClass*) NOT the function (UFunction*)
	// This is critical for proper function override creation with Entry + ParentCall nodes
	FBlueprintEditorUtils::AddFunctionGraph(PipelineBlueprint, NewGraph, false, PipelineBlueprint->ParentClass.Get());

	// Find the entry node that was created by AddFunctionGraph
	UK2Node_FunctionEntry* EntryNode = nullptr;
	for (UEdGraphNode* Node : NewGraph->Nodes)
	{
		EntryNode = Cast<UK2Node_FunctionEntry>(Node);
		if (EntryNode)
		{
			break;
		}
	}

	// Entry node should have been created automatically by AddFunctionGraph
	// If not found, something went wrong
	if (!EntryNode)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create function entry node. AddFunctionGraph may have failed."));
	}

	// Set node position
	EntryNode->NodePosX = static_cast<int32>(NodePosition.X);
	EntryNode->NodePosY = static_cast<int32>(NodePosition.Y);

	FString EntryNodeId = EntryNode->NodeGuid.ToString();

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(PipelineBlueprint);

	// Collect entry node pins for response
	TArray<TSharedPtr<FJsonValue>> EntryPinsArray;
	for (UEdGraphPin* Pin : EntryNode->Pins)
	{
		if (Pin)
		{
			TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
			PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
			PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
			PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
			EntryPinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("function_name"), FunctionName);
	ResultObj->SetStringField(TEXT("graph_name"), NewGraph->GetName());
	ResultObj->SetStringField(TEXT("entry_node_id"), EntryNodeId);
	ResultObj->SetArrayField(TEXT("entry_pins"), EntryPinsArray);
	ResultObj->SetBoolField(TEXT("already_exists"), false);
	ResultObj->SetStringField(TEXT("message"), TEXT("Function override created successfully"));

	UE_LOG(LogTemp, Log, TEXT("Created function override: %s in %s (Entry: %s)"), *FunctionName, *PipelinePath, *EntryNodeId);

	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPInterchangeCommands::HandleAddInterchangePipelineNode(const TSharedPtr<FJsonObject>& Params)
{
	FString PipelinePath;
	if (!Params->TryGetStringField(TEXT("pipeline_path"), PipelinePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pipeline_path' parameter"));
	}

	FString NodeType;
	if (!Params->TryGetStringField(TEXT("node_type"), NodeType))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_type' parameter"));
	}

	UBlueprint* PipelineBlueprint = LoadPipelineBlueprint(PipelinePath);
	if (!PipelineBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Pipeline not found: %s"), *PipelinePath));
	}

	// Get optional parameters
	FString FunctionName;
	Params->TryGetStringField(TEXT("function_name"), FunctionName);

	FString TargetClass;
	Params->TryGetStringField(TEXT("target_class"), TargetClass);

	FVector2D NodePosition(0.0f, 0.0f);
	if (Params->HasField(TEXT("node_position")))
	{
		NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
	}

	// Transaction + Modify for stable Undo/Redo
	const FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Add Interchange Pipeline Node")));
	PipelineBlueprint->Modify();

	// Get the event graph (or first available graph)
	UEdGraph* TargetGraph = nullptr;
	if (PipelineBlueprint->UbergraphPages.Num() > 0)
	{
		TargetGraph = PipelineBlueprint->UbergraphPages[0];
	}
	else if (PipelineBlueprint->FunctionGraphs.Num() > 0)
	{
		TargetGraph = PipelineBlueprint->FunctionGraphs[0];
	}

	if (!TargetGraph)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No graph found in pipeline blueprint"));
	}

	UEdGraphNode* NewNode = nullptr;

	if (NodeType == TEXT("FunctionCall"))
	{
		// Create a function call node
		UK2Node_CallFunction* CallFunctionNode = NewObject<UK2Node_CallFunction>(TargetGraph);
		
		// Find the function
		UFunction* TargetFunction = nullptr;
		if (!TargetClass.IsEmpty())
		{
			UClass* Class = FindObject<UClass>(nullptr, *TargetClass);
			if (!Class)
			{
				// Try with full path
				Class = LoadClass<UObject>(nullptr, *TargetClass);
			}
			if (Class)
			{
				TargetFunction = Class->FindFunctionByName(*FunctionName);
			}
		}

		if (TargetFunction)
		{
			CallFunctionNode->SetFromFunction(TargetFunction);
		}
		else
		{
			// Try to find function by name in common classes
			CallFunctionNode->FunctionReference.SetExternalMember(*FunctionName, nullptr);
		}

		CallFunctionNode->NodePosX = NodePosition.X;
		CallFunctionNode->NodePosY = NodePosition.Y;

		TargetGraph->AddNode(CallFunctionNode);
		CallFunctionNode->CreateNewGuid();
		CallFunctionNode->PostPlacedNewNode();
		CallFunctionNode->AllocateDefaultPins();

		NewNode = CallFunctionNode;
	}
	else if (NodeType == TEXT("ParentCall"))
	{
		// Create a call parent function node
		UK2Node_CallParentFunction* ParentCallNode = NewObject<UK2Node_CallParentFunction>(TargetGraph);
		
		if (!FunctionName.IsEmpty() && PipelineBlueprint->ParentClass)
		{
			UFunction* ParentFunction = PipelineBlueprint->ParentClass->FindFunctionByName(*FunctionName);
			if (ParentFunction)
			{
				ParentCallNode->SetFromFunction(ParentFunction);
			}
		}

		ParentCallNode->NodePosX = NodePosition.X;
		ParentCallNode->NodePosY = NodePosition.Y;

		TargetGraph->AddNode(ParentCallNode);
		ParentCallNode->CreateNewGuid();
		ParentCallNode->PostPlacedNewNode();
		ParentCallNode->AllocateDefaultPins();

		NewNode = ParentCallNode;
	}
	else if (NodeType == TEXT("Variable"))
	{
		// Create a variable get node
		UK2Node_VariableGet* VarGetNode = NewObject<UK2Node_VariableGet>(TargetGraph);
		
		if (!FunctionName.IsEmpty())
		{
			VarGetNode->VariableReference.SetSelfMember(FName(*FunctionName));
		}

		VarGetNode->NodePosX = NodePosition.X;
		VarGetNode->NodePosY = NodePosition.Y;

		TargetGraph->AddNode(VarGetNode);
		VarGetNode->CreateNewGuid();
		VarGetNode->PostPlacedNewNode();
		VarGetNode->AllocateDefaultPins();
		VarGetNode->ReconstructNode();

		NewNode = VarGetNode;
	}
	else
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown node type: %s"), *NodeType));
	}

	if (!NewNode)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create node"));
	}

	// Node insertion is structural.
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(PipelineBlueprint);
	PipelineBlueprint->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("node_id"), NewNode->NodeGuid.ToString());
	ResultObj->SetStringField(TEXT("node_type"), NodeType);
	ResultObj->SetStringField(TEXT("node_class"), NewNode->GetClass()->GetName());
	ResultObj->SetStringField(TEXT("resolved_asset_path"), PipelineBlueprint->GetPathName());

	// List output pins
	TArray<TSharedPtr<FJsonValue>> PinsArray;
	for (UEdGraphPin* Pin : NewNode->Pins)
	{
		if (Pin)
		{
			TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
			PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
			PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
			PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
			PinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
		}
	}
	ResultObj->SetArrayField(TEXT("pins"), PinsArray);

	UE_LOG(LogTemp, Log, TEXT("Added node of type %s to pipeline %s"), *NodeType, *PipelinePath);

	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPInterchangeCommands::HandleConnectInterchangePipelineNodes(const TSharedPtr<FJsonObject>& Params)
{
	FString PipelinePath;
	if (!Params->TryGetStringField(TEXT("pipeline_path"), PipelinePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pipeline_path' parameter"));
	}

	FString SourceNodeId;
	if (!Params->TryGetStringField(TEXT("source_node_id"), SourceNodeId))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_node_id' parameter"));
	}

	FString TargetNodeId;
	if (!Params->TryGetStringField(TEXT("target_node_id"), TargetNodeId))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'target_node_id' parameter"));
	}

	FString SourcePinName;
	if (!Params->TryGetStringField(TEXT("source_pin"), SourcePinName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_pin' parameter"));
	}

	FString TargetPinName;
	if (!Params->TryGetStringField(TEXT("target_pin"), TargetPinName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'target_pin' parameter"));
	}

	UBlueprint* PipelineBlueprint = LoadPipelineBlueprint(PipelinePath);
	if (!PipelineBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Pipeline not found: %s"), *PipelinePath));
	}

	// Find nodes in all graphs
	UEdGraphNode* SourceNode = nullptr;
	UEdGraphNode* TargetNode = nullptr;
	UEdGraph* FoundGraph = nullptr;

	auto FindNodesInGraph = [&](UEdGraph* Graph)
	{
		if (!Graph) return;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node->NodeGuid.ToString() == SourceNodeId)
			{
				SourceNode = Node;
				FoundGraph = Graph;
			}
			if (Node->NodeGuid.ToString() == TargetNodeId)
			{
				TargetNode = Node;
			}
		}
	};

	// Search in all graphs
	for (UEdGraph* Graph : PipelineBlueprint->UbergraphPages)
	{
		FindNodesInGraph(Graph);
	}
	for (UEdGraph* Graph : PipelineBlueprint->FunctionGraphs)
	{
		FindNodesInGraph(Graph);
	}

	if (!SourceNode)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Source node not found: %s"), *SourceNodeId));
	}
	if (!TargetNode)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Target node not found: %s"), *TargetNodeId));
	}

	// Find pins
	UEdGraphPin* SourcePin = nullptr;
	UEdGraphPin* TargetPin = nullptr;

	for (UEdGraphPin* Pin : SourceNode->Pins)
	{
		if (Pin->PinName.ToString() == SourcePinName && Pin->Direction == EGPD_Output)
		{
			SourcePin = Pin;
			break;
		}
	}

	for (UEdGraphPin* Pin : TargetNode->Pins)
	{
		if (Pin->PinName.ToString() == TargetPinName && Pin->Direction == EGPD_Input)
		{
			TargetPin = Pin;
			break;
		}
	}

	if (!SourcePin)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Source pin not found: %s"), *SourcePinName));
	}
	if (!TargetPin)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Target pin not found: %s"), *TargetPinName));
	}

	// Transaction + Modify for stable Undo/Redo
	const FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Connect Interchange Pipeline Nodes")));
	PipelineBlueprint->Modify();
	if (FoundGraph)
	{
		FoundGraph->Modify();
	}
	if (SourceNode)
	{
		SourceNode->Modify();
	}
	if (TargetNode)
	{
		TargetNode->Modify();
	}

	// Make the connection
	if (!FoundGraph)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Could not determine graph for connection"));
	}

	const UEdGraphSchema* Schema = FoundGraph->GetSchema();
	if (!Schema)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Could not get graph schema"));
	}

	// Check if connection is valid first
	FPinConnectionResponse Response = Schema->CanCreateConnection(SourcePin, TargetPin);
	if (Response.Response == CONNECT_RESPONSE_DISALLOW)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Cannot connect pins: %s"), *Response.Message.ToString()));
	}

	// Try to make the connection
	bool bConnected = Schema->TryCreateConnection(SourcePin, TargetPin);
	if (bConnected)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(PipelineBlueprint);
		PipelineBlueprint->MarkPackageDirty();

		TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
		ResultObj->SetBoolField(TEXT("success"), true);
		ResultObj->SetStringField(TEXT("source_node_id"), SourceNodeId);
		ResultObj->SetStringField(TEXT("target_node_id"), TargetNodeId);
		ResultObj->SetStringField(TEXT("source_pin"), SourcePinName);
		ResultObj->SetStringField(TEXT("target_pin"), TargetPinName);
		ResultObj->SetStringField(TEXT("message"), TEXT("Nodes connected successfully"));
		ResultObj->SetStringField(TEXT("resolved_asset_path"), PipelineBlueprint->GetPathName());

		UE_LOG(LogTemp, Log, TEXT("Connected nodes in pipeline %s"), *PipelinePath);

		return ResultObj;
	}

	return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to connect nodes"));
}

TSharedPtr<FJsonObject> FUnrealMCPInterchangeCommands::HandleFindInterchangePipelineNodes(const TSharedPtr<FJsonObject>& Params)
{
	FString PipelinePath;
	if (!Params->TryGetStringField(TEXT("pipeline_path"), PipelinePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pipeline_path' parameter"));
	}

	UBlueprint* PipelineBlueprint = LoadPipelineBlueprint(PipelinePath);
	if (!PipelineBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Pipeline not found: %s"), *PipelinePath));
	}

	FString NodeTypeFilter;
	Params->TryGetStringField(TEXT("node_type"), NodeTypeFilter);

	FString FunctionNameFilter;
	Params->TryGetStringField(TEXT("function_name"), FunctionNameFilter);

	TArray<TSharedPtr<FJsonValue>> NodesArray;

	auto ProcessGraph = [&](UEdGraph* Graph, const FString& GraphType)
	{
		if (!Graph) return;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			FString NodeClassName = Node->GetClass()->GetName();
			FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
			
			// Sanitize node title to avoid JSON issues (remove newlines, escape quotes)
			NodeTitle.ReplaceInline(TEXT("\r\n"), TEXT(" "));
			NodeTitle.ReplaceInline(TEXT("\n"), TEXT(" "));
			NodeTitle.ReplaceInline(TEXT("\r"), TEXT(" "));
			NodeTitle.ReplaceInline(TEXT("\""), TEXT("'"));

			// Apply filters
			if (!NodeTypeFilter.IsEmpty())
			{
				if (!NodeClassName.Contains(NodeTypeFilter))
				{
					continue;
				}
			}

			if (!FunctionNameFilter.IsEmpty())
			{
				if (!NodeTitle.Contains(FunctionNameFilter))
				{
					continue;
				}
			}

			TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
			NodeObj->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
			NodeObj->SetStringField(TEXT("node_class"), NodeClassName);
			NodeObj->SetStringField(TEXT("node_title"), NodeTitle);
			NodeObj->SetStringField(TEXT("graph_name"), Graph->GetName());
			NodeObj->SetStringField(TEXT("graph_type"), GraphType);
			NodeObj->SetNumberField(TEXT("pos_x"), static_cast<double>(Node->NodePosX));
			NodeObj->SetNumberField(TEXT("pos_y"), static_cast<double>(Node->NodePosY));

			// List pins
			TArray<TSharedPtr<FJsonValue>> PinsArray;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin)
				{
					TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
					PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
					PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
					PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
					PinObj->SetBoolField(TEXT("connected"), Pin->LinkedTo.Num() > 0);
					PinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
				}
			}
			NodeObj->SetArrayField(TEXT("pins"), PinsArray);

			NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
		}
	};

	// Process all graphs
	for (UEdGraph* Graph : PipelineBlueprint->UbergraphPages)
	{
		ProcessGraph(Graph, TEXT("EventGraph"));
	}
	for (UEdGraph* Graph : PipelineBlueprint->FunctionGraphs)
	{
		ProcessGraph(Graph, TEXT("FunctionGraph"));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetArrayField(TEXT("nodes"), NodesArray);
	ResultObj->SetNumberField(TEXT("count"), static_cast<double>(NodesArray.Num()));

	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPInterchangeCommands::HandleAddInterchangeIterateNodesBlock(const TSharedPtr<FJsonObject>& Params)
{
	FString PipelinePath;
	if (!Params->TryGetStringField(TEXT("pipeline_path"), PipelinePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pipeline_path' parameter"));
	}

	FString NodeClass;
	if (!Params->TryGetStringField(TEXT("node_class"), NodeClass))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_class' parameter"));
	}

	FString GraphName = TEXT("ExecutePipeline");
	Params->TryGetStringField(TEXT("graph_name"), GraphName);

	UBlueprint* PipelineBlueprint = LoadPipelineBlueprint(PipelinePath);
	if (!PipelineBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Pipeline not found: %s"), *PipelinePath));
	}

	FVector2D NodePosition(0.0f, 0.0f);
	if (Params->HasField(TEXT("node_position")))
	{
		NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
	}

	// Find the target graph
	UEdGraph* TargetGraph = FindOrCreateFunctionGraph(PipelineBlueprint, GraphName);
	if (!TargetGraph)
	{
		// Try event graphs
		for (UEdGraph* Graph : PipelineBlueprint->UbergraphPages)
		{
			if (Graph && Graph->GetName().Contains(GraphName))
			{
				TargetGraph = Graph;
				break;
			}
		}
	}

	if (!TargetGraph)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Graph not found: %s. Create function override first."), *GraphName));
	}

	// Transaction + Modify for stable Undo/Redo
	const FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Add Interchange IterateNodes Block")));
	PipelineBlueprint->Modify();
	TargetGraph->Modify();

	// Create the IterateNodesOfType function call
	// Note: This is a templated function, so we need to set it up properly
	UK2Node_CallFunction* IterateNode = NewObject<UK2Node_CallFunction>(TargetGraph);
	
	// Find UInterchangeBaseNodeContainer::IterateNodesOfType
	UClass* NodeContainerClass = FindObject<UClass>(nullptr, TEXT("/Script/InterchangeCore.InterchangeBaseNodeContainer"));
	if (!NodeContainerClass)
	{
		NodeContainerClass = LoadClass<UObject>(nullptr, TEXT("/Script/InterchangeCore.InterchangeBaseNodeContainer"));
	}

	if (NodeContainerClass)
	{
		// Look for IterateNodes function (non-templated version)
		UFunction* IterateFunc = NodeContainerClass->FindFunctionByName(TEXT("IterateNodes"));
		if (IterateFunc)
		{
			IterateNode->SetFromFunction(IterateFunc);
		}
	}

	IterateNode->NodePosX = NodePosition.X;
	IterateNode->NodePosY = NodePosition.Y;

	TargetGraph->AddNode(IterateNode);
	IterateNode->CreateNewGuid();
	IterateNode->PostPlacedNewNode();
	IterateNode->AllocateDefaultPins();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(PipelineBlueprint);
	PipelineBlueprint->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("iterate_node_id"), IterateNode->NodeGuid.ToString());
	ResultObj->SetStringField(TEXT("node_class"), NodeClass);
	ResultObj->SetStringField(TEXT("graph_name"), TargetGraph->GetName());
	ResultObj->SetStringField(TEXT("message"), TEXT("Iterate nodes block created. Connect to node container and add processing logic."));
	ResultObj->SetStringField(TEXT("resolved_asset_path"), PipelineBlueprint->GetPathName());

	// List pins
	TArray<TSharedPtr<FJsonValue>> PinsArray;
	for (UEdGraphPin* Pin : IterateNode->Pins)
	{
		if (Pin)
		{
			TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
			PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
			PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
			PinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
		}
	}
	ResultObj->SetArrayField(TEXT("pins"), PinsArray);

	UE_LOG(LogTemp, Log, TEXT("Added IterateNodes block for %s in pipeline %s"), *NodeClass, *PipelinePath);

	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPInterchangeCommands::HandleCompileInterchangePipeline(const TSharedPtr<FJsonObject>& Params)
{
	FString PipelinePath;
	if (!Params->TryGetStringField(TEXT("pipeline_path"), PipelinePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pipeline_path' parameter"));
	}

	UBlueprint* PipelineBlueprint = LoadPipelineBlueprint(PipelinePath);
	if (!PipelineBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Pipeline not found: %s"), *PipelinePath));
	}

	// Mark as modified - actual compilation will happen when user saves or uses the blueprint
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(PipelineBlueprint);
	PipelineBlueprint->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("pipeline_path"), PipelinePath);
	ResultObj->SetStringField(TEXT("resolved_asset_path"), PipelineBlueprint->GetPathName());
	ResultObj->SetStringField(TEXT("status"), TEXT("Modified"));
	ResultObj->SetStringField(TEXT("message"), TEXT("Pipeline marked as modified. Compile in Blueprint Editor for full validation."));

	UE_LOG(LogTemp, Log, TEXT("Marked pipeline %s as modified"), *PipelinePath);

	return ResultObj;
}
