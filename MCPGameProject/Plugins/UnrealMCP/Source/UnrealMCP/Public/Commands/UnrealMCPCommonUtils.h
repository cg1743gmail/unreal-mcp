#pragma once

#include "CoreMinimal.h"
#include "Json.h"

// Forward declarations
class AActor;
class UObject;
class UBlueprint;
class UWidgetBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UK2Node_Event;
class UK2Node_CallFunction;
class UK2Node_VariableGet;
class UK2Node_VariableSet;
class UK2Node_InputAction;
class UK2Node_Self;
class UFunction;



/**
 * Common utilities for UnrealMCP commands
 */
class UNREALMCP_API FUnrealMCPCommonUtils
{
public:
    // JSON utilities
    static TSharedPtr<FJsonObject> CreateErrorResponse(const FString& Message);
    static TSharedPtr<FJsonObject> CreateErrorResponseEx(const FString& Message, const FString& Code = TEXT("ERR_GENERIC"), const FString& Details = TEXT(""));
    static TSharedPtr<FJsonObject> CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data = nullptr);
    static void GetIntArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<int32>& OutArray);
    static void GetFloatArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<float>& OutArray);
    static FVector2D GetVector2DFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName);
    static FVector GetVectorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName);
    static FRotator GetRotatorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName);
    
    // Actor utilities
    static TSharedPtr<FJsonValue> ActorToJson(AActor* Actor);
    static TSharedPtr<FJsonObject> ActorToJsonObject(AActor* Actor, bool bDetailed = false);
    
    // Blueprint utilities
    // NOTE: BlueprintName may be either a short asset name (e.g. "BP_Player") or a long package/object path (e.g. "/Game/Foo/BP_Player" or "/Game/Foo/BP_Player.BP_Player").
    static UBlueprint* FindBlueprint(const FString& BlueprintName);
    static UBlueprint* FindBlueprintByName(const FString& BlueprintName);
    static UEdGraph* FindOrCreateEventGraph(UBlueprint* Blueprint);

    // Asset path utilities (configurable + safe defaults)
    static FString GetDefaultBlueprintFolder();
    static FString GetDefaultWidgetFolder();
    static void GetAllowedWriteRoots(TArray<FString>& OutRoots);
    static bool IsWritePathAllowed(const FString& LongPackageOrAssetPath, FString& OutError);

    static bool NormalizeLongPackageFolder(const FString& InFolder, FString& OutFolder, FString& OutError);
    static bool NormalizeLongPackageAssetPath(const FString& InAssetPath, FString& OutAssetPath, FString& OutError);
    static bool MakeObjectPathFromAssetPath(const FString& LongPackageAssetPath, FString& OutObjectPath, FString& OutError);

    // Response helpers: canonical asset/object paths
    // - resolved_asset_path: long package asset path ("/Game/.../AssetName")
    // - object_path: object path ("/Game/.../AssetName.AssetName")
    static void AddResolvedAssetFields(const TSharedPtr<FJsonObject>& Obj, const FString& AnyAssetOrObjectPath);
    static void AddResolvedAssetFieldsFromUObject(const TSharedPtr<FJsonObject>& Obj, const UObject* Asset);

    static UObject* LoadAssetByPathSmart(const FString& InPath);

    static UBlueprint* ResolveBlueprintFromNameOrPath(const FString& BlueprintName, const FString& BlueprintPath, FString& OutResolvedAssetPath, TArray<FString>& OutCandidates);
    static UWidgetBlueprint* ResolveWidgetBlueprintFromNameOrPath(const FString& BlueprintName, const FString& BlueprintPath, FString& OutResolvedAssetPath, TArray<FString>& OutCandidates);

    
    // Blueprint node utilities
    static UK2Node_Event* CreateEventNode(UEdGraph* Graph, const FString& EventName, const FVector2D& Position);
    static UK2Node_CallFunction* CreateFunctionCallNode(UEdGraph* Graph, UFunction* Function, const FVector2D& Position);
    static UK2Node_VariableGet* CreateVariableGetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position);
    static UK2Node_VariableSet* CreateVariableSetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position);
    static UK2Node_InputAction* CreateInputActionNode(UEdGraph* Graph, const FString& ActionName, const FVector2D& Position);
    static UK2Node_Self* CreateSelfReferenceNode(UEdGraph* Graph, const FVector2D& Position);
    static bool ConnectGraphNodes(UEdGraph* Graph, UEdGraphNode* SourceNode, const FString& SourcePinName, 
                                UEdGraphNode* TargetNode, const FString& TargetPinName);
    static UEdGraphPin* FindPin(UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Direction = EGPD_MAX);
    static UK2Node_Event* FindExistingEventNode(UEdGraph* Graph, const FString& EventName);

    // Property utilities
    static bool SetObjectProperty(UObject* Object, const FString& PropertyName,
                                 const TSharedPtr<FJsonValue>& Value, FString& OutErrorMessage);

    // Read property utilities (mirror of SetObjectProperty)
    // - OutCppType: optional, returns Property->GetCPPType()
    // - OutExportText: optional, returns ExportTextItem() representation for debugging/fallback
    static bool GetObjectProperty(UObject* Object, const FString& PropertyName,
                                 TSharedPtr<FJsonValue>& OutValue,
                                 FString& OutErrorMessage,
                                 FString* OutCppType = nullptr,
                                 FString* OutExportText = nullptr);

    // Export properties to JSON object
    // - If PropertyNames is empty, exports all reflected properties (excluding transient).
    // - If bOnlyEditable is true, exports only properties that are editable in editor (EditAnywhere/EditDefaultsOnly/etc.).
    static bool ExportObjectProperties(UObject* Object,
                                      const TArray<FString>& PropertyNames,
                                      TSharedPtr<FJsonObject>& OutProps,
                                      FString& OutErrorMessage,
                                      bool bOnlyEditable = true);
};
 