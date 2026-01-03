#include "Commands/UnrealMCPCommonUtils.h"
#include "GameFramework/Actor.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "UObject/UObjectIterator.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "Engine/Selection.h"

#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabase.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "WidgetBlueprint.h"
#include "Algo/Unique.h"
#include "Modules/ModuleManager.h"



// JSON Utilities
TSharedPtr<FJsonObject> FUnrealMCPCommonUtils::CreateErrorResponse(const FString& Message)
{
    return CreateErrorResponseEx(Message);
}

TSharedPtr<FJsonObject> FUnrealMCPCommonUtils::CreateErrorResponseEx(const FString& Message, const FString& Code, const FString& Details)
{
    TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
    ResponseObject->SetBoolField(TEXT("success"), false);

    // Backward-compatible field (many clients read this string)
    ResponseObject->SetStringField(TEXT("error"), Message);

    // Structured error fields
    ResponseObject->SetStringField(TEXT("error_code"), Code);
    if (!Details.IsEmpty())
    {
        ResponseObject->SetStringField(TEXT("error_details"), Details);
    }

    TSharedPtr<FJsonObject> ErrorInfo = MakeShared<FJsonObject>();
    ErrorInfo->SetStringField(TEXT("message"), Message);
    ErrorInfo->SetStringField(TEXT("code"), Code);
    if (!Details.IsEmpty())
    {
        ErrorInfo->SetStringField(TEXT("details"), Details);
    }
    ResponseObject->SetObjectField(TEXT("error_info"), ErrorInfo);

    return ResponseObject;
}

TSharedPtr<FJsonObject> FUnrealMCPCommonUtils::CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data)
{
    TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
    ResponseObject->SetBoolField(TEXT("success"), true);
    
    if (Data.IsValid())
    {
        ResponseObject->SetObjectField(TEXT("data"), Data);
    }
    
    return ResponseObject;
}

void FUnrealMCPCommonUtils::GetIntArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<int32>& OutArray)
{
    OutArray.Reset();
    
    if (!JsonObject->HasField(FieldName))
    {
        return;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray))
    {
        for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
        {
            OutArray.Add((int32)Value->AsNumber());
        }
    }
}

void FUnrealMCPCommonUtils::GetFloatArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<float>& OutArray)
{
    OutArray.Reset();
    
    if (!JsonObject->HasField(FieldName))
    {
        return;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray))
    {
        for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
        {
            OutArray.Add((float)Value->AsNumber());
        }
    }
}

FVector2D FUnrealMCPCommonUtils::GetVector2DFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FVector2D Result(0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 2)
    {
        Result.X = (float)(*JsonArray)[0]->AsNumber();
        Result.Y = (float)(*JsonArray)[1]->AsNumber();
    }
    
    return Result;
}

FVector FUnrealMCPCommonUtils::GetVectorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FVector Result(0.0f, 0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 3)
    {
        Result.X = (float)(*JsonArray)[0]->AsNumber();
        Result.Y = (float)(*JsonArray)[1]->AsNumber();
        Result.Z = (float)(*JsonArray)[2]->AsNumber();
    }
    
    return Result;
}

FRotator FUnrealMCPCommonUtils::GetRotatorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FRotator Result(0.0f, 0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 3)
    {
        Result.Pitch = (float)(*JsonArray)[0]->AsNumber();
        Result.Yaw = (float)(*JsonArray)[1]->AsNumber();
        Result.Roll = (float)(*JsonArray)[2]->AsNumber();
    }
    
    return Result;
}

// -------------------------
// Asset path utilities
// -------------------------

namespace UnrealMcpAssetPath
{
	static const TCHAR* Section = TEXT("UnrealMCP");

	static FString GetConfigString(const TCHAR* Key, const FString& DefaultValue)
	{
		FString Value;
		if (GConfig && GConfig->GetString(Section, Key, Value, GEngineIni) && !Value.IsEmpty())
		{
			return Value;
		}
		return DefaultValue;
	}

	static bool GetConfigBool(const TCHAR* Key, bool DefaultValue)
	{
		bool bValue = DefaultValue;
		if (GConfig)
		{
			GConfig->GetBool(Section, Key, bValue, GEngineIni);
		}
		return bValue;
	}

	static void NormalizeRoot(FString& Root)
	{
		Root.TrimStartAndEndInline();
		if (!Root.EndsWith(TEXT("/")))
		{
			Root += TEXT("/");
		}
	}

	static void SplitCsv(const FString& Csv, TArray<FString>& Out)
	{
		TArray<FString> Parts;
		Csv.ParseIntoArray(Parts, TEXT(","), true);
		for (FString& P : Parts)
		{
			P.TrimStartAndEndInline();
			if (!P.IsEmpty())
			{
				Out.Add(P);
			}
		}
	}
}

FString FUnrealMCPCommonUtils::GetDefaultBlueprintFolder()
{
	// Safe default: keep UnrealMCP-generated assets under a dedicated root.
	FString Folder = UnrealMcpAssetPath::GetConfigString(TEXT("DefaultBlueprintFolder"), TEXT("/Game/UnrealMCP/Blueprints/"));
	FString Err;
	FString Normalized;
	if (NormalizeLongPackageFolder(Folder, Normalized, Err))
	{
		return Normalized;
	}
	return TEXT("/Game/UnrealMCP/Blueprints/");
}

FString FUnrealMCPCommonUtils::GetDefaultWidgetFolder()
{
	FString Folder = UnrealMcpAssetPath::GetConfigString(TEXT("DefaultWidgetFolder"), TEXT("/Game/UnrealMCP/Widgets/"));
	FString Err;
	FString Normalized;
	if (NormalizeLongPackageFolder(Folder, Normalized, Err))
	{
		return Normalized;
	}
	return TEXT("/Game/UnrealMCP/Widgets/");
}

void FUnrealMCPCommonUtils::GetAllowedWriteRoots(TArray<FString>& OutRoots)
{
	OutRoots.Reset();

	FString Csv = UnrealMcpAssetPath::GetConfigString(TEXT("AllowedWriteRoots"), TEXT("/Game/UnrealMCP/"));
	UnrealMcpAssetPath::SplitCsv(Csv, OutRoots);
	if (OutRoots.Num() == 0)
	{
		OutRoots.Add(TEXT("/Game/UnrealMCP/"));
	}

	for (FString& Root : OutRoots)
	{
		UnrealMcpAssetPath::NormalizeRoot(Root);
	}
}

bool FUnrealMCPCommonUtils::IsWritePathAllowed(const FString& LongPackageOrAssetPath, FString& OutError)
{
	OutError.Reset();

	const bool bStrict = UnrealMcpAssetPath::GetConfigBool(TEXT("bStrictWriteAllowlist"), true);
	if (!bStrict)
	{
		return true;
	}

	FString NormalizedAssetPath;
	FString Err;
	if (!NormalizeLongPackageAssetPath(LongPackageOrAssetPath, NormalizedAssetPath, Err))
	{
		OutError = Err;
		return false;
	}

	// Disallow writes to engine content.
	if (NormalizedAssetPath.StartsWith(TEXT("/Engine/")))
	{
		OutError = TEXT("Write operations to /Engine are not allowed");
		return false;
	}

	TArray<FString> Roots;
	GetAllowedWriteRoots(Roots);
	for (const FString& Root : Roots)
	{
		if (NormalizedAssetPath.StartsWith(Root))
		{
			return true;
		}
	}

	OutError = FString::Printf(TEXT("Write path '%s' is not allowed. Configure [UnrealMCP] AllowedWriteRoots to include the desired /Game/... root."), *NormalizedAssetPath);
	return false;
}

bool FUnrealMCPCommonUtils::NormalizeLongPackageFolder(const FString& InFolder, FString& OutFolder, FString& OutError)
{
	OutError.Reset();
	OutFolder = InFolder;
	OutFolder.TrimStartAndEndInline();

	if (OutFolder.IsEmpty())
	{
		OutError = TEXT("Folder path is empty");
		return false;
	}

	if (!OutFolder.StartsWith(TEXT("/Game/")))
	{
		OutError = FString::Printf(TEXT("Folder must start with /Game/. Got: %s"), *OutFolder);
		return false;
	}

	if (!OutFolder.EndsWith(TEXT("/")))
	{
		OutFolder += TEXT("/");
	}

	// Folder paths should be valid long package names without an object suffix.
	FString AsPackage = OutFolder.LeftChop(1);
	if (!FPackageName::IsValidLongPackageName(AsPackage))
	{
		OutError = FString::Printf(TEXT("Invalid long package folder: %s"), *OutFolder);
		return false;
	}

	return true;
}

bool FUnrealMCPCommonUtils::NormalizeLongPackageAssetPath(const FString& InAssetPath, FString& OutAssetPath, FString& OutError)
{
	OutError.Reset();
	OutAssetPath = InAssetPath;
	OutAssetPath.TrimStartAndEndInline();

	if (OutAssetPath.IsEmpty())
	{
		OutError = TEXT("Asset path is empty");
		return false;
	}

	// Accept object path (contains '.') and normalize to long package asset path.
	if (OutAssetPath.Contains(TEXT(".")))
	{
		FSoftObjectPath SoftPath(OutAssetPath);
		const FString LongPackageName = SoftPath.GetLongPackageName();
		const FString AssetName = SoftPath.GetAssetName();
		if (LongPackageName.IsEmpty() || AssetName.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Invalid object path: %s"), *OutAssetPath);
			return false;
		}
		OutAssetPath = FString::Printf(TEXT("%s/%s"), *LongPackageName, *AssetName);
	}

	if (!OutAssetPath.StartsWith(TEXT("/Game/")) && !OutAssetPath.StartsWith(TEXT("/Engine/")))
	{
		OutError = FString::Printf(TEXT("Asset path must start with /Game/ (or /Engine/ for read-only). Got: %s"), *OutAssetPath);
		return false;
	}

	if (!FPackageName::IsValidLongPackageName(OutAssetPath))
	{
		OutError = FString::Printf(TEXT("Invalid long package asset path: %s"), *OutAssetPath);
		return false;
	}

	return true;
}

bool FUnrealMCPCommonUtils::MakeObjectPathFromAssetPath(const FString& LongPackageAssetPath, FString& OutObjectPath, FString& OutError)
{
	OutError.Reset();
	FString NormalizedAssetPath;
	if (!NormalizeLongPackageAssetPath(LongPackageAssetPath, NormalizedAssetPath, OutError))
	{
		return false;
	}

	const FString AssetName = FPackageName::GetShortName(NormalizedAssetPath);
	OutObjectPath = FString::Printf(TEXT("%s.%s"), *NormalizedAssetPath, *AssetName);
	return true;
}

void FUnrealMCPCommonUtils::AddResolvedAssetFields(const TSharedPtr<FJsonObject>& Obj, const FString& AnyAssetOrObjectPath)
{
	if (!Obj.IsValid())
	{
		return;
	}

	FString NormalizedAssetPath;
	FString Err;
	if (!NormalizeLongPackageAssetPath(AnyAssetOrObjectPath, NormalizedAssetPath, Err))
	{
		return;
	}

	Obj->SetStringField(TEXT("resolved_asset_path"), NormalizedAssetPath);

	FString ObjectPath;
	if (MakeObjectPathFromAssetPath(NormalizedAssetPath, ObjectPath, Err))
	{
		Obj->SetStringField(TEXT("object_path"), ObjectPath);
	}
}

void FUnrealMCPCommonUtils::AddResolvedAssetFieldsFromUObject(const TSharedPtr<FJsonObject>& Obj, const UObject* Asset)
{
	if (!Obj.IsValid() || !Asset)
	{
		return;
	}

	// Outermost package name is a long package name: /Game/.../AssetName
	const FString PackageName = Asset->GetOutermost() ? Asset->GetOutermost()->GetName() : FString();
	if (!PackageName.IsEmpty())
	{
		AddResolvedAssetFields(Obj, PackageName);
	}
}

UObject* FUnrealMCPCommonUtils::LoadAssetByPathSmart(const FString& InPath)
{
	FString NormalizedAssetPath;
	FString Err;
	if (!NormalizeLongPackageAssetPath(InPath, NormalizedAssetPath, Err))
	{
		return nullptr;
	}

	FString ObjectPath;
	if (!MakeObjectPathFromAssetPath(NormalizedAssetPath, ObjectPath, Err))
	{
		return nullptr;
	}

	return UEditorAssetLibrary::LoadAsset(ObjectPath);
}

UBlueprint* FUnrealMCPCommonUtils::ResolveBlueprintFromNameOrPath(const FString& BlueprintName, const FString& BlueprintPath, FString& OutResolvedAssetPath, TArray<FString>& OutCandidates)
{
	OutResolvedAssetPath.Reset();
	OutCandidates.Reset();

	// Path wins (recommended).
	if (!BlueprintPath.IsEmpty())
	{
		FString Normalized;
		FString Err;
		if (!NormalizeLongPackageAssetPath(BlueprintPath, Normalized, Err))
		{
			OutCandidates.Add(FString::Printf(TEXT("Invalid blueprint_path: %s"), *Err));
			return nullptr;
		}

		UObject* Asset = LoadAssetByPathSmart(Normalized);
		UBlueprint* BP = Cast<UBlueprint>(Asset);
		if (!BP)
		{
			OutCandidates.Add(TEXT("Path did not resolve to a UBlueprint"));
			return nullptr;
		}
		OutResolvedAssetPath = Normalized;
		return BP;
	}

	if (BlueprintName.IsEmpty())
	{
		return nullptr;
	}

	// Name-only fallback: search AssetRegistry for exact name matches.
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), Assets, true);

	for (const FAssetData& AssetData : Assets)
	{
		if (AssetData.AssetName.ToString().Equals(BlueprintName, ESearchCase::CaseSensitive))
		{
			const FString Candidate = AssetData.PackageName.ToString();
			// PackageName is like /Game/Foo/BP_Test (without object suffix)
			OutCandidates.Add(Candidate);
		}
	}

	// Remove duplicates + stabilize order.
	OutCandidates.Sort();
	OutCandidates.SetNum(Algo::Unique(OutCandidates));

	if (OutCandidates.Num() != 1)
	{
		return nullptr;
	}

	OutResolvedAssetPath = OutCandidates[0];
	const FString ObjPath = FString::Printf(TEXT("%s.%s"), *OutResolvedAssetPath, *FPackageName::GetShortName(OutResolvedAssetPath));
	return Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(ObjPath));
}


UWidgetBlueprint* FUnrealMCPCommonUtils::ResolveWidgetBlueprintFromNameOrPath(const FString& BlueprintName, const FString& BlueprintPath, FString& OutResolvedAssetPath, TArray<FString>& OutCandidates)
{
	OutResolvedAssetPath.Reset();
	OutCandidates.Reset();

	if (!BlueprintPath.IsEmpty())
	{
		FString Normalized;
		FString Err;
		if (!NormalizeLongPackageAssetPath(BlueprintPath, Normalized, Err))
		{
			OutCandidates.Add(FString::Printf(TEXT("Invalid blueprint_path: %s"), *Err));
			return nullptr;
		}

		UObject* Asset = LoadAssetByPathSmart(Normalized);
		UWidgetBlueprint* BP = Cast<UWidgetBlueprint>(Asset);
		if (!BP)
		{
			OutCandidates.Add(TEXT("Path did not resolve to a UWidgetBlueprint"));
			return nullptr;
		}
		OutResolvedAssetPath = Normalized;
		return BP;
	}

	if (BlueprintName.IsEmpty())
	{
		return nullptr;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssetsByClass(UWidgetBlueprint::StaticClass()->GetClassPathName(), Assets, true);

	for (const FAssetData& AssetData : Assets)
	{
		if (AssetData.AssetName.ToString().Equals(BlueprintName, ESearchCase::CaseSensitive))
		{
			OutCandidates.Add(AssetData.PackageName.ToString());
		}
	}

	OutCandidates.Sort();
	OutCandidates.SetNum(Algo::Unique(OutCandidates));

	if (OutCandidates.Num() != 1)
	{
		return nullptr;
	}

	OutResolvedAssetPath = OutCandidates[0];
	const FString ObjPath = FString::Printf(TEXT("%s.%s"), *OutResolvedAssetPath, *FPackageName::GetShortName(OutResolvedAssetPath));
	return Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(ObjPath));
}

// -------------------------
// Blueprint Utilities
// -------------------------

UBlueprint* FUnrealMCPCommonUtils::FindBlueprint(const FString& BlueprintName)
{
	FString Resolved;
	TArray<FString> Candidates;

	// If caller passed a path, treat it as blueprint_path.
	if (BlueprintName.StartsWith(TEXT("/")))
	{
		return ResolveBlueprintFromNameOrPath(TEXT(""), BlueprintName, Resolved, Candidates);
	}

	// Otherwise treat it as a short name.
	return ResolveBlueprintFromNameOrPath(BlueprintName, TEXT(""), Resolved, Candidates);
}


UBlueprint* FUnrealMCPCommonUtils::FindBlueprintByName(const FString& BlueprintName)
{
	return FindBlueprint(BlueprintName);
}


UEdGraph* FUnrealMCPCommonUtils::FindOrCreateEventGraph(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return nullptr;
    }
    
    // Try to find the event graph
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph->GetName().Contains(TEXT("EventGraph")))
        {
            return Graph;
        }
    }
    
    // Create a new event graph if none exists
    UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FName(TEXT("EventGraph")), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(Blueprint, NewGraph);
    return NewGraph;
}

// Blueprint node utilities
UK2Node_Event* FUnrealMCPCommonUtils::CreateEventNode(UEdGraph* Graph, const FString& EventName, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
    if (!Blueprint)
    {
        return nullptr;
    }
    
    // Check for existing event node with this exact name
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
        if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
        {
            UE_LOG(LogTemp, Display, TEXT("Using existing event node with name %s (ID: %s)"), 
                *EventName, *EventNode->NodeGuid.ToString());
            return EventNode;
        }
    }

    // No existing node found, create a new one
    UK2Node_Event* EventNode = nullptr;
    
    // Find the function to create the event
    UClass* BlueprintClass = Blueprint->GeneratedClass;
    UFunction* EventFunction = BlueprintClass->FindFunctionByName(FName(*EventName));
    
    if (EventFunction)
    {
        EventNode = NewObject<UK2Node_Event>(Graph);
        EventNode->EventReference.SetExternalMember(FName(*EventName), BlueprintClass);
        EventNode->NodePosX = Position.X;
        EventNode->NodePosY = Position.Y;
        Graph->AddNode(EventNode, true);
        EventNode->PostPlacedNewNode();
        EventNode->AllocateDefaultPins();
        UE_LOG(LogTemp, Display, TEXT("Created new event node with name %s (ID: %s)"), 
            *EventName, *EventNode->NodeGuid.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to find function for event name: %s"), *EventName);
    }
    
    return EventNode;
}

UK2Node_CallFunction* FUnrealMCPCommonUtils::CreateFunctionCallNode(UEdGraph* Graph, UFunction* Function, const FVector2D& Position)
{
    if (!Graph || !Function)
    {
        return nullptr;
    }
    
    UK2Node_CallFunction* FunctionNode = NewObject<UK2Node_CallFunction>(Graph);
    FunctionNode->SetFromFunction(Function);
    FunctionNode->NodePosX = Position.X;
    FunctionNode->NodePosY = Position.Y;
    Graph->AddNode(FunctionNode, true);
    FunctionNode->CreateNewGuid();
    FunctionNode->PostPlacedNewNode();
    FunctionNode->AllocateDefaultPins();
    
    return FunctionNode;
}

UK2Node_VariableGet* FUnrealMCPCommonUtils::CreateVariableGetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position)
{
    if (!Graph || !Blueprint)
    {
        return nullptr;
    }
    
    UK2Node_VariableGet* VariableGetNode = NewObject<UK2Node_VariableGet>(Graph);
    
    FName VarName(*VariableName);
    FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VarName);
    
    if (Property)
    {
        VariableGetNode->VariableReference.SetFromField<FProperty>(Property, false);
        VariableGetNode->NodePosX = Position.X;
        VariableGetNode->NodePosY = Position.Y;
        Graph->AddNode(VariableGetNode, true);
        VariableGetNode->PostPlacedNewNode();
        VariableGetNode->AllocateDefaultPins();
        
        return VariableGetNode;
    }
    
    return nullptr;
}

UK2Node_VariableSet* FUnrealMCPCommonUtils::CreateVariableSetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position)
{
    if (!Graph || !Blueprint)
    {
        return nullptr;
    }
    
    UK2Node_VariableSet* VariableSetNode = NewObject<UK2Node_VariableSet>(Graph);
    
    FName VarName(*VariableName);
    FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VarName);
    
    if (Property)
    {
        VariableSetNode->VariableReference.SetFromField<FProperty>(Property, false);
        VariableSetNode->NodePosX = Position.X;
        VariableSetNode->NodePosY = Position.Y;
        Graph->AddNode(VariableSetNode, true);
        VariableSetNode->PostPlacedNewNode();
        VariableSetNode->AllocateDefaultPins();
        
        return VariableSetNode;
    }
    
    return nullptr;
}

UK2Node_InputAction* FUnrealMCPCommonUtils::CreateInputActionNode(UEdGraph* Graph, const FString& ActionName, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    UK2Node_InputAction* InputActionNode = NewObject<UK2Node_InputAction>(Graph);
    InputActionNode->InputActionName = FName(*ActionName);
    InputActionNode->NodePosX = Position.X;
    InputActionNode->NodePosY = Position.Y;
    Graph->AddNode(InputActionNode, true);
    InputActionNode->CreateNewGuid();
    InputActionNode->PostPlacedNewNode();
    InputActionNode->AllocateDefaultPins();
    
    return InputActionNode;
}

UK2Node_Self* FUnrealMCPCommonUtils::CreateSelfReferenceNode(UEdGraph* Graph, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    UK2Node_Self* SelfNode = NewObject<UK2Node_Self>(Graph);
    SelfNode->NodePosX = Position.X;
    SelfNode->NodePosY = Position.Y;
    Graph->AddNode(SelfNode, true);
    SelfNode->CreateNewGuid();
    SelfNode->PostPlacedNewNode();
    SelfNode->AllocateDefaultPins();
    
    return SelfNode;
}

bool FUnrealMCPCommonUtils::ConnectGraphNodes(UEdGraph* Graph, UEdGraphNode* SourceNode, const FString& SourcePinName, 
                                           UEdGraphNode* TargetNode, const FString& TargetPinName)
{
    if (!Graph || !SourceNode || !TargetNode)
    {
        return false;
    }
    
    UEdGraphPin* SourcePin = FindPin(SourceNode, SourcePinName, EGPD_Output);
    UEdGraphPin* TargetPin = FindPin(TargetNode, TargetPinName, EGPD_Input);
    
    if (SourcePin && TargetPin)
    {
        SourcePin->MakeLinkTo(TargetPin);
        return true;
    }
    
    return false;
}

UEdGraphPin* FUnrealMCPCommonUtils::FindPin(UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Direction)
{
    if (!Node)
    {
        return nullptr;
    }
    
    // Log all pins for debugging
    UE_LOG(LogTemp, Display, TEXT("FindPin: Looking for pin '%s' (Direction: %d) in node '%s'"), 
           *PinName, (int32)Direction, *Node->GetName());
    
    for (UEdGraphPin* Pin : Node->Pins)
    {
        UE_LOG(LogTemp, Display, TEXT("  - Available pin: '%s', Direction: %d, Category: %s"), 
               *Pin->PinName.ToString(), (int32)Pin->Direction, *Pin->PinType.PinCategory.ToString());
    }
    
    // First try exact match
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin->PinName.ToString() == PinName && (Direction == EGPD_MAX || Pin->Direction == Direction))
        {
            UE_LOG(LogTemp, Display, TEXT("  - Found exact matching pin: '%s'"), *Pin->PinName.ToString());
            return Pin;
        }
    }
    
    // If no exact match and we're looking for a component reference, try case-insensitive match
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase) && 
            (Direction == EGPD_MAX || Pin->Direction == Direction))
        {
            UE_LOG(LogTemp, Display, TEXT("  - Found case-insensitive matching pin: '%s'"), *Pin->PinName.ToString());
            return Pin;
        }
    }
    
    // If we're looking for a component output and didn't find it by name, try to find the first data output pin
    if (Direction == EGPD_Output && Cast<UK2Node_VariableGet>(Node) != nullptr)
    {
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
            {
                UE_LOG(LogTemp, Display, TEXT("  - Found fallback data output pin: '%s'"), *Pin->PinName.ToString());
                return Pin;
            }
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("  - No matching pin found for '%s'"), *PinName);
    return nullptr;
}

// Actor utilities
TSharedPtr<FJsonValue> FUnrealMCPCommonUtils::ActorToJson(AActor* Actor)
{
    if (!Actor)
    {
        return MakeShared<FJsonValueNull>();
    }
    
    TSharedPtr<FJsonObject> ActorObject = MakeShared<FJsonObject>();
    ActorObject->SetStringField(TEXT("name"), Actor->GetName());
    ActorObject->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
    
    FVector Location = Actor->GetActorLocation();
    TArray<TSharedPtr<FJsonValue>> LocationArray;
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.X));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Y));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Z));
    ActorObject->SetArrayField(TEXT("location"), LocationArray);
    
    FRotator Rotation = Actor->GetActorRotation();
    TArray<TSharedPtr<FJsonValue>> RotationArray;
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
    ActorObject->SetArrayField(TEXT("rotation"), RotationArray);
    
    FVector Scale = Actor->GetActorScale3D();
    TArray<TSharedPtr<FJsonValue>> ScaleArray;
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.X));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Y));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Z));
    ActorObject->SetArrayField(TEXT("scale"), ScaleArray);
    
    return MakeShared<FJsonValueObject>(ActorObject);
}

TSharedPtr<FJsonObject> FUnrealMCPCommonUtils::ActorToJsonObject(AActor* Actor, bool bDetailed)
{
    if (!Actor)
    {
        return nullptr;
    }
    
    TSharedPtr<FJsonObject> ActorObject = MakeShared<FJsonObject>();
    ActorObject->SetStringField(TEXT("name"), Actor->GetName());
    ActorObject->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
    
    FVector Location = Actor->GetActorLocation();
    TArray<TSharedPtr<FJsonValue>> LocationArray;
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.X));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Y));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Z));
    ActorObject->SetArrayField(TEXT("location"), LocationArray);
    
    FRotator Rotation = Actor->GetActorRotation();
    TArray<TSharedPtr<FJsonValue>> RotationArray;
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
    ActorObject->SetArrayField(TEXT("rotation"), RotationArray);
    
    FVector Scale = Actor->GetActorScale3D();
    TArray<TSharedPtr<FJsonValue>> ScaleArray;
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.X));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Y));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Z));
    ActorObject->SetArrayField(TEXT("scale"), ScaleArray);
    
    return ActorObject;
}

UK2Node_Event* FUnrealMCPCommonUtils::FindExistingEventNode(UEdGraph* Graph, const FString& EventName)
{
    if (!Graph)
    {
        return nullptr;
    }

    // Look for existing event nodes
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
        if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
        {
            UE_LOG(LogTemp, Display, TEXT("Found existing event node with name: %s"), *EventName);
            return EventNode;
        }
    }

    return nullptr;
}

bool FUnrealMCPCommonUtils::SetObjectProperty(UObject* Object, const FString& PropertyName, 
                                     const TSharedPtr<FJsonValue>& Value, FString& OutErrorMessage)
{
    if (!Object)
    {
        OutErrorMessage = TEXT("Invalid object");
        return false;
    }

    FProperty* Property = Object->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property)
    {
        OutErrorMessage = FString::Printf(TEXT("Property not found: %s"), *PropertyName);
        return false;
    }

    void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(Object);
    
    // Handle different property types
    if (Property->IsA<FBoolProperty>())
    {
        ((FBoolProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsBool());
        return true;
    }
    else if (Property->IsA<FIntProperty>())
    {
        int32 IntValue = static_cast<int32>(Value->AsNumber());
        FIntProperty* IntProperty = CastField<FIntProperty>(Property);
        if (IntProperty)
        {
            IntProperty->SetPropertyValue_InContainer(Object, IntValue);
            return true;
        }
    }
    else if (Property->IsA<FFloatProperty>())
    {
        ((FFloatProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsNumber());
        return true;
    }
    else if (Property->IsA<FStrProperty>())
    {
        ((FStrProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsString());
        return true;
    }
    else if (Property->IsA<FByteProperty>())
    {
        FByteProperty* ByteProp = CastField<FByteProperty>(Property);
        UEnum* EnumDef = ByteProp ? ByteProp->GetIntPropertyEnum() : nullptr;
        
        // If this is a TEnumAsByte property (has associated enum)
        if (EnumDef)
        {
            // Handle numeric value
            if (Value->Type == EJson::Number)
            {
                uint8 ByteValue = static_cast<uint8>(Value->AsNumber());
                ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
                
                UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric value: %d"), 
                      *PropertyName, ByteValue);
                return true;
            }
            // Handle string enum value
            else if (Value->Type == EJson::String)
            {
                FString EnumValueName = Value->AsString();
                
                // Try to convert numeric string to number first
                if (EnumValueName.IsNumeric())
                {
                    uint8 ByteValue = FCString::Atoi(*EnumValueName);
                    ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric string value: %s -> %d"), 
                          *PropertyName, *EnumValueName, ByteValue);
                    return true;
                }
                
                // Handle qualified enum names (e.g., "Player0" or "EAutoReceiveInput::Player0")
                if (EnumValueName.Contains(TEXT("::")))
                {
                    EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
                }
                
                int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
                if (EnumValue == INDEX_NONE)
                {
                    // Try with full name as fallback
                    EnumValue = EnumDef->GetValueByNameString(Value->AsString());
                }
                
                if (EnumValue != INDEX_NONE)
                {
                    ByteProp->SetPropertyValue(PropertyAddr, static_cast<uint8>(EnumValue));
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to name value: %s -> %lld"), 
                          *PropertyName, *EnumValueName, EnumValue);
                    return true;
                }
                else
                {
                    // Log all possible enum values for debugging
                    UE_LOG(LogTemp, Warning, TEXT("Could not find enum value for '%s'. Available options:"), *EnumValueName);
                    for (int32 i = 0; i < EnumDef->NumEnums(); i++)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("  - %s (value: %d)"), 
                               *EnumDef->GetNameStringByIndex(i), EnumDef->GetValueByIndex(i));
                    }
                    
                    OutErrorMessage = FString::Printf(TEXT("Could not find enum value for '%s'"), *EnumValueName);
                    return false;
                }
            }
        }
        else
        {
            // Regular byte property
            uint8 ByteValue = static_cast<uint8>(Value->AsNumber());
            ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
            return true;
        }
    }
    else if (Property->IsA<FEnumProperty>())
    {
        FEnumProperty* EnumProp = CastField<FEnumProperty>(Property);
        UEnum* EnumDef = EnumProp ? EnumProp->GetEnum() : nullptr;
        FNumericProperty* UnderlyingNumericProp = EnumProp ? EnumProp->GetUnderlyingProperty() : nullptr;
        
        if (EnumDef && UnderlyingNumericProp)
        {
            // Handle numeric value
            if (Value->Type == EJson::Number)
            {
                int64 EnumValue = static_cast<int64>(Value->AsNumber());
                UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                
                UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric value: %lld"), 
                      *PropertyName, EnumValue);
                return true;
            }
            // Handle string enum value
            else if (Value->Type == EJson::String)
            {
                FString EnumValueName = Value->AsString();
                
                // Try to convert numeric string to number first
                if (EnumValueName.IsNumeric())
                {
                    int64 EnumValue = FCString::Atoi64(*EnumValueName);
                    UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric string value: %s -> %lld"), 
                          *PropertyName, *EnumValueName, EnumValue);
                    return true;
                }
                
                // Handle qualified enum names
                if (EnumValueName.Contains(TEXT("::")))
                {
                    EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
                }
                
                int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
                if (EnumValue == INDEX_NONE)
                {
                    // Try with full name as fallback
                    EnumValue = EnumDef->GetValueByNameString(Value->AsString());
                }
                
                if (EnumValue != INDEX_NONE)
                {
                    UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to name value: %s -> %lld"), 
                          *PropertyName, *EnumValueName, EnumValue);
                    return true;
                }
                else
                {
                    // Log all possible enum values for debugging
                    UE_LOG(LogTemp, Warning, TEXT("Could not find enum value for '%s'. Available options:"), *EnumValueName);
                    for (int32 i = 0; i < EnumDef->NumEnums(); i++)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("  - %s (value: %d)"), 
                               *EnumDef->GetNameStringByIndex(i), EnumDef->GetValueByIndex(i));
                    }
                    
                    OutErrorMessage = FString::Printf(TEXT("Could not find enum value for '%s'"), *EnumValueName);
                    return false;
                }
            }
        }
    }
    
    OutErrorMessage = FString::Printf(TEXT("Unsupported property type: %s for property %s"),
                                    *Property->GetClass()->GetName(), *PropertyName);
    return false;
}

static TSharedPtr<FJsonValue> UnrealMcpPropertyValueToJson(FProperty* Property, void* PropertyAddr, FString& OutErrorMessage, bool& bOutSupported)
{
    bOutSupported = true;

    if (!Property)
    {
        bOutSupported = false;
        OutErrorMessage = TEXT("Invalid property");
        return MakeShared<FJsonValueNull>();
    }

    if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
    {
        return MakeShared<FJsonValueBoolean>(BoolProp->GetPropertyValue(PropertyAddr));
    }

    if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
    {
        if (NumProp->IsInteger())
        {
            const int64 V = NumProp->GetSignedIntPropertyValue(PropertyAddr);
            return MakeShared<FJsonValueNumber>((double)V);
        }
        if (NumProp->IsFloatingPoint())
        {
            const double V = NumProp->GetFloatingPointPropertyValue(PropertyAddr);
            return MakeShared<FJsonValueNumber>(V);
        }
    }

    if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
    {
        return MakeShared<FJsonValueString>(StrProp->GetPropertyValue(PropertyAddr));
    }

    if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
    {
        return MakeShared<FJsonValueString>(NameProp->GetPropertyValue(PropertyAddr).ToString());
    }

    if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
    {
        return MakeShared<FJsonValueString>(TextProp->GetPropertyValue(PropertyAddr).ToString());
    }

    if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
    {
        UEnum* EnumDef = ByteProp->GetIntPropertyEnum();
        const uint8 V = ByteProp->GetPropertyValue(PropertyAddr);

        if (EnumDef)
        {
            const FString Name = EnumDef->GetNameStringByValue((int64)V);
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("name"), Name);
            Obj->SetNumberField(TEXT("value"), (double)V);
            Obj->SetStringField(TEXT("enum"), EnumDef->GetName());
            return MakeShared<FJsonValueObject>(Obj);
        }

        return MakeShared<FJsonValueNumber>((double)V);
    }

    if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
    {
        UEnum* EnumDef = EnumProp->GetEnum();
        FNumericProperty* Underlying = EnumProp->GetUnderlyingProperty();
        if (EnumDef && Underlying)
        {
            const int64 V = Underlying->GetSignedIntPropertyValue(PropertyAddr);
            const FString Name = EnumDef->GetNameStringByValue(V);
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("name"), Name);
            Obj->SetNumberField(TEXT("value"), (double)V);
            Obj->SetStringField(TEXT("enum"), EnumDef->GetName());
            return MakeShared<FJsonValueObject>(Obj);
        }
    }

    if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
        if (StructProp->Struct == TBaseStructure<FVector>::Get())
        {
            const FVector* V = (const FVector*)PropertyAddr;
            TArray<TSharedPtr<FJsonValue>> Arr;
            Arr.Add(MakeShared<FJsonValueNumber>(V->X));
            Arr.Add(MakeShared<FJsonValueNumber>(V->Y));
            Arr.Add(MakeShared<FJsonValueNumber>(V->Z));
            return MakeShared<FJsonValueArray>(Arr);
        }
        if (StructProp->Struct == TBaseStructure<FRotator>::Get())
        {
            const FRotator* R = (const FRotator*)PropertyAddr;
            TArray<TSharedPtr<FJsonValue>> Arr;
            Arr.Add(MakeShared<FJsonValueNumber>(R->Pitch));
            Arr.Add(MakeShared<FJsonValueNumber>(R->Yaw));
            Arr.Add(MakeShared<FJsonValueNumber>(R->Roll));
            return MakeShared<FJsonValueArray>(Arr);
        }
        if (StructProp->Struct == TBaseStructure<FVector2D>::Get())
        {
            const FVector2D* V = (const FVector2D*)PropertyAddr;
            TArray<TSharedPtr<FJsonValue>> Arr;
            Arr.Add(MakeShared<FJsonValueNumber>(V->X));
            Arr.Add(MakeShared<FJsonValueNumber>(V->Y));
            return MakeShared<FJsonValueArray>(Arr);
        }
        if (StructProp->Struct == TBaseStructure<FLinearColor>::Get())
        {
            const FLinearColor* C = (const FLinearColor*)PropertyAddr;
            TArray<TSharedPtr<FJsonValue>> Arr;
            Arr.Add(MakeShared<FJsonValueNumber>(C->R));
            Arr.Add(MakeShared<FJsonValueNumber>(C->G));
            Arr.Add(MakeShared<FJsonValueNumber>(C->B));
            Arr.Add(MakeShared<FJsonValueNumber>(C->A));
            return MakeShared<FJsonValueArray>(Arr);
        }
        if (StructProp->Struct == TBaseStructure<FTransform>::Get())
        {
            const FTransform* T = (const FTransform*)PropertyAddr;
            const FVector L = T->GetLocation();
            const FRotator Rot = T->GetRotation().Rotator();
            const FVector S = T->GetScale3D();

            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

            TArray<TSharedPtr<FJsonValue>> Loc;
            Loc.Add(MakeShared<FJsonValueNumber>(L.X));
            Loc.Add(MakeShared<FJsonValueNumber>(L.Y));
            Loc.Add(MakeShared<FJsonValueNumber>(L.Z));
            Obj->SetArrayField(TEXT("location"), Loc);

            TArray<TSharedPtr<FJsonValue>> RotArr;
            RotArr.Add(MakeShared<FJsonValueNumber>(Rot.Pitch));
            RotArr.Add(MakeShared<FJsonValueNumber>(Rot.Yaw));
            RotArr.Add(MakeShared<FJsonValueNumber>(Rot.Roll));
            Obj->SetArrayField(TEXT("rotation"), RotArr);

            TArray<TSharedPtr<FJsonValue>> Scale;
            Scale.Add(MakeShared<FJsonValueNumber>(S.X));
            Scale.Add(MakeShared<FJsonValueNumber>(S.Y));
            Scale.Add(MakeShared<FJsonValueNumber>(S.Z));
            Obj->SetArrayField(TEXT("scale"), Scale);

            return MakeShared<FJsonValueObject>(Obj);

        }

        // Fallback: export as text
        bOutSupported = false;
    }

    if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
    {
        UObject* Obj = ObjProp->GetObjectPropertyValue(PropertyAddr);
        if (!Obj)
        {
            return MakeShared<FJsonValueNull>();
        }
        return MakeShared<FJsonValueString>(Obj->GetPathName());
    }

    if (FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property))
    {
        const FSoftObjectPtr* Ptr = (const FSoftObjectPtr*)PropertyAddr;
        return MakeShared<FJsonValueString>(Ptr->ToSoftObjectPath().ToString());
    }

    if (FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Property))
    {
        const FSoftObjectPtr* Ptr = (const FSoftObjectPtr*)PropertyAddr;
        return MakeShared<FJsonValueString>(Ptr->ToSoftObjectPath().ToString());
    }

    if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
    {
        FScriptArrayHelper Helper(ArrayProp, PropertyAddr);
        TArray<TSharedPtr<FJsonValue>> Out;
        Out.Reserve(Helper.Num());
        for (int32 i = 0; i < Helper.Num(); i++)
        {
            void* ElemAddr = Helper.GetRawPtr(i);
            bool bElemSupported = true;
            FString ElemErr;
            Out.Add(UnrealMcpPropertyValueToJson(ArrayProp->Inner, ElemAddr, ElemErr, bElemSupported));
            if (!bElemSupported)
            {
                // keep going; we'll provide export_text at the top-level
                bOutSupported = false;
            }
        }
        return MakeShared<FJsonValueArray>(Out);
    }

    // Last resort: export text (stable for most properties)
    bOutSupported = false;
    return MakeShared<FJsonValueNull>();
}

