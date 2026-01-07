#include "Commands/UnrealMCPBlueprintNodeCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "GameFramework/InputSettings.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "EdGraphSchema_K2.h"
#include "ScopedTransaction.h"

// Declare the log category
DEFINE_LOG_CATEGORY_STATIC(LogUnrealMCP, Log, All);

FUnrealMCPBlueprintNodeCommands::FUnrealMCPBlueprintNodeCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("connect_blueprint_nodes"))
    {
        return HandleConnectBlueprintNodes(Params);
    }
    else if (CommandType == TEXT("add_blueprint_get_self_component_reference"))
    {
        return HandleAddBlueprintGetSelfComponentReference(Params);
    }
    else if (CommandType == TEXT("add_blueprint_event_node"))
    {
        return HandleAddBlueprintEvent(Params);
    }
    else if (CommandType == TEXT("add_blueprint_function_node"))
    {
        return HandleAddBlueprintFunctionCall(Params);
    }
    else if (CommandType == TEXT("add_blueprint_variable"))
    {
        return HandleAddBlueprintVariable(Params);
    }
    else if (CommandType == TEXT("add_blueprint_input_action_node"))
    {
        return HandleAddBlueprintInputActionNode(Params);
    }
    else if (CommandType == TEXT("add_blueprint_self_reference"))
    {
        return HandleAddBlueprintSelfReference(Params);
    }
    else if (CommandType == TEXT("find_blueprint_nodes"))
    {
        return HandleFindBlueprintNodes(Params);
    }
    // Construction Script graph operations
    else if (CommandType == TEXT("get_construction_script_graph"))
    {
        return HandleGetConstructionScriptGraph(Params);
    }
    else if (CommandType == TEXT("add_construction_script_node"))
    {
        return HandleAddConstructionScriptNode(Params);
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown blueprint node command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleConnectBlueprintNodes(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Optional (recommended): disambiguate by canonical asset path
    FString BlueprintPath;
    Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath);

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

    // Resolve the Blueprint (path recommended; name-only is allowed if unique)
    FString ResolvedPath;
    TArray<FString> Candidates;
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::ResolveBlueprintFromNameOrPath(BlueprintName, BlueprintPath, ResolvedPath, Candidates);
    if (!Blueprint)
    {
        FString Details;
        if (Candidates.Num() > 1)
        {
            Details = TEXT("Multiple blueprints matched by name. Please pass blueprint_path. Candidates:\n");
            for (const FString& C : Candidates)
            {
                Details += TEXT("- ") + C + TEXT("\n");
            }
        }
        return FUnrealMCPCommonUtils::CreateErrorResponseEx(
            FString::Printf(TEXT("Blueprint '%s' not found or ambiguous"), *BlueprintName),
            TEXT("ERR_ASSET_NOT_FOUND"),
            Details);
    }

    FString ObjectPath;
    {
        FString PathErr;
        FUnrealMCPCommonUtils::MakeObjectPathFromAssetPath(ResolvedPath, ObjectPath, PathErr);
    }

    // Get the event graph
    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Find the nodes
    UEdGraphNode* SourceNode = nullptr;
    UEdGraphNode* TargetNode = nullptr;
    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        if (Node->NodeGuid.ToString() == SourceNodeId)
        {
            SourceNode = Node;
        }
        else if (Node->NodeGuid.ToString() == TargetNodeId)
        {
            TargetNode = Node;
        }
    }

    if (!SourceNode || !TargetNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Source or target node not found"));
    }

    // Transaction + Modify for stable Undo/Redo
    const FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Connect Blueprint Nodes")));
    Blueprint->Modify();
    EventGraph->Modify();
    SourceNode->Modify();
    TargetNode->Modify();

    // Connect the nodes
    if (FUnrealMCPCommonUtils::ConnectGraphNodes(EventGraph, SourceNode, SourcePinName, TargetNode, TargetPinName))
    {
        // Graph wiring is structural.
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        Blueprint->MarkPackageDirty();

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("source_node_id"), SourceNodeId);
        ResultObj->SetStringField(TEXT("target_node_id"), TargetNodeId);
        ResultObj->SetStringField(TEXT("resolved_asset_path"), ResolvedPath);
        if (!ObjectPath.IsEmpty())
        {
            ResultObj->SetStringField(TEXT("object_path"), ObjectPath);
        }
        return ResultObj;
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to connect nodes"));
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleAddBlueprintGetSelfComponentReference(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Optional (recommended): disambiguate by canonical asset path
    FString BlueprintPath;
    Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath);

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    // Get position parameters (optional)
    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Resolve the Blueprint (path recommended; name-only is allowed if unique)
    FString ResolvedPath;
    TArray<FString> Candidates;
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::ResolveBlueprintFromNameOrPath(BlueprintName, BlueprintPath, ResolvedPath, Candidates);
    if (!Blueprint)
    {
        FString Details;
        if (Candidates.Num() > 1)
        {
            Details = TEXT("Multiple blueprints matched by name. Please pass blueprint_path. Candidates:\n");
            for (const FString& C : Candidates)
            {
                Details += TEXT("- ") + C + TEXT("\n");
            }
        }
        return FUnrealMCPCommonUtils::CreateErrorResponseEx(
            FString::Printf(TEXT("Blueprint '%s' not found or ambiguous"), *BlueprintName),
            TEXT("ERR_ASSET_NOT_FOUND"),
            Details);
    }

    FString ObjectPath;
    {
        FString PathErr;
        FUnrealMCPCommonUtils::MakeObjectPathFromAssetPath(ResolvedPath, ObjectPath, PathErr);
    }

    // Get the event graph
    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Transaction + Modify for stable Undo/Redo
    const FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Add GetSelf Component Reference")));
    Blueprint->Modify();
    EventGraph->Modify();

    // We'll skip component verification since the GetAllNodes API may have changed in UE5.5

    // Create the variable get node directly
    UK2Node_VariableGet* GetComponentNode = NewObject<UK2Node_VariableGet>(EventGraph);
    if (!GetComponentNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create get component node"));
    }
    GetComponentNode->SetFlags(RF_Transactional);
    GetComponentNode->Modify();

    // Set up the variable reference properly for UE5.5
    FMemberReference& VarRef = GetComponentNode->VariableReference;
    VarRef.SetSelfMember(FName(*ComponentName));

    // Set node position
    GetComponentNode->NodePosX = NodePosition.X;
    GetComponentNode->NodePosY = NodePosition.Y;

    // Add to graph
    EventGraph->AddNode(GetComponentNode);
    GetComponentNode->CreateNewGuid();
    GetComponentNode->PostPlacedNewNode();
    GetComponentNode->AllocateDefaultPins();

    // Explicitly reconstruct node for UE5.5
    GetComponentNode->ReconstructNode();

    // Graph/node insertion is structural.
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), GetComponentNode->NodeGuid.ToString());
    ResultObj->SetStringField(TEXT("resolved_asset_path"), ResolvedPath);
    if (!ObjectPath.IsEmpty())
    {
        ResultObj->SetStringField(TEXT("object_path"), ObjectPath);
    }
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleAddBlueprintEvent(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Optional (recommended): disambiguate by canonical asset path
    FString BlueprintPath;
    Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath);

    FString EventName;
    if (!Params->TryGetStringField(TEXT("event_name"), EventName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'event_name' parameter"));
    }

    // Get position parameters (optional)
    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Resolve the Blueprint (path recommended; name-only is allowed if unique)
    FString ResolvedPath;
    TArray<FString> Candidates;
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::ResolveBlueprintFromNameOrPath(BlueprintName, BlueprintPath, ResolvedPath, Candidates);
    if (!Blueprint)
    {
        FString Details;
        if (Candidates.Num() > 1)
        {
            Details = TEXT("Multiple blueprints matched by name. Please pass blueprint_path. Candidates:\n");
            for (const FString& C : Candidates)
            {
                Details += TEXT("- ") + C + TEXT("\n");
            }
        }
        return FUnrealMCPCommonUtils::CreateErrorResponseEx(
            FString::Printf(TEXT("Blueprint '%s' not found or ambiguous"), *BlueprintName),
            TEXT("ERR_ASSET_NOT_FOUND"),
            Details);
    }

    FString ObjectPath;
    {
        FString PathErr;
        FUnrealMCPCommonUtils::MakeObjectPathFromAssetPath(ResolvedPath, ObjectPath, PathErr);
    }

    // Get the event graph
    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Transaction + Modify for stable Undo/Redo
    const FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Add Blueprint Event Node")));
    Blueprint->Modify();
    EventGraph->Modify();

    // Create the event node
    UK2Node_Event* EventNode = FUnrealMCPCommonUtils::CreateEventNode(EventGraph, EventName, NodePosition);
    if (!EventNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create event node"));
    }
    EventNode->SetFlags(RF_Transactional);
    EventNode->Modify();

    // Node insertion is structural.
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), EventNode->NodeGuid.ToString());
    ResultObj->SetStringField(TEXT("resolved_asset_path"), ResolvedPath);
    if (!ObjectPath.IsEmpty())
    {
        ResultObj->SetStringField(TEXT("object_path"), ObjectPath);
    }
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleAddBlueprintFunctionCall(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Optional (recommended): disambiguate by canonical asset path
    FString BlueprintPath;
    Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath);

    FString FunctionName;
    if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
    }

    // Get position parameters (optional)
    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Check for target parameter (optional)
    FString Target;
    Params->TryGetStringField(TEXT("target"), Target);

    // Resolve the Blueprint (path recommended; name-only is allowed if unique)
    FString ResolvedPath;
    TArray<FString> Candidates;
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::ResolveBlueprintFromNameOrPath(BlueprintName, BlueprintPath, ResolvedPath, Candidates);
    if (!Blueprint)
    {
        FString Details;
        if (Candidates.Num() > 1)
        {
            Details = TEXT("Multiple blueprints matched by name. Please pass blueprint_path. Candidates:\n");
            for (const FString& C : Candidates)
            {
                Details += TEXT("- ") + C + TEXT("\n");
            }
        }
        return FUnrealMCPCommonUtils::CreateErrorResponseEx(
            FString::Printf(TEXT("Blueprint '%s' not found or ambiguous"), *BlueprintName),
            TEXT("ERR_ASSET_NOT_FOUND"),
            Details);
    }

    FString ObjectPath;
    {
        FString PathErr;
        FUnrealMCPCommonUtils::MakeObjectPathFromAssetPath(ResolvedPath, ObjectPath, PathErr);
    }

    // Get the event graph
    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Transaction + Modify for stable Undo/Redo
    const FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Add Blueprint Function Call")));
    Blueprint->Modify();
    EventGraph->Modify();

    // Find the function
    UFunction* Function = nullptr;
    UK2Node_CallFunction* FunctionNode = nullptr;
    
    // Add extensive logging for debugging
    UE_LOG(LogTemp, Display, TEXT("Looking for function '%s' in target '%s'"), 
           *FunctionName, Target.IsEmpty() ? TEXT("Blueprint") : *Target);
    
    // Check if we have a target class specified
    if (!Target.IsEmpty())
    {
        // Try to find the target class
        UClass* TargetClass = nullptr;
        
        // First try without a prefix
        TargetClass = FindObject<UClass>(ANY_PACKAGE, *Target);
        UE_LOG(LogTemp, Display, TEXT("Tried to find class '%s': %s"), 
               *Target, TargetClass ? TEXT("Found") : TEXT("Not found"));
        
        // If not found, try with U prefix (common convention for UE classes)
        if (!TargetClass && !Target.StartsWith(TEXT("U")))
        {
            FString TargetWithPrefix = FString(TEXT("U")) + Target;
            TargetClass = FindObject<UClass>(ANY_PACKAGE, *TargetWithPrefix);
            UE_LOG(LogTemp, Display, TEXT("Tried to find class '%s': %s"), 
                   *TargetWithPrefix, TargetClass ? TEXT("Found") : TEXT("Not found"));
        }
        
        // If still not found, try with common component names
        if (!TargetClass)
        {
            // Try some common component class names
            TArray<FString> PossibleClassNames;
            PossibleClassNames.Add(FString(TEXT("U")) + Target + TEXT("Component"));
            PossibleClassNames.Add(Target + TEXT("Component"));
            
            for (const FString& ClassName : PossibleClassNames)
            {
                TargetClass = FindObject<UClass>(ANY_PACKAGE, *ClassName);
                if (TargetClass)
                {
                    UE_LOG(LogTemp, Display, TEXT("Found class using alternative name '%s'"), *ClassName);
                    break;
                }
            }
        }
        
        // Special case handling for common classes like UGameplayStatics
        if (!TargetClass && Target == TEXT("UGameplayStatics"))
        {
            // For UGameplayStatics, use a direct reference to known class
            TargetClass = FindObject<UClass>(ANY_PACKAGE, TEXT("UGameplayStatics"));
            if (!TargetClass)
            {
                // Try loading it from its known package
                TargetClass = LoadObject<UClass>(nullptr, TEXT("/Script/Engine.GameplayStatics"));
                UE_LOG(LogTemp, Display, TEXT("Explicitly loading GameplayStatics: %s"), 
                       TargetClass ? TEXT("Success") : TEXT("Failed"));
            }
        }
        
        // If we found a target class, look for the function there
        if (TargetClass)
        {
            UE_LOG(LogTemp, Display, TEXT("Looking for function '%s' in class '%s'"), 
                   *FunctionName, *TargetClass->GetName());
                   
            // First try exact name
            Function = TargetClass->FindFunctionByName(*FunctionName);
            
            // If not found, try class hierarchy
            UClass* CurrentClass = TargetClass;
            while (!Function && CurrentClass)
            {
                UE_LOG(LogTemp, Display, TEXT("Searching in class: %s"), *CurrentClass->GetName());
                
                // Try exact match
                Function = CurrentClass->FindFunctionByName(*FunctionName);
                
                // Try case-insensitive match
                if (!Function)
                {
                    for (TFieldIterator<UFunction> FuncIt(CurrentClass); FuncIt; ++FuncIt)
                    {
                        UFunction* AvailableFunc = *FuncIt;
                        UE_LOG(LogTemp, Display, TEXT("  - Available function: %s"), *AvailableFunc->GetName());
                        
                        if (AvailableFunc->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
                        {
                            UE_LOG(LogTemp, Display, TEXT("  - Found case-insensitive match: %s"), *AvailableFunc->GetName());
                            Function = AvailableFunc;
                            break;
                        }
                    }
                }
                
                // Move to parent class
                CurrentClass = CurrentClass->GetSuperClass();
            }
            
            // Special handling for known functions
            if (!Function)
            {
                if (TargetClass->GetName() == TEXT("GameplayStatics") && 
                    (FunctionName == TEXT("GetActorOfClass") || FunctionName.Equals(TEXT("GetActorOfClass"), ESearchCase::IgnoreCase)))
                {
                    UE_LOG(LogTemp, Display, TEXT("Using special case handling for GameplayStatics::GetActorOfClass"));
                    
                    // Create the function node directly
                    FunctionNode = NewObject<UK2Node_CallFunction>(EventGraph);
                    if (FunctionNode)
                    {
                        // Direct setup for known function
                        FunctionNode->FunctionReference.SetExternalMember(
                            FName(TEXT("GetActorOfClass")), 
                            TargetClass
                        );
                        
                        FunctionNode->NodePosX = NodePosition.X;
                        FunctionNode->NodePosY = NodePosition.Y;
                        EventGraph->AddNode(FunctionNode);
                        FunctionNode->CreateNewGuid();
                        FunctionNode->PostPlacedNewNode();
                        FunctionNode->AllocateDefaultPins();
                        
                        UE_LOG(LogTemp, Display, TEXT("Created GetActorOfClass node directly"));
                        
                        // List all pins
                        for (UEdGraphPin* Pin : FunctionNode->Pins)
                        {
                            UE_LOG(LogTemp, Display, TEXT("  - Pin: %s, Direction: %d, Category: %s"), 
                                   *Pin->PinName.ToString(), (int32)Pin->Direction, *Pin->PinType.PinCategory.ToString());
                        }
                    }
                }
            }
        }
    }
    
    // If we still haven't found the function, try in the blueprint's class
    if (!Function && !FunctionNode)
    {
        UE_LOG(LogTemp, Display, TEXT("Trying to find function in blueprint class"));
        Function = Blueprint->GeneratedClass->FindFunctionByName(*FunctionName);
    }
    
    // Create the function call node if we found the function
    if (Function && !FunctionNode)
    {
        FunctionNode = FUnrealMCPCommonUtils::CreateFunctionCallNode(EventGraph, Function, NodePosition);
    }
    
    if (!FunctionNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Function not found: %s in target %s"), *FunctionName, Target.IsEmpty() ? TEXT("Blueprint") : *Target));
    }

    // Set parameters if provided
    if (Params->HasField(TEXT("params")))
    {
        const TSharedPtr<FJsonObject>* ParamsObj;
        if (Params->TryGetObjectField(TEXT("params"), ParamsObj))
        {
            // Process parameters
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Param : (*ParamsObj)->Values)
            {
                const FString& ParamName = Param.Key;
                const TSharedPtr<FJsonValue>& ParamValue = Param.Value;
                
                // Find the parameter pin
                UEdGraphPin* ParamPin = FUnrealMCPCommonUtils::FindPin(FunctionNode, ParamName, EGPD_Input);
                if (ParamPin)
                {
                    UE_LOG(LogTemp, Display, TEXT("Found parameter pin '%s' of category '%s'"), 
                           *ParamName, *ParamPin->PinType.PinCategory.ToString());
                    UE_LOG(LogTemp, Display, TEXT("  Current default value: '%s'"), *ParamPin->DefaultValue);
                    if (ParamPin->PinType.PinSubCategoryObject.IsValid())
                    {
                        UE_LOG(LogTemp, Display, TEXT("  Pin subcategory: '%s'"), 
                               *ParamPin->PinType.PinSubCategoryObject->GetName());
                    }
                    
                    // Set parameter based on type
                    if (ParamValue->Type == EJson::String)
                    {
                        FString StringVal = ParamValue->AsString();
                        UE_LOG(LogTemp, Display, TEXT("  Setting string parameter '%s' to: '%s'"), 
                               *ParamName, *StringVal);
                        
                        // Handle class reference parameters (e.g., ActorClass in GetActorOfClass)
                        if (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
                        {
                            // For class references, we require the exact class name with proper prefix
                            // - Actor classes must start with 'A' (e.g., ACameraActor)
                            // - Non-actor classes must start with 'U' (e.g., UObject)
                            const FString& ClassName = StringVal;
                            
                            // TODO: This likely won't work in UE5.5+, so don't rely on it.
                            UClass* Class = FindObject<UClass>(ANY_PACKAGE, *ClassName);

                            if (!Class)
                            {
                                Class = LoadObject<UClass>(nullptr, *ClassName);
                                UE_LOG(LogUnrealMCP, Display, TEXT("FindObject<UClass> failed. Assuming soft path  path: %s"), *ClassName);
                            }
                            
                            // If not found, try with Engine module path
                            if (!Class)
                            {
                                FString EngineClassName = FString::Printf(TEXT("/Script/Engine.%s"), *ClassName);
                                Class = LoadObject<UClass>(nullptr, *EngineClassName);
                                UE_LOG(LogUnrealMCP, Display, TEXT("Trying Engine module path: %s"), *EngineClassName);
                            }
                            
                            if (!Class)
                            {
                                UE_LOG(LogUnrealMCP, Error, TEXT("Failed to find class '%s'. Make sure to use the exact class name with proper prefix (A for actors, U for non-actors)"), *ClassName);
                                return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find class '%s'"), *ClassName));
                            }

                            const UEdGraphSchema_K2* K2Schema = Cast<const UEdGraphSchema_K2>(EventGraph->GetSchema());
                            if (!K2Schema)
                            {
                                UE_LOG(LogUnrealMCP, Error, TEXT("Failed to get K2Schema"));
                                return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get K2Schema"));
                            }

                            K2Schema->TrySetDefaultObject(*ParamPin, Class);
                            if (ParamPin->DefaultObject != Class)
                            {
                                UE_LOG(LogUnrealMCP, Error, TEXT("Failed to set class reference for pin '%s' to '%s'"), *ParamPin->PinName.ToString(), *ClassName);
                                return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to set class reference for pin '%s'"), *ParamPin->PinName.ToString()));
                            }

                            UE_LOG(LogUnrealMCP, Log, TEXT("Successfully set class reference for pin '%s' to '%s'"), *ParamPin->PinName.ToString(), *ClassName);
                            continue;
                        }
                        else if (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
                        {
                            // Ensure we're using an integer value (no decimal)
                            int32 IntValue = FMath::RoundToInt(ParamValue->AsNumber());
                            ParamPin->DefaultValue = FString::FromInt(IntValue);
                            UE_LOG(LogTemp, Display, TEXT("  Set integer parameter '%s' to: %d (string: '%s')"), 
                                   *ParamName, IntValue, *ParamPin->DefaultValue);
                        }
                        else if (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Float)
                        {
                            // For other numeric types
                            float FloatValue = ParamValue->AsNumber();
                            ParamPin->DefaultValue = FString::SanitizeFloat(FloatValue);
                            UE_LOG(LogTemp, Display, TEXT("  Set float parameter '%s' to: %f (string: '%s')"), 
                                   *ParamName, FloatValue, *ParamPin->DefaultValue);
                        }
                        else if (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
                        {
                            bool BoolValue = ParamValue->AsBool();
                            ParamPin->DefaultValue = BoolValue ? TEXT("true") : TEXT("false");
                            UE_LOG(LogTemp, Display, TEXT("  Set boolean parameter '%s' to: %s"), 
                                   *ParamName, *ParamPin->DefaultValue);
                        }
                        else if (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && ParamPin->PinType.PinSubCategoryObject == TBaseStructure<FVector>::Get())
                        {
                            // Handle array parameters - like Vector parameters
                            const TArray<TSharedPtr<FJsonValue>>* ArrayValue;
                            if (ParamValue->TryGetArray(ArrayValue))
                            {
                                // Check if this could be a vector (array of 3 numbers)
                                if (ArrayValue->Num() == 3)
                                {
                                    // Create a proper vector string: (X=0.0,Y=0.0,Z=1000.0)
                                    float X = (*ArrayValue)[0]->AsNumber();
                                    float Y = (*ArrayValue)[1]->AsNumber();
                                    float Z = (*ArrayValue)[2]->AsNumber();
                                    
                                    FString VectorString = FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), X, Y, Z);
                                    ParamPin->DefaultValue = VectorString;
                                    
                                    UE_LOG(LogTemp, Display, TEXT("  Set vector parameter '%s' to: %s"), 
                                           *ParamName, *VectorString);
                                    UE_LOG(LogTemp, Display, TEXT("  Final pin value: '%s'"), 
                                           *ParamPin->DefaultValue);
                                }
                                else
                                {
                                    UE_LOG(LogTemp, Warning, TEXT("Array parameter type not fully supported yet"));
                                }
                            }
                        }
                    }
                    else if (ParamValue->Type == EJson::Number)
                    {
                        // Handle integer vs float parameters correctly
                        if (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
                        {
                            // Ensure we're using an integer value (no decimal)
                            int32 IntValue = FMath::RoundToInt(ParamValue->AsNumber());
                            ParamPin->DefaultValue = FString::FromInt(IntValue);
                            UE_LOG(LogTemp, Display, TEXT("  Set integer parameter '%s' to: %d (string: '%s')"), 
                                   *ParamName, IntValue, *ParamPin->DefaultValue);
                        }
                        else
                        {
                            // For other numeric types
                            float FloatValue = ParamValue->AsNumber();
                            ParamPin->DefaultValue = FString::SanitizeFloat(FloatValue);
                            UE_LOG(LogTemp, Display, TEXT("  Set float parameter '%s' to: %f (string: '%s')"), 
                                   *ParamName, FloatValue, *ParamPin->DefaultValue);
                        }
                    }
                    else if (ParamValue->Type == EJson::Boolean)
                    {
                        bool BoolValue = ParamValue->AsBool();
                        ParamPin->DefaultValue = BoolValue ? TEXT("true") : TEXT("false");
                        UE_LOG(LogTemp, Display, TEXT("  Set boolean parameter '%s' to: %s"), 
                               *ParamName, *ParamPin->DefaultValue);
                    }
                    else if (ParamValue->Type == EJson::Array)
                    {
                        UE_LOG(LogTemp, Display, TEXT("  Processing array parameter '%s'"), *ParamName);
                        // Handle array parameters - like Vector parameters
                        const TArray<TSharedPtr<FJsonValue>>* ArrayValue;
                        if (ParamValue->TryGetArray(ArrayValue))
                        {
                            // Check if this could be a vector (array of 3 numbers)
                            if (ArrayValue->Num() == 3 && 
                                (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct) &&
                                (ParamPin->PinType.PinSubCategoryObject == TBaseStructure<FVector>::Get()))
                            {
                                // Create a proper vector string: (X=0.0,Y=0.0,Z=1000.0)
                                float X = (*ArrayValue)[0]->AsNumber();
                                float Y = (*ArrayValue)[1]->AsNumber();
                                float Z = (*ArrayValue)[2]->AsNumber();
                                
                                FString VectorString = FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), X, Y, Z);
                                ParamPin->DefaultValue = VectorString;
                                
                                UE_LOG(LogTemp, Display, TEXT("  Set vector parameter '%s' to: %s"), 
                                       *ParamName, *VectorString);
                                UE_LOG(LogTemp, Display, TEXT("  Final pin value: '%s'"), 
                                       *ParamPin->DefaultValue);
                            }
                            else
                            {
                                UE_LOG(LogTemp, Warning, TEXT("Array parameter type not fully supported yet"));
                            }
                        }
                    }
                    // Add handling for other types as needed
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("Parameter pin '%s' not found"), *ParamName);
                }
            }
        }
    }

    if (FunctionNode)
    {
        FunctionNode->SetFlags(RF_Transactional);
        FunctionNode->Modify();
    }

    // Node insertion is structural.
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), FunctionNode->NodeGuid.ToString());
    ResultObj->SetStringField(TEXT("resolved_asset_path"), ResolvedPath);
    if (!ObjectPath.IsEmpty())
    {
        ResultObj->SetStringField(TEXT("object_path"), ObjectPath);
    }
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleAddBlueprintVariable(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Optional (recommended): disambiguate by canonical asset path
    FString BlueprintPath;
    Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath);

    FString VariableName;
    if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'variable_name' parameter"));
    }

    FString VariableType;
    if (!Params->TryGetStringField(TEXT("variable_type"), VariableType))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'variable_type' parameter"));
    }

    // Get optional parameters
    bool IsExposed = false;
    if (Params->HasField(TEXT("is_exposed")))
    {
        IsExposed = Params->GetBoolField(TEXT("is_exposed"));
    }

    // Resolve the Blueprint (path recommended; name-only is allowed if unique)
    FString ResolvedPath;
    TArray<FString> Candidates;
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::ResolveBlueprintFromNameOrPath(BlueprintName, BlueprintPath, ResolvedPath, Candidates);
    if (!Blueprint)
    {
        FString Details;
        if (Candidates.Num() > 1)
        {
            Details = TEXT("Multiple blueprints matched by name. Please pass blueprint_path. Candidates:\n");
            for (const FString& C : Candidates)
            {
                Details += TEXT("- ") + C + TEXT("\n");
            }
        }
        return FUnrealMCPCommonUtils::CreateErrorResponseEx(
            FString::Printf(TEXT("Blueprint '%s' not found or ambiguous"), *BlueprintName),
            TEXT("ERR_ASSET_NOT_FOUND"),
            Details);
    }

    FString ObjectPath;
    {
        FString PathErr;
        FUnrealMCPCommonUtils::MakeObjectPathFromAssetPath(ResolvedPath, ObjectPath, PathErr);
    }

    // Transaction + Modify for stable Undo/Redo
    const FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Add Blueprint Variable")));
    Blueprint->Modify();

    // Create variable based on type
    FEdGraphPinType PinType;
    
    // Set up pin type based on variable_type string
    if (VariableType == TEXT("Boolean"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    }
    else if (VariableType == TEXT("Integer") || VariableType == TEXT("Int"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
    }
    else if (VariableType == TEXT("Float"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Float;
    }
    else if (VariableType == TEXT("String"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_String;
    }
    else if (VariableType == TEXT("Vector"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unsupported variable type: %s"), *VariableType));
    }

    // Create the variable
    FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VariableName), PinType);

    // Set variable properties
    FBPVariableDescription* NewVar = nullptr;
    for (FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        if (Variable.VarName == FName(*VariableName))
        {
            NewVar = &Variable;
            break;
        }
    }

    if (NewVar)
    {
        // Set exposure in editor
        if (IsExposed)
        {
            NewVar->PropertyFlags |= CPF_Edit;
        }
    }

    // Member variable changes are structural.
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("variable_name"), VariableName);
    ResultObj->SetStringField(TEXT("variable_type"), VariableType);
    ResultObj->SetStringField(TEXT("resolved_asset_path"), ResolvedPath);
    if (!ObjectPath.IsEmpty())
    {
        ResultObj->SetStringField(TEXT("object_path"), ObjectPath);
    }
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleAddBlueprintInputActionNode(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Optional (recommended): disambiguate by canonical asset path
    FString BlueprintPath;
    Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath);

    FString ActionName;
    if (!Params->TryGetStringField(TEXT("action_name"), ActionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'action_name' parameter"));
    }

    // Get position parameters (optional)
    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Resolve the Blueprint (path recommended; name-only is allowed if unique)
    FString ResolvedPath;
    TArray<FString> Candidates;
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::ResolveBlueprintFromNameOrPath(BlueprintName, BlueprintPath, ResolvedPath, Candidates);
    if (!Blueprint)
    {
        FString Details;
        if (Candidates.Num() > 1)
        {
            Details = TEXT("Multiple blueprints matched by name. Please pass blueprint_path. Candidates:\n");
            for (const FString& C : Candidates)
            {
                Details += TEXT("- ") + C + TEXT("\n");
            }
        }
        return FUnrealMCPCommonUtils::CreateErrorResponseEx(
            FString::Printf(TEXT("Blueprint '%s' not found or ambiguous"), *BlueprintName),
            TEXT("ERR_ASSET_NOT_FOUND"),
            Details);
    }

    FString ObjectPath;
    {
        FString PathErr;
        FUnrealMCPCommonUtils::MakeObjectPathFromAssetPath(ResolvedPath, ObjectPath, PathErr);
    }

    // Get the event graph
    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Transaction + Modify for stable Undo/Redo
    const FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Add Blueprint Input Action Node")));
    Blueprint->Modify();
    EventGraph->Modify();

    // Create the input action node
    UK2Node_InputAction* InputActionNode = FUnrealMCPCommonUtils::CreateInputActionNode(EventGraph, ActionName, NodePosition);
    if (!InputActionNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create input action node"));
    }
    InputActionNode->SetFlags(RF_Transactional);
    InputActionNode->Modify();

    // Node insertion is structural.
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), InputActionNode->NodeGuid.ToString());
    ResultObj->SetStringField(TEXT("resolved_asset_path"), ResolvedPath);
    if (!ObjectPath.IsEmpty())
    {
        ResultObj->SetStringField(TEXT("object_path"), ObjectPath);
    }
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleAddBlueprintSelfReference(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Optional (recommended): disambiguate by canonical asset path
    FString BlueprintPath;
    Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath);

    // Get position parameters (optional)
    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Resolve the Blueprint (path recommended; name-only is allowed if unique)
    FString ResolvedPath;
    TArray<FString> Candidates;
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::ResolveBlueprintFromNameOrPath(BlueprintName, BlueprintPath, ResolvedPath, Candidates);
    if (!Blueprint)
    {
        FString Details;
        if (Candidates.Num() > 1)
        {
            Details = TEXT("Multiple blueprints matched by name. Please pass blueprint_path. Candidates:\n");
            for (const FString& C : Candidates)
            {
                Details += TEXT("- ") + C + TEXT("\n");
            }
        }
        return FUnrealMCPCommonUtils::CreateErrorResponseEx(
            FString::Printf(TEXT("Blueprint '%s' not found or ambiguous"), *BlueprintName),
            TEXT("ERR_ASSET_NOT_FOUND"),
            Details);
    }

    FString ObjectPath;
    {
        FString PathErr;
        FUnrealMCPCommonUtils::MakeObjectPathFromAssetPath(ResolvedPath, ObjectPath, PathErr);
    }

    // Get the event graph
    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Transaction + Modify for stable Undo/Redo
    const FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Add Blueprint Self Reference Node")));
    Blueprint->Modify();
    EventGraph->Modify();

    // Create the self node
    UK2Node_Self* SelfNode = FUnrealMCPCommonUtils::CreateSelfReferenceNode(EventGraph, NodePosition);
    if (!SelfNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create self node"));
    }
    SelfNode->SetFlags(RF_Transactional);
    SelfNode->Modify();

    // Node insertion is structural.
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), SelfNode->NodeGuid.ToString());
    ResultObj->SetStringField(TEXT("resolved_asset_path"), ResolvedPath);
    if (!ObjectPath.IsEmpty())
    {
        ResultObj->SetStringField(TEXT("object_path"), ObjectPath);
    }
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleFindBlueprintNodes(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString NodeType;
    if (!Params->TryGetStringField(TEXT("node_type"), NodeType))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_type' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Create a JSON array for the node GUIDs
    TArray<TSharedPtr<FJsonValue>> NodeGuidArray;
    
    // Filter nodes by the exact requested type
    if (NodeType == TEXT("Event"))
    {
        FString EventName;
        if (!Params->TryGetStringField(TEXT("event_name"), EventName))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'event_name' parameter for Event node search"));
        }
        
        // Look for nodes with exact event name (e.g., ReceiveBeginPlay)
        for (UEdGraphNode* Node : EventGraph->Nodes)
        {
            UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
            if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
            {
                UE_LOG(LogTemp, Display, TEXT("Found event node with name %s: %s"), *EventName, *EventNode->NodeGuid.ToString());
                NodeGuidArray.Add(MakeShared<FJsonValueString>(EventNode->NodeGuid.ToString()));
            }
        }
    }
    // Add other node types as needed (InputAction, etc.)
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("node_guids"), NodeGuidArray);

    FUnrealMCPCommonUtils::AddResolvedAssetFieldsFromUObject(ResultObj, Blueprint);

    return ResultObj;
}

