#include "Commands/UnrealMCPUMGCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "WidgetBlueprint.h"
// We'll create widgets using regular Factory classes
#include "Factories/Factory.h"
// Remove problematic includes that don't exist in UE 5.5
// #include "UMGEditorSubsystem.h"
// #include "WidgetBlueprintFactory.h"
#include "WidgetBlueprintEditor.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "JsonObjectConverter.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/Button.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_Event.h"
#include "ScopedTransaction.h"

FUnrealMCPUMGCommands::FUnrealMCPUMGCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleCommand(const FString& CommandName, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandName == TEXT("create_umg_widget_blueprint"))
	{
		return HandleCreateUMGWidgetBlueprint(Params);
	}
	else if (CommandName == TEXT("add_text_block_to_widget"))
	{
		return HandleAddTextBlockToWidget(Params);
	}
	else if (CommandName == TEXT("add_widget_to_viewport"))
	{
		return HandleAddWidgetToViewport(Params);
	}
	else if (CommandName == TEXT("add_button_to_widget"))
	{
		return HandleAddButtonToWidget(Params);
	}
	else if (CommandName == TEXT("bind_widget_event"))
	{
		return HandleBindWidgetEvent(Params);
	}
	else if (CommandName == TEXT("set_text_block_binding"))
	{
		return HandleSetTextBlockBinding(Params);
	}

	return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown UMG command: %s"), *CommandName));
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleCreateUMGWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	// Resolve destination asset path (safe + configurable)
	FString AssetName = BlueprintName;

	FString FolderPath = FUnrealMCPCommonUtils::GetDefaultWidgetFolder();
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
		FullAssetPath = FolderPath + AssetName;
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

	// Check if asset already exists
	if (UEditorAssetLibrary::DoesAssetExist(ObjectPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' already exists"), *BlueprintName));
	}


	// Create package
	UPackage* Package = CreatePackage(*FullAssetPath);
	if (!Package)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package"));
	}

	// Transaction + Modify for stable Undo/Redo
	const FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Create UMG Widget Blueprint")));
	Package->SetFlags(RF_Transactional);
	Package->Modify();

	// Create Widget Blueprint using KismetEditorUtilities
	UBlueprint* NewBlueprint = FKismetEditorUtilities::CreateBlueprint(
		UUserWidget::StaticClass(),  // Parent class
		Package,                     // Outer package
		FName(*AssetName),           // Blueprint name
		BPTYPE_Normal,               // Blueprint type
		UBlueprint::StaticClass(),   // Blueprint class
		UBlueprintGeneratedClass::StaticClass(), // Generated class
		FName("CreateUMGWidget")     // Creation method name
	);

	// Make sure the Blueprint was created successfully
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(NewBlueprint);
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Widget Blueprint"));
	}
	WidgetBlueprint->SetFlags(RF_Transactional);
	WidgetBlueprint->Modify();

	if (WidgetBlueprint->WidgetTree)
	{
		WidgetBlueprint->WidgetTree->SetFlags(RF_Transactional);
		WidgetBlueprint->WidgetTree->Modify();
	}

	// Add a default Canvas Panel if one doesn't exist
	if (WidgetBlueprint->WidgetTree && !WidgetBlueprint->WidgetTree->RootWidget)
	{
		UCanvasPanel* RootCanvas = WidgetBlueprint->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		if (RootCanvas)
		{
			RootCanvas->SetFlags(RF_Transactional);
			RootCanvas->Modify();
			WidgetBlueprint->WidgetTree->RootWidget = RootCanvas;
		}
	}

	// Mark the package dirty and notify asset registry
	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(WidgetBlueprint);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);

	// Compile the blueprint
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	// Create success response
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("name"), BlueprintName);
	ResultObj->SetStringField(TEXT("path"), FullAssetPath); // legacy
	ResultObj->SetStringField(TEXT("object_path"), ObjectPath); // legacy
	FUnrealMCPCommonUtils::AddResolvedAssetFields(ResultObj, FullAssetPath);
	return ResultObj;

}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddTextBlockToWidget(const TSharedPtr<FJsonObject>& Params)
{
	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));
	}

	// Resolve the Widget Blueprint (path recommended; name-only is allowed if unique)
	FString BlueprintPath;
	Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath);

	FString ResolvedPath;
	TArray<FString> Candidates;
	UWidgetBlueprint* WidgetBlueprint = FUnrealMCPCommonUtils::ResolveWidgetBlueprintFromNameOrPath(BlueprintName, BlueprintPath, ResolvedPath, Candidates);
	if (!WidgetBlueprint)
	{
		FString Details;
		if (Candidates.Num() > 1)
		{
			Details = TEXT("Multiple widget blueprints matched by name. Please pass blueprint_path. Candidates:\n");
			for (const FString& C : Candidates)
			{
				Details += TEXT("- ") + C + TEXT("\n");
			}
		}
		return FUnrealMCPCommonUtils::CreateErrorResponseEx(FString::Printf(TEXT("Widget Blueprint '%s' not found or ambiguous"), *BlueprintName), TEXT("ERR_ASSET_NOT_FOUND"), Details);
	}


	// Get optional parameters
	FString InitialText = TEXT("New Text Block");
	Params->TryGetStringField(TEXT("text"), InitialText);

	FVector2D Position(0.0f, 0.0f);
	if (Params->HasField(TEXT("position")))
	{
		const TArray<TSharedPtr<FJsonValue>>* PosArray;
		if (Params->TryGetArrayField(TEXT("position"), PosArray) && PosArray->Num() >= 2)
		{
			Position.X = (*PosArray)[0]->AsNumber();
			Position.Y = (*PosArray)[1]->AsNumber();
		}
	}

	// Transaction + Modify for stable Undo/Redo
	const FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Add TextBlock to Widget")));
	WidgetBlueprint->Modify();
	if (WidgetBlueprint->WidgetTree)
	{
		WidgetBlueprint->WidgetTree->Modify();
	}

	// Create Text Block widget
	UTextBlock* TextBlock = WidgetBlueprint->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *WidgetName);
	if (!TextBlock)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Text Block widget"));
	}
	TextBlock->SetFlags(RF_Transactional);
	TextBlock->Modify();

	// Set initial text
	TextBlock->SetText(FText::FromString(InitialText));

	// Add to canvas panel
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetBlueprint->WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Root Canvas Panel not found"));
	}
	RootCanvas->Modify();

	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(TextBlock);
	if (PanelSlot)
	{
		PanelSlot->SetFlags(RF_Transactional);
		PanelSlot->Modify();
		PanelSlot->SetPosition(Position);
	}

	// Mark the package dirty and compile
	WidgetBlueprint->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	// Create success response
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("widget_name"), WidgetName);
	ResultObj->SetStringField(TEXT("text"), InitialText);
	ResultObj->SetStringField(TEXT("resolved_asset_path"), ResolvedPath);
	{
		FString ObjectPath;
		FString PathErr;
		if (!ResolvedPath.IsEmpty() && FUnrealMCPCommonUtils::MakeObjectPathFromAssetPath(ResolvedPath, ObjectPath, PathErr))
		{
			ResultObj->SetStringField(TEXT("object_path"), ObjectPath);
		}
	}
	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddWidgetToViewport(const TSharedPtr<FJsonObject>& Params)
{
	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	// Resolve the Widget Blueprint (path recommended; name-only is allowed if unique)
	FString BlueprintPath;
	Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath);

	FString ResolvedPath;
	TArray<FString> Candidates;
	UWidgetBlueprint* WidgetBlueprint = FUnrealMCPCommonUtils::ResolveWidgetBlueprintFromNameOrPath(BlueprintName, BlueprintPath, ResolvedPath, Candidates);
	if (!WidgetBlueprint)
	{
		FString Details;
		if (Candidates.Num() > 1)
		{
			Details = TEXT("Multiple widget blueprints matched by name. Please pass blueprint_path. Candidates:\n");
			for (const FString& C : Candidates)
			{
				Details += TEXT("- ") + C + TEXT("\n");
			}
		}
		return FUnrealMCPCommonUtils::CreateErrorResponseEx(FString::Printf(TEXT("Widget Blueprint '%s' not found or ambiguous"), *BlueprintName), TEXT("ERR_ASSET_NOT_FOUND"), Details);
	}


	// Get optional Z-order parameter
	int32 ZOrder = 0;
	Params->TryGetNumberField(TEXT("z_order"), ZOrder);

	// Create widget instance
	UClass* WidgetClass = WidgetBlueprint->GeneratedClass;
	if (!WidgetClass)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get widget class"));
	}

	// Note: This creates the widget but doesn't add it to viewport
	// The actual addition to viewport should be done through Blueprint nodes
	// as it requires a game context

	// Create success response with instructions
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
	ResultObj->SetStringField(TEXT("class_path"), WidgetClass->GetPathName());
	ResultObj->SetNumberField(TEXT("z_order"), ZOrder);
	ResultObj->SetStringField(TEXT("note"), TEXT("Widget class ready. Use CreateWidget and AddToViewport nodes in Blueprint to display in game."));
	FUnrealMCPCommonUtils::AddResolvedAssetFields(ResultObj, ResolvedPath);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddButtonToWidget(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), false);


	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing blueprint_name parameter"));
		return Response;
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing widget_name parameter"));
		return Response;
	}

	FString ButtonText;
	if (!Params->TryGetStringField(TEXT("text"), ButtonText))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing text parameter"));
		return Response;
	}

	// Resolve the Widget Blueprint
	FString BlueprintPath;
	Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath);

	FString ResolvedPath;
	TArray<FString> Candidates;
	UWidgetBlueprint* WidgetBlueprint = FUnrealMCPCommonUtils::ResolveWidgetBlueprintFromNameOrPath(BlueprintName, BlueprintPath, ResolvedPath, Candidates);
	if (!WidgetBlueprint)
	{
		FString Msg = FString::Printf(TEXT("Failed to load Widget Blueprint: %s"), *BlueprintName);
		if (Candidates.Num() > 1)
		{
			Msg += TEXT(" (ambiguous; pass blueprint_path)");
		}
		Response->SetStringField(TEXT("error"), Msg);
		return Response;
	}


	// Create Button widget
	UButton* Button = NewObject<UButton>(WidgetBlueprint->GeneratedClass->GetDefaultObject(), UButton::StaticClass(), *WidgetName);
	if (!Button)
	{
		Response->SetStringField(TEXT("error"), TEXT("Failed to create Button widget"));
		return Response;
	}

	// Set button text
	UTextBlock* ButtonTextBlock = NewObject<UTextBlock>(Button, UTextBlock::StaticClass(), *(WidgetName + TEXT("_Text")));
	if (ButtonTextBlock)
	{
		ButtonTextBlock->SetText(FText::FromString(ButtonText));
		Button->AddChild(ButtonTextBlock);
	}

	// Get canvas panel and add button
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetBlueprint->WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		Response->SetStringField(TEXT("error"), TEXT("Root widget is not a Canvas Panel"));
		return Response;
	}

	// Add to canvas and set position
	UCanvasPanelSlot* ButtonSlot = RootCanvas->AddChildToCanvas(Button);
	if (ButtonSlot)
	{
		const TArray<TSharedPtr<FJsonValue>>* Position;
		if (Params->TryGetArrayField(TEXT("position"), Position) && Position->Num() >= 2)
		{
			FVector2D Pos(
				(*Position)[0]->AsNumber(),
				(*Position)[1]->AsNumber()
			);
			ButtonSlot->SetPosition(Pos);
		}
	}

	// Transaction + Modify for stable Undo/Redo
	const FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Bind Widget Event")));
	WidgetBlueprint->Modify();
	if (WidgetBlueprint->WidgetTree)
	{
		WidgetBlueprint->WidgetTree->Modify();
	}

	// Save the Widget Blueprint
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	WidgetBlueprint->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	WidgetBlueprint->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	WidgetBlueprint->MarkPackageDirty();

	// NOTE: BlueprintPath may be empty when the caller resolves by name only.
	// Prefer saving via the resolved asset path.
	{
		FString SaveObjectPath;
		FString SaveErr;
		if (!ResolvedPath.IsEmpty() && FUnrealMCPCommonUtils::MakeObjectPathFromAssetPath(ResolvedPath, SaveObjectPath, SaveErr))
		{
			UEditorAssetLibrary::SaveAsset(SaveObjectPath, false);
		}
		else if (!BlueprintPath.IsEmpty())
		{
			UEditorAssetLibrary::SaveAsset(BlueprintPath, false);
		}
	}

	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("widget_name"), WidgetName);
	Response->SetStringField(TEXT("resolved_asset_path"), ResolvedPath);
	{
		FString ObjectPath;
		FString PathErr;
		if (!ResolvedPath.IsEmpty() && FUnrealMCPCommonUtils::MakeObjectPathFromAssetPath(ResolvedPath, ObjectPath, PathErr))
		{
			Response->SetStringField(TEXT("object_path"), ObjectPath);
		}
	}
	return Response;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleBindWidgetEvent(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), false);


	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing blueprint_name parameter"));
		return Response;
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing widget_name parameter"));
		return Response;
	}

	FString EventName;
	if (!Params->TryGetStringField(TEXT("event_name"), EventName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing event_name parameter"));
		return Response;
	}

	// Resolve the Widget Blueprint
	FString BlueprintPath;
	Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath);

	FString ResolvedPath;
	TArray<FString> Candidates;
	UWidgetBlueprint* WidgetBlueprint = FUnrealMCPCommonUtils::ResolveWidgetBlueprintFromNameOrPath(BlueprintName, BlueprintPath, ResolvedPath, Candidates);
	if (!WidgetBlueprint)
	{
		FString Msg = FString::Printf(TEXT("Failed to load Widget Blueprint: %s"), *BlueprintName);
		if (Candidates.Num() > 1)
		{
			Msg += TEXT(" (ambiguous; pass blueprint_path)");
		}
		Response->SetStringField(TEXT("error"), Msg);
		return Response;
	}


	// Create the event graph if it doesn't exist
	UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(WidgetBlueprint);
	if (!EventGraph)
	{
		Response->SetStringField(TEXT("error"), TEXT("Failed to find or create event graph"));
		return Response;
	}

	// Find the widget in the blueprint
	UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(*WidgetName);
	if (!Widget)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to find widget: %s"), *WidgetName));
		return Response;
	}

	// Create the event node (e.g., OnClicked for buttons)
	UK2Node_Event* EventNode = nullptr;
	
	// Find existing nodes first
	TArray<UK2Node_Event*> AllEventNodes;
	FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Event>(WidgetBlueprint, AllEventNodes);
	
	for (UK2Node_Event* Node : AllEventNodes)
	{
		if (Node->CustomFunctionName == FName(*EventName) && Node->EventReference.GetMemberParentClass() == Widget->GetClass())
		{
			EventNode = Node;
			break;
		}
	}

	// If no existing node, create a new one
	if (!EventNode)
	{
		// Calculate position - place it below existing nodes
		float MaxHeight = 0.0f;
		for (UEdGraphNode* Node : EventGraph->Nodes)
		{
			MaxHeight = FMath::Max(MaxHeight, Node->NodePosY);
		}
		
		const FVector2D NodePos(200, MaxHeight + 200);

		// Call CreateNewBoundEventForClass, which returns void, so we can't capture the return value directly
		// We'll need to find the node after creating it
		FKismetEditorUtilities::CreateNewBoundEventForClass(
			Widget->GetClass(),
			FName(*EventName),
			WidgetBlueprint,
			nullptr  // We don't need a specific property binding
		);

		// Now find the newly created node
		TArray<UK2Node_Event*> UpdatedEventNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Event>(WidgetBlueprint, UpdatedEventNodes);
		
		for (UK2Node_Event* Node : UpdatedEventNodes)
		{
			if (Node->CustomFunctionName == FName(*EventName) && Node->EventReference.GetMemberParentClass() == Widget->GetClass())
			{
				EventNode = Node;
				
				// Set position of the node
				EventNode->NodePosX = NodePos.X;
				EventNode->NodePosY = NodePos.Y;
				
				break;
			}
		}
	}

	if (!EventNode)
	{
		Response->SetStringField(TEXT("error"), TEXT("Failed to create event node"));
		return Response;
	}

	// Transaction + Modify for stable Undo/Redo
	const FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Bind Widget Event")));
	WidgetBlueprint->Modify();
	if (WidgetBlueprint->WidgetTree)
	{
		WidgetBlueprint->WidgetTree->Modify();
	}

	// Save the Widget Blueprint
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	WidgetBlueprint->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	WidgetBlueprint->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	WidgetBlueprint->MarkPackageDirty();

	{
		FString SaveObjectPath;
		FString SaveErr;
		if (!ResolvedPath.IsEmpty() && FUnrealMCPCommonUtils::MakeObjectPathFromAssetPath(ResolvedPath, SaveObjectPath, SaveErr))
		{
			UEditorAssetLibrary::SaveAsset(SaveObjectPath, false);
		}
		else if (!BlueprintPath.IsEmpty())
		{
			UEditorAssetLibrary::SaveAsset(BlueprintPath, false);
		}
	}

	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("event_name"), EventName);
	Response->SetStringField(TEXT("resolved_asset_path"), ResolvedPath);
	{
		FString ObjectPath;
		FString PathErr;
		if (!ResolvedPath.IsEmpty() && FUnrealMCPCommonUtils::MakeObjectPathFromAssetPath(ResolvedPath, ObjectPath, PathErr))
		{
			Response->SetStringField(TEXT("object_path"), ObjectPath);
		}
	}
	return Response;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleSetTextBlockBinding(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), false);


	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing blueprint_name parameter"));
		return Response;
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing widget_name parameter"));
		return Response;
	}

	FString BindingName;
	if (!Params->TryGetStringField(TEXT("binding_name"), BindingName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing binding_name parameter"));
		return Response;
	}

	// Resolve the Widget Blueprint
	FString BlueprintPath;
	Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath);

	FString ResolvedPath;
	TArray<FString> Candidates;
	UWidgetBlueprint* WidgetBlueprint = FUnrealMCPCommonUtils::ResolveWidgetBlueprintFromNameOrPath(BlueprintName, BlueprintPath, ResolvedPath, Candidates);
	if (!WidgetBlueprint)
	{
		FString Msg = FString::Printf(TEXT("Failed to load Widget Blueprint: %s"), *BlueprintName);
		if (Candidates.Num() > 1)
		{
			Msg += TEXT(" (ambiguous; pass blueprint_path)");
		}
		Response->SetStringField(TEXT("error"), Msg);
		return Response;
	}


	// Transaction + Modify for stable Undo/Redo
	const FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set TextBlock Binding")));
	WidgetBlueprint->Modify();
	if (WidgetBlueprint->WidgetTree)
	{
		WidgetBlueprint->WidgetTree->Modify();
	}

	// Create a variable for binding if it doesn't exist
	FBlueprintEditorUtils::AddMemberVariable(
		WidgetBlueprint,
		FName(*BindingName),
		FEdGraphPinType(UEdGraphSchema_K2::PC_Text, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType())
	);

	// Find the TextBlock widget
	UTextBlock* TextBlock = Cast<UTextBlock>(WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName)));
	if (!TextBlock)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to find TextBlock widget: %s"), *WidgetName));
		return Response;
	}

	// Create binding function
	const FString FunctionName = FString::Printf(TEXT("Get%s"), *BindingName);
	UEdGraph* FuncGraph = FBlueprintEditorUtils::CreateNewGraph(
		WidgetBlueprint,
		FName(*FunctionName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);

	if (FuncGraph)
	{
		// Add the function to the blueprint with proper template parameter
		// Template requires null for last parameter when not using a signature-source
		FBlueprintEditorUtils::AddFunctionGraph<UClass>(WidgetBlueprint, FuncGraph, false, nullptr);

		// Create entry node
		UK2Node_FunctionEntry* EntryNode = nullptr;
		
		// Create entry node - use the API that exists in UE 5.5
		EntryNode = NewObject<UK2Node_FunctionEntry>(FuncGraph);
		FuncGraph->AddNode(EntryNode, false, false);
		EntryNode->NodePosX = 0;
		EntryNode->NodePosY = 0;
		EntryNode->FunctionReference.SetExternalMember(FName(*FunctionName), WidgetBlueprint->GeneratedClass);
		EntryNode->AllocateDefaultPins();

		// Create get variable node
		UK2Node_VariableGet* GetVarNode = NewObject<UK2Node_VariableGet>(FuncGraph);
		GetVarNode->VariableReference.SetSelfMember(FName(*BindingName));
		FuncGraph->AddNode(GetVarNode, false, false);
		GetVarNode->NodePosX = 200;
		GetVarNode->NodePosY = 0;
		GetVarNode->AllocateDefaultPins();

		// Connect nodes
		UEdGraphPin* EntryThenPin = EntryNode->FindPin(UEdGraphSchema_K2::PN_Then);
		UEdGraphPin* GetVarOutPin = GetVarNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue);
		if (EntryThenPin && GetVarOutPin)
		{
			EntryThenPin->MakeLinkTo(GetVarOutPin);
		}
	}

	// Save the Widget Blueprint
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	WidgetBlueprint->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	WidgetBlueprint->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	WidgetBlueprint->MarkPackageDirty();

	{
		FString SaveObjectPath;
		FString SaveErr;
		if (!ResolvedPath.IsEmpty() && FUnrealMCPCommonUtils::MakeObjectPathFromAssetPath(ResolvedPath, SaveObjectPath, SaveErr))
		{
			UEditorAssetLibrary::SaveAsset(SaveObjectPath, false);
		}
		else if (!BlueprintPath.IsEmpty())
		{
			UEditorAssetLibrary::SaveAsset(BlueprintPath, false);
		}
	}

	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("binding_name"), BindingName);
	Response->SetStringField(TEXT("resolved_asset_path"), ResolvedPath);
	{
		FString ObjectPath;
		FString PathErr;
		if (!ResolvedPath.IsEmpty() && FUnrealMCPCommonUtils::MakeObjectPathFromAssetPath(ResolvedPath, ObjectPath, PathErr))
		{
			Response->SetStringField(TEXT("object_path"), ObjectPath);
		}
	}
	return Response;
} 