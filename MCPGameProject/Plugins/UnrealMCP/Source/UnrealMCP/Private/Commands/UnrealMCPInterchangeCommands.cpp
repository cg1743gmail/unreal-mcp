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

	// Destination path for the blueprint
	FString PackagePath = TEXT("/Game/Blueprints/");
	FString FullPath = PackagePath + BlueprintName;

	// Check if blueprint already exists
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
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

	UPackage* Package = CreatePackage(*FullPath);
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
	ResultObj->SetStringField(TEXT("path"), FullPath);
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

	FString PackagePath = TEXT("/Game/Blueprints/");
	Params->TryGetStringField(TEXT("package_path"), PackagePath);

	// Ensure package path starts with /Game/
	if (!PackagePath.StartsWith(TEXT("/Game/")))
	{
		PackagePath = TEXT("/Game/") + PackagePath;
	}
	
	// Ensure package path ends with /
	if (!PackagePath.EndsWith(TEXT("/")))
	{
		PackagePath += TEXT("/");
	}

	FString FullPath = PackagePath + BlueprintName;

	// Check if blueprint already exists
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
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

	UPackage* Package = CreatePackage(*FullPath);
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
	ResultObj->SetStringField(TEXT("path"), FullPath);
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
		AssetObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
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

	// Load the asset
	UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!Asset)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
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
	ResultObj->SetStringField(TEXT("asset_path"), AssetPath);
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