bool FUnrealMCPCommonUtils::GetObjectProperty(UObject* Object, const FString& PropertyName,
    TSharedPtr<FJsonValue>& OutValue,
    FString& OutErrorMessage,
    FString* OutCppType,
    FString* OutExportText)
{
    OutValue = MakeShared<FJsonValueNull>();
    OutErrorMessage.Empty();

    if (!Object)
    {
        OutErrorMessage = TEXT("Invalid object");
        return false;
    }

    FProperty* Property = Object->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property)
    {
        OutErrorMessage = FString::Printf(TEXT("Property not found: %s"), *PropertyName);
        return false;
    }

    if (OutCppType)
        *OutCppType = Property->GetCPPType();

    void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(Object);

    FString ExportText;
    Property->ExportTextItem_Direct(ExportText, PropertyAddr, nullptr, Object, PPF_None);

    if (OutExportText)
        *OutExportText = ExportText;

    bool bSupported = true;
    FString ConvErr;
    TSharedPtr<FJsonValue> Value = UnrealMcpPropertyValueToJson(Property, PropertyAddr, ConvErr, bSupported);

    if (bSupported)
    {
        OutValue = Value;
        return true;
    }

    // Fallback: always return something readable
    TSharedPtr<FJsonObject> Fallback = MakeShared<FJsonObject>();
    Fallback->SetStringField(TEXT("format"), TEXT("export_text"));
    Fallback->SetStringField(TEXT("export_text"), ExportText);
    Fallback->SetStringField(TEXT("cpp_type"), Property->GetCPPType());
    OutValue = MakeShared<FJsonValueObject>(Fallback);
    return true;
}

bool FUnrealMCPCommonUtils::ExportObjectProperties(UObject* Object,
    const TArray<FString>& PropertyNames,
    TSharedPtr<FJsonObject>& OutProps,
    FString& OutErrorMessage,
    bool bOnlyEditable)
{
    OutProps = MakeShared<FJsonObject>();
    OutErrorMessage.Empty();

    if (!Object)
    {
        OutErrorMessage = TEXT("Invalid object");
        return false;
    }

    auto ShouldExport = [&](FProperty* Prop) -> bool
    {
        if (!Prop)
            return false;

        // Avoid noisy/unsafe fields
        if (Prop->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
            return false;

        if (bOnlyEditable && !Prop->HasAnyPropertyFlags(CPF_Edit))
            return false;

        return true;
    };

    if (PropertyNames.Num() > 0)
    {
        for (const FString& Name : PropertyNames)
        {
            if (Name.IsEmpty())
                continue;

            FProperty* Prop = Object->GetClass()->FindPropertyByName(*Name);
            if (!Prop)
            {
                // Keep going; report missing as export_text error object
                TSharedPtr<FJsonObject> Missing = MakeShared<FJsonObject>();
                Missing->SetStringField(TEXT("error"), TEXT("Property not found"));
                OutProps->SetObjectField(Name, Missing);
                continue;
            }

            if (!ShouldExport(Prop))
                continue;

            TSharedPtr<FJsonValue> V;
            FString Err;
            FString CppType;
            FString ExportText;
            if (GetObjectProperty(Object, Name, V, Err, &CppType, &ExportText))
            {
                OutProps->SetField(Name, V);
            }
            else
            {
                TSharedPtr<FJsonObject> Fail = MakeShared<FJsonObject>();
                Fail->SetStringField(TEXT("error"), Err);
                OutProps->SetObjectField(Name, Fail);
            }
        }

        return true;
    }

    // Export all
    for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
    {
        FProperty* Prop = *It;
        if (!ShouldExport(Prop))
            continue;

        const FString Name = Prop->GetName();
        TSharedPtr<FJsonValue> V;
        FString Err;
        if (GetObjectProperty(Object, Name, V, Err, nullptr, nullptr))
        {
            OutProps->SetField(Name, V);
        }
    }

    return true;
}