// ============================================================================
// Construction Script Graph Operations
// ============================================================================

UEdGraph* FUnrealMCPBlueprintNodeCommands::FindConstructionScriptGraph(UBlueprint* Blueprint) const
{
    if (!Blueprint)
    {
        return nullptr;
    }

    // Construction Script is stored in FunctionGraphs with the name "UserConstructionScript"
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (Graph && Graph->GetFName() == UEdGraphSchema_K2::FN_UserConstructionScript)
        {
            return Graph;
        }
    }

    return nullptr;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleGetConstructionScriptGraph(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Optional: disambiguate by canonical asset path
    FString BlueprintPath;
    Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath);

    // Resolve the Blueprint
    FString ResolvedPath;
    TArray<FString> Candidates;
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::ResolveBlueprintFromNameOrPath(BlueprintName, BlueprintPath, ResolvedPath, Candidates);
    if (!Blueprint)
    {
        FString Details;
        if (Candidates.Num() > 1)
        {
            Details = TEXT("Multiple blueprints matched by name. Please pass blueprint_path. Candidates:\n");
            for (const FString& C : Candidates)
            {
                Details += TEXT("- ") + C + TEXT("\n");
            }
        }
        return FUnrealMCPCommonUtils::CreateErrorResponseEx(
            FString::Printf(TEXT("Blueprint '%s' not found or ambiguous"), *BlueprintName),
            TEXT("ERR_ASSET_NOT_FOUND"),
            Details);
    }

    // Find Construction Script graph
    UEdGraph* ConstructionScriptGraph = FindConstructionScriptGraph(Blueprint);
    if (!ConstructionScriptGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint does not have a Construction Script graph. Only Actor-based blueprints have Construction Script."));
    }

    // Build result
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("graph_name"), ConstructionScriptGraph->GetName());
    ResultObj->SetNumberField(TEXT("node_count"), static_cast<double>(ConstructionScriptGraph->Nodes.Num()));

    // Find entry node
    FString EntryNodeId;
    for (UEdGraphNode* Node : ConstructionScriptGraph->Nodes)
    {
        if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
        {
            EntryNodeId = EntryNode->NodeGuid.ToString();
            break;
        }
    }
    ResultObj->SetStringField(TEXT("entry_node_id"), EntryNodeId);

    // List all nodes
    TArray<TSharedPtr<FJsonValue>> NodesArray;
    for (UEdGraphNode* Node : ConstructionScriptGraph->Nodes)
    {
        if (!Node)
            continue;

        TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
        NodeObj->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
        NodeObj->SetStringField(TEXT("node_class"), Node->GetClass()->GetName());
        NodeObj->SetStringField(TEXT("node_title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
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
                PinObj->SetBoolField(TEXT("is_connected"), Pin->LinkedTo.Num() > 0);
                PinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
            }
        }
        NodeObj->SetArrayField(TEXT("pins"), PinsArray);

        NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
    }
    ResultObj->SetArrayField(TEXT("nodes"), NodesArray);

    FUnrealMCPCommonUtils::AddResolvedAssetFields(ResultObj, ResolvedPath);

    UE_LOG(LogTemp, Log, TEXT("Retrieved Construction Script graph for blueprint: %s"), *BlueprintName);

    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleAddConstructionScriptNode(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString NodeType;
    if (!Params->TryGetStringField(TEXT("node_type"), NodeType))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_type' parameter"));
    }

    // Optional: disambiguate by canonical asset path
    FString BlueprintPath;
    Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath);

    // Get optional parameters
    FString FunctionName;
    Params->TryGetStringField(TEXT("function_name"), FunctionName);

    FString Target;
    Params->TryGetStringField(TEXT("target"), Target);

    FString VariableName;
    Params->TryGetStringField(TEXT("variable_name"), VariableName);

    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Resolve the Blueprint
    FString ResolvedPath;
    TArray<FString> Candidates;
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::ResolveBlueprintFromNameOrPath(BlueprintName, BlueprintPath, ResolvedPath, Candidates);
    if (!Blueprint)
    {
        FString Details;
        if (Candidates.Num() > 1)
        {
            Details = TEXT("Multiple blueprints matched by name. Please pass blueprint_path. Candidates:\n");
            for (const FString& C : Candidates)
            {
                Details += TEXT("- ") + C + TEXT("\n");
            }
        }
        return FUnrealMCPCommonUtils::CreateErrorResponseEx(
            FString::Printf(TEXT("Blueprint '%s' not found or ambiguous"), *BlueprintName),
            TEXT("ERR_ASSET_NOT_FOUND"),
            Details);
    }

    // Find Construction Script graph
    UEdGraph* ConstructionScriptGraph = FindConstructionScriptGraph(Blueprint);
    if (!ConstructionScriptGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint does not have a Construction Script graph. Only Actor-based blueprints have Construction Script."));
    }

    // Transaction + Modify for stable Undo/Redo
    const FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Add Construction Script Node")));
    Blueprint->Modify();
    ConstructionScriptGraph->Modify();

    UEdGraphNode* NewNode = nullptr;

    if (NodeType == TEXT("FunctionCall"))
    {
        if (FunctionName.IsEmpty())
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter for FunctionCall node"));
        }

        // Find the function
        UFunction* TargetFunction = nullptr;
        UClass* TargetClass = nullptr;

        if (!Target.IsEmpty())
        {
            // Try to find the target class
            TargetClass = FindObject<UClass>(ANY_PACKAGE, *Target);
            if (!TargetClass && !Target.StartsWith(TEXT("U")))
            {
                TargetClass = FindObject<UClass>(ANY_PACKAGE, *(TEXT("U") + Target));
            }
            if (!TargetClass)
            {
                TargetClass = FindObject<UClass>(ANY_PACKAGE, *(Target + TEXT("Component")));
            }
            if (!TargetClass)
            {
                TargetClass = FindObject<UClass>(ANY_PACKAGE, *(TEXT("U") + Target + TEXT("Component")));
            }

            if (TargetClass)
            {
                TargetFunction = TargetClass->FindFunctionByName(*FunctionName);
            }
        }

        // Try blueprint's generated class if not found
        if (!TargetFunction && Blueprint->GeneratedClass)
        {
            TargetFunction = Blueprint->GeneratedClass->FindFunctionByName(*FunctionName);
        }

        // Try common classes
        if (!TargetFunction)
        {
            // Try AActor
            TargetFunction = AActor::StaticClass()->FindFunctionByName(*FunctionName);
        }
        if (!TargetFunction)
        {
            // Try USceneComponent
            TargetFunction = USceneComponent::StaticClass()->FindFunctionByName(*FunctionName);
        }

        if (TargetFunction)
        {
            UK2Node_CallFunction* FunctionNode = NewObject<UK2Node_CallFunction>(ConstructionScriptGraph);
            FunctionNode->SetFromFunction(TargetFunction);
            FunctionNode->NodePosX = NodePosition.X;
            FunctionNode->NodePosY = NodePosition.Y;

            ConstructionScriptGraph->AddNode(FunctionNode);
            FunctionNode->CreateNewGuid();
            FunctionNode->PostPlacedNewNode();
            FunctionNode->AllocateDefaultPins();

            NewNode = FunctionNode;
        }
        else
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Function not found: %s"), *FunctionName));
        }
    }
    else if (NodeType == TEXT("VariableGet"))
    {
        if (VariableName.IsEmpty())
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'variable_name' parameter for VariableGet node"));
        }

        UK2Node_VariableGet* VarGetNode = NewObject<UK2Node_VariableGet>(ConstructionScriptGraph);
        
        // Use GUID for stable variable reference (UE5 best practice)
        FGuid VarGuid = FBlueprintEditorUtils::FindMemberVariableGuidByName(Blueprint, FName(*VariableName));
        VarGetNode->VariableReference.SetSelfMember(FName(*VariableName), VarGuid);
        
        VarGetNode->NodePosX = NodePosition.X;
        VarGetNode->NodePosY = NodePosition.Y;

        ConstructionScriptGraph->AddNode(VarGetNode);
        VarGetNode->CreateNewGuid();
        VarGetNode->PostPlacedNewNode();
        VarGetNode->AllocateDefaultPins();
        VarGetNode->ReconstructNode();

        NewNode = VarGetNode;
    }
    else if (NodeType == TEXT("VariableSet"))
    {
        if (VariableName.IsEmpty())
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'variable_name' parameter for VariableSet node"));
        }

        UK2Node_VariableSet* VarSetNode = NewObject<UK2Node_VariableSet>(ConstructionScriptGraph);
        
        // Use GUID for stable variable reference (UE5 best practice)
        FGuid VarGuid = FBlueprintEditorUtils::FindMemberVariableGuidByName(Blueprint, FName(*VariableName));
        VarSetNode->VariableReference.SetSelfMember(FName(*VariableName), VarGuid);
        
        VarSetNode->NodePosX = NodePosition.X;
        VarSetNode->NodePosY = NodePosition.Y;

        ConstructionScriptGraph->AddNode(VarSetNode);
        VarSetNode->CreateNewGuid();
        VarSetNode->PostPlacedNewNode();
        VarSetNode->AllocateDefaultPins();
        VarSetNode->ReconstructNode();

        NewNode = VarSetNode;
    }
    else if (NodeType == TEXT("Self"))
    {
        UK2Node_Self* SelfNode = NewObject<UK2Node_Self>(ConstructionScriptGraph);
        SelfNode->NodePosX = NodePosition.X;
        SelfNode->NodePosY = NodePosition.Y;

        ConstructionScriptGraph->AddNode(SelfNode);
        SelfNode->CreateNewGuid();
        SelfNode->PostPlacedNewNode();
        SelfNode->AllocateDefaultPins();

        NewNode = SelfNode;
    }
    else if (NodeType == TEXT("GetComponent"))
    {
        // Get a component reference (similar to add_blueprint_get_self_component_reference)
        FString ComponentName;
        if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter for GetComponent node"));
        }

        UK2Node_VariableGet* GetComponentNode = NewObject<UK2Node_VariableGet>(ConstructionScriptGraph);
        
        // Use GUID for stable component reference (UE5 best practice)
        FGuid CompGuid = FBlueprintEditorUtils::FindMemberVariableGuidByName(Blueprint, FName(*ComponentName));
        GetComponentNode->VariableReference.SetSelfMember(FName(*ComponentName), CompGuid);
        
        GetComponentNode->NodePosX = NodePosition.X;
        GetComponentNode->NodePosY = NodePosition.Y;

        ConstructionScriptGraph->AddNode(GetComponentNode);
        GetComponentNode->CreateNewGuid();
        GetComponentNode->PostPlacedNewNode();
        GetComponentNode->AllocateDefaultPins();
        GetComponentNode->ReconstructNode();

        NewNode = GetComponentNode;
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown node type for Construction Script: %s. Supported: FunctionCall, VariableGet, VariableSet, Self, GetComponent"), *NodeType));
    }

    if (!NewNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create node"));
    }

    NewNode->SetFlags(RF_Transactional);
    NewNode->Modify();

    // Mark blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();

    // Build result
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("node_id"), NewNode->NodeGuid.ToString());
    ResultObj->SetStringField(TEXT("node_type"), NodeType);
    ResultObj->SetStringField(TEXT("node_class"), NewNode->GetClass()->GetName());
    ResultObj->SetStringField(TEXT("graph_name"), TEXT("UserConstructionScript"));

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

    FUnrealMCPCommonUtils::AddResolvedAssetFields(ResultObj, ResolvedPath);

    UE_LOG(LogTemp, Log, TEXT("Added %s node to Construction Script of blueprint: %s"), *NodeType, *BlueprintName);

    return ResultObj;
} 