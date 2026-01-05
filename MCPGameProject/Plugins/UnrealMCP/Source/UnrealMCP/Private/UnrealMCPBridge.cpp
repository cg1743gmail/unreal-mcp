#include "UnrealMCPBridge.h"
#include "MCPServerRunnable.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "HAL/RunnableThread.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/StaticMeshActor.h"

#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "JsonObjectConverter.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
// Add Blueprint related includes
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
// UE5.5 correct includes
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
// Blueprint Graph specific includes
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CallFunction.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "GameFramework/InputSettings.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
// Include our new command handler classes
#include "Commands/UnrealMCPEditorCommands.h"
#include "Commands/UnrealMCPBlueprintCommands.h"
#include "Commands/UnrealMCPBlueprintNodeCommands.h"
#include "Commands/UnrealMCPProjectCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Commands/UnrealMCPUMGCommands.h"
#include "Commands/UnrealMCPInterchangeCommands.h"

// Default settings
#define MCP_SERVER_HOST "127.0.0.1"
#define MCP_SERVER_PORT 55557

UUnrealMCPBridge::UUnrealMCPBridge()
{
    EditorCommands = MakeShared<FUnrealMCPEditorCommands>();
    BlueprintCommands = MakeShared<FUnrealMCPBlueprintCommands>();
    BlueprintNodeCommands = MakeShared<FUnrealMCPBlueprintNodeCommands>();
    ProjectCommands = MakeShared<FUnrealMCPProjectCommands>();
    UMGCommands = MakeShared<FUnrealMCPUMGCommands>();
    InterchangeCommands = MakeShared<FUnrealMCPInterchangeCommands>();
}

UUnrealMCPBridge::~UUnrealMCPBridge()
{
    // Make sure server is stopped even if Deinitialize was not called.
    StopServer();

    EditorCommands.Reset();
    BlueprintCommands.Reset();
    BlueprintNodeCommands.Reset();
    ProjectCommands.Reset();
    UMGCommands.Reset();
    InterchangeCommands.Reset();
}

// Initialize subsystem
void UUnrealMCPBridge::Initialize(FSubsystemCollectionBase& Collection)
{
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Initializing"));
    
    bIsRunning = false;
    ListenerSocket = nullptr;
    ConnectionSocket = nullptr;
    ServerThread = nullptr;
    ServerRunnable = nullptr;
    Port = MCP_SERVER_PORT;
    FIPv4Address::Parse(MCP_SERVER_HOST, ServerAddress);

    // Start the server automatically
    StartServer();
}

// Clean up resources when subsystem is destroyed
void UUnrealMCPBridge::Deinitialize()
{
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Shutting down"));
    StopServer();
}

// Start the MCP server
void UUnrealMCPBridge::StartServer()
{
    if (bIsRunning)
    {
        UE_LOG(LogTemp, Warning, TEXT("UnrealMCPBridge: Server is already running"));
        return;
    }

    // Create socket subsystem
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to get socket subsystem"));
        return;
    }

    // Create listener socket
    FSocket* NewListenerSocket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("UnrealMCPListener"), false);
    if (!NewListenerSocket)
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to create listener socket"));
        return;
    }

    // Allow address reuse for quick restarts
    NewListenerSocket->SetReuseAddr(true);
    NewListenerSocket->SetNonBlocking(true);

    // Bind to address
    FIPv4Endpoint Endpoint(ServerAddress, Port);
    if (!NewListenerSocket->Bind(*Endpoint.ToInternetAddr()))
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to bind listener socket to %s:%d"), *ServerAddress.ToString(), Port);
        SocketSubsystem->DestroySocket(NewListenerSocket);
        return;
    }

    // Start listening
    if (!NewListenerSocket->Listen(5))
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to start listening"));
        SocketSubsystem->DestroySocket(NewListenerSocket);
        return;
    }

    ListenerSocket = NewListenerSocket;
    bIsRunning = true;
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Server started on %s:%d"), *ServerAddress.ToString(), Port);

    // Start server thread (keep runnable pointer to manage lifecycle)
    ServerRunnable = new FMCPServerRunnable(this, ListenerSocket);
    ServerThread = FRunnableThread::Create(
        ServerRunnable,
        TEXT("UnrealMCPServerThread"),
        0, TPri_Normal
    );


    if (!ServerThread)
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to create server thread"));

        // Avoid leaking runnable when thread creation fails.
        delete ServerRunnable;
        ServerRunnable = nullptr;

        StopServer();
        return;
    }
}

// Stop the MCP server
void UUnrealMCPBridge::StopServer()
{
    if (!bIsRunning)
    {
        return;
    }

    bIsRunning = false;

    // Clean up thread
    if (ServerRunnable)
    {
        ServerRunnable->Stop();
    }

    if (ServerThread)
    {
        // Wait for Run() loop to exit before destroying sockets.
        ServerThread->WaitForCompletion();
        delete ServerThread;
        ServerThread = nullptr;
    }

    if (ServerRunnable)
    {
        delete ServerRunnable;
        ServerRunnable = nullptr;
    }


    // Close sockets (FSocket must be destroyed via ISocketSubsystem, never delete)
    if (ConnectionSocket)
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ConnectionSocket);
        ConnectionSocket = nullptr;
    }

    if (ListenerSocket)
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket);
        ListenerSocket = nullptr;
    }

    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Server stopped"));
}

// Execute a command received from a client
FString UUnrealMCPBridge::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    FString McpRequestId;
    FString McpTraceId;
    FString McpToken;

    if (Params.IsValid() && Params->HasField(TEXT("_mcp")))
    {
        const TSharedPtr<FJsonObject> McpObj = Params->GetObjectField(TEXT("_mcp"));
        if (McpObj.IsValid())
        {
            McpObj->TryGetStringField(TEXT("request_id"), McpRequestId);
            McpObj->TryGetStringField(TEXT("trace_id"), McpTraceId);
            McpObj->TryGetStringField(TEXT("token"), McpToken);
        }
    }

    if (!McpRequestId.IsEmpty())
    {
        UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge[%s]: Executing command: %s"), *McpRequestId, *CommandType);
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Executing command: %s"), *CommandType);
    }


    // Create a promise to wait for the result
    TPromise<FString> Promise;
    TFuture<FString> Future = Promise.GetFuture();

    // Queue execution on Game Thread
    AsyncTask(ENamedThreads::GameThread, [this, CommandType, Params, McpRequestId, McpTraceId, McpToken, Promise = MoveTemp(Promise)]() mutable
    {
        TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject);

        auto SetStructuredError = [&](const FString& Code, const FString& Message, const FString& Details)
        {
            // UE-2: consistent, machine-friendly top-level success flag
            ResponseJson->SetBoolField(TEXT("success"), false);
            ResponseJson->SetStringField(TEXT("status"), TEXT("error"));

            // Backward-compatible string field
            ResponseJson->SetStringField(TEXT("error"), Message);

            // Structured fields
            ResponseJson->SetStringField(TEXT("error_code"), Code);
            if (!Details.IsEmpty())
            {
                ResponseJson->SetStringField(TEXT("error_details"), Details);
            }

            TSharedPtr<FJsonObject> ErrorInfo = MakeShareable(new FJsonObject);
            ErrorInfo->SetStringField(TEXT("message"), Message);
            ErrorInfo->SetStringField(TEXT("code"), Code);
            if (!Details.IsEmpty())
            {
                ErrorInfo->SetStringField(TEXT("details"), Details);
            }
            ResponseJson->SetObjectField(TEXT("error_info"), ErrorInfo);
        };

        auto Dispatch = [&](const FString& InCommandType, const TSharedPtr<FJsonObject>& InParams) -> TSharedPtr<FJsonObject>
        {
            if (InCommandType == TEXT("ping"))
            {
                TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
                Obj->SetStringField(TEXT("message"), TEXT("pong"));
                return Obj;
            }

            // Editor Commands (including actor manipulation)
            if (InCommandType == TEXT("get_actors_in_level") ||
                InCommandType == TEXT("find_actors_by_name") ||
                InCommandType == TEXT("spawn_actor") ||
                InCommandType == TEXT("create_actor") ||
                InCommandType == TEXT("delete_actor") ||
                InCommandType == TEXT("set_actor_transform") ||
                InCommandType == TEXT("get_actor_properties") ||
                InCommandType == TEXT("set_actor_property") ||
                InCommandType == TEXT("spawn_blueprint_actor") ||
                InCommandType == TEXT("focus_viewport") ||
                InCommandType == TEXT("take_screenshot"))
            {
                return EditorCommands->HandleCommand(InCommandType, InParams);
            }

            // Blueprint Commands
            if (InCommandType == TEXT("create_blueprint") ||
                InCommandType == TEXT("add_component_to_blueprint") ||
                InCommandType == TEXT("set_component_property") ||
                InCommandType == TEXT("set_physics_properties") ||
                InCommandType == TEXT("compile_blueprint") ||
                InCommandType == TEXT("set_blueprint_property") ||
                InCommandType == TEXT("set_static_mesh_properties") ||
                InCommandType == TEXT("set_pawn_properties") ||
                InCommandType == TEXT("list_blueprint_components") ||
                InCommandType == TEXT("get_component_property") ||
                InCommandType == TEXT("get_blueprint_property"))

            {
                return BlueprintCommands->HandleCommand(InCommandType, InParams);
            }

            // Blueprint Node Commands
            if (InCommandType == TEXT("connect_blueprint_nodes") ||
                InCommandType == TEXT("add_blueprint_get_self_component_reference") ||
                InCommandType == TEXT("add_blueprint_self_reference") ||
                InCommandType == TEXT("find_blueprint_nodes") ||
                InCommandType == TEXT("add_blueprint_event_node") ||
                InCommandType == TEXT("add_blueprint_input_action_node") ||
                InCommandType == TEXT("add_blueprint_function_node") ||
                InCommandType == TEXT("add_blueprint_get_component_node") ||
                InCommandType == TEXT("add_blueprint_variable"))
            {
                return BlueprintNodeCommands->HandleCommand(InCommandType, InParams);
            }

            // Project Commands
            if (InCommandType == TEXT("create_input_mapping"))
            {
                return ProjectCommands->HandleCommand(InCommandType, InParams);
            }

            // UMG Commands
            if (InCommandType == TEXT("create_umg_widget_blueprint") ||
                InCommandType == TEXT("add_text_block_to_widget") ||
                InCommandType == TEXT("add_button_to_widget") ||
                InCommandType == TEXT("bind_widget_event") ||
                InCommandType == TEXT("set_text_block_binding") ||
                InCommandType == TEXT("add_widget_to_viewport"))
            {
                return UMGCommands->HandleCommand(InCommandType, InParams);
            }

            // Interchange Commands (UE 5.6+)
            if (InCommandType == TEXT("import_model") ||
                InCommandType == TEXT("create_interchange_blueprint") ||
                InCommandType == TEXT("create_custom_interchange_blueprint") ||
                InCommandType == TEXT("get_interchange_assets") ||
                InCommandType == TEXT("reimport_asset") ||
                InCommandType == TEXT("get_interchange_info") ||
                InCommandType == TEXT("create_interchange_pipeline_blueprint") ||
                InCommandType == TEXT("get_interchange_pipelines") ||
                InCommandType == TEXT("configure_interchange_pipeline") ||
                InCommandType == TEXT("get_interchange_pipeline_graph") ||
                InCommandType == TEXT("add_interchange_pipeline_function_override") ||
                InCommandType == TEXT("add_interchange_pipeline_node") ||
                InCommandType == TEXT("connect_interchange_pipeline_nodes") ||
                InCommandType == TEXT("find_interchange_pipeline_nodes") ||
                InCommandType == TEXT("add_interchange_iterate_nodes_block") ||
                InCommandType == TEXT("compile_interchange_pipeline"))
            {
                return InterchangeCommands->HandleCommand(InCommandType, InParams);
            }

            return FUnrealMCPCommonUtils::CreateErrorResponseEx(
                FString::Printf(TEXT("Unknown command: %s"), *InCommandType),
                TEXT("ERR_UNKNOWN_COMMAND"),
                TEXT(""));
        };

        auto ExtractError = [&](const TSharedPtr<FJsonObject>& ResultObj, FString& OutMsg, FString& OutCode, FString& OutDetails)
        {
            OutMsg = ResultObj.IsValid() && ResultObj->HasField(TEXT("error")) ? ResultObj->GetStringField(TEXT("error")) : TEXT("Unknown error");
            OutCode = ResultObj.IsValid() && ResultObj->HasField(TEXT("error_code")) ? ResultObj->GetStringField(TEXT("error_code")) : TEXT("ERR_GENERIC");
            OutDetails = ResultObj.IsValid() && ResultObj->HasField(TEXT("error_details")) ? ResultObj->GetStringField(TEXT("error_details")) : TEXT("");
        };

        try
        {
            // Security gate: editor-only
            if (!GIsEditor)
            {
                SetStructuredError(TEXT("ERR_EDITOR_ONLY"), TEXT("UnrealMCP commands require Editor context"), TEXT(""));
                FString ResultString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
                FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
                Promise.SetValue(ResultString);
                return;
            }

            // Security gate: optional token enforcement
            FString RequiredToken;
            if (GConfig)
            {
                GConfig->GetString(TEXT("UnrealMCP"), TEXT("SecurityToken"), RequiredToken, GEngineIni);
            }
            if (!RequiredToken.IsEmpty() && McpToken != RequiredToken)
            {
                SetStructuredError(TEXT("ERR_UNAUTHORIZED"), TEXT("Unauthorized"), TEXT("Missing or invalid SecurityToken"));
                FString ResultString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
                FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
                Promise.SetValue(ResultString);
                return;
            }

            // Security gate: read-only mode (best-effort classification)
            bool bReadOnly = false;
            if (GConfig)
            {
                GConfig->GetBool(TEXT("UnrealMCP"), TEXT("bReadOnly"), bReadOnly, GEngineIni);
            }
            auto IsWriteCommand = [](const FString& InType) -> bool
            {
                return InType.StartsWith(TEXT("create_")) ||
                    InType.StartsWith(TEXT("add_")) ||
                    InType.StartsWith(TEXT("set_")) ||
                    InType.StartsWith(TEXT("delete_")) ||
                    InType.StartsWith(TEXT("spawn_")) ||
                    InType.StartsWith(TEXT("import_")) ||
                    InType.StartsWith(TEXT("reimport_")) ||
                    (InType == TEXT("batch"));
            };
            if (bReadOnly && IsWriteCommand(CommandType))
            {
                SetStructuredError(TEXT("ERR_READ_ONLY"), TEXT("Server is in read-only mode"), TEXT("Disable [UnrealMCP] bReadOnly or run against an allowed editor session"));
                FString ResultString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
                FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
                Promise.SetValue(ResultString);
                return;
            }

            // UE-3: batch execution
            if (CommandType == TEXT("batch"))
            {

                bool bStopOnError = true;
                if (Params.IsValid() && Params->HasField(TEXT("stop_on_error")))
                {
                    bStopOnError = Params->GetBoolField(TEXT("stop_on_error"));
                }

                const TArray<TSharedPtr<FJsonValue>>* CommandsArray = nullptr;
                if (!Params.IsValid() || !Params->TryGetArrayField(TEXT("commands"), CommandsArray) || !CommandsArray)
                {
                    SetStructuredError(TEXT("ERR_BAD_REQUEST"), TEXT("Missing 'commands' array"), TEXT("batch expects params.commands: [{type, params}]"));
                }
                else
                {
                    TArray<TSharedPtr<FJsonValue>> Items;
                    int32 OkCount = 0;
                    int32 ErrCount = 0;

                    for (int32 Index = 0; Index < CommandsArray->Num(); ++Index)
                    {
                        const TSharedPtr<FJsonValue>& V = (*CommandsArray)[Index];
                        TSharedPtr<FJsonObject> CmdObj = V.IsValid() ? V->AsObject() : nullptr;
                        FString SubType;
                        TSharedPtr<FJsonObject> SubParams = MakeShareable(new FJsonObject);

                        if (!CmdObj.IsValid() || !CmdObj->TryGetStringField(TEXT("type"), SubType))
                        {
                            ++ErrCount;
                            TSharedPtr<FJsonObject> Item = MakeShareable(new FJsonObject);
                            Item->SetNumberField(TEXT("index"), Index);
                            Item->SetBoolField(TEXT("success"), false);
                            Item->SetStringField(TEXT("error"), TEXT("Missing command.type"));
                            Item->SetStringField(TEXT("error_code"), TEXT("ERR_BAD_REQUEST"));
                            Items.Add(MakeShareable(new FJsonValueObject(Item)));
                            if (bStopOnError) break;
                            continue;
                        }

                        if (CmdObj->HasField(TEXT("params")))
                        {
                            TSharedPtr<FJsonValue> PV = CmdObj->TryGetField(TEXT("params"));
                            if (PV.IsValid() && PV->Type == EJson::Object)
                            {
                                SubParams = PV->AsObject();
                            }
                        }

                        TSharedPtr<FJsonObject> SubResult = Dispatch(SubType, SubParams);
                        bool bSubSuccess = true;
                        if (SubResult.IsValid() && SubResult->HasField(TEXT("success")))
                        {
                            bSubSuccess = SubResult->GetBoolField(TEXT("success"));
                        }

                        TSharedPtr<FJsonObject> Item = MakeShareable(new FJsonObject);
                        Item->SetNumberField(TEXT("index"), Index);
                        Item->SetStringField(TEXT("type"), SubType);
                        Item->SetBoolField(TEXT("success"), bSubSuccess);

                        if (bSubSuccess)
                        {
                            ++OkCount;
                            Item->SetObjectField(TEXT("result"), SubResult);
                        }
                        else
                        {
                            ++ErrCount;
                            FString ErrMsg, ErrCode, ErrDetails;
                            ExtractError(SubResult, ErrMsg, ErrCode, ErrDetails);
                            Item->SetStringField(TEXT("error"), ErrMsg);
                            Item->SetStringField(TEXT("error_code"), ErrCode);
                            if (!ErrDetails.IsEmpty())
                            {
                                Item->SetStringField(TEXT("error_details"), ErrDetails);
                            }
                            if (SubResult.IsValid() && SubResult->HasField(TEXT("error_info")))
                            {
                                Item->SetObjectField(TEXT("error_info"), SubResult->GetObjectField(TEXT("error_info")));
                            }

                            if (bStopOnError) 
                            {
                                Items.Add(MakeShareable(new FJsonValueObject(Item)));
                                break;
                            }
                        }

                        Items.Add(MakeShareable(new FJsonValueObject(Item)));
                    }

                    TSharedPtr<FJsonObject> Summary = MakeShareable(new FJsonObject);
                    Summary->SetNumberField(TEXT("total"), CommandsArray->Num());
                    Summary->SetNumberField(TEXT("ok"), OkCount);
                    Summary->SetNumberField(TEXT("error"), ErrCount);
                    Summary->SetBoolField(TEXT("stop_on_error"), bStopOnError);

                    TSharedPtr<FJsonObject> BatchResult = MakeShareable(new FJsonObject);
                    BatchResult->SetArrayField(TEXT("items"), Items);
                    BatchResult->SetObjectField(TEXT("summary"), Summary);

                    ResponseJson->SetObjectField(TEXT("result"), BatchResult);
                    const bool bBatchSuccess = (ErrCount == 0);
                    ResponseJson->SetBoolField(TEXT("success"), bBatchSuccess);
                    ResponseJson->SetStringField(TEXT("status"), bBatchSuccess ? TEXT("success") : TEXT("error"));



                    if (!bBatchSuccess)
                    {
                        ResponseJson->SetStringField(TEXT("error"), TEXT("Batch contains error(s)"));
                        ResponseJson->SetStringField(TEXT("error_code"), TEXT("ERR_BATCH"));
                    }
                }
            }
            else
            {
                TSharedPtr<FJsonObject> ResultJson = Dispatch(CommandType, Params);

                bool bSuccess = true;
                if (ResultJson.IsValid() && ResultJson->HasField(TEXT("success")))
                {
                    bSuccess = ResultJson->GetBoolField(TEXT("success"));
                }

                if (bSuccess)
                {
                    ResponseJson->SetBoolField(TEXT("success"), true);
                    ResponseJson->SetStringField(TEXT("status"), TEXT("success"));
                    ResponseJson->SetObjectField(TEXT("result"), ResultJson);
                }
                else
                {
                    FString ErrMsg, ErrCode, ErrDetails;
                    ExtractError(ResultJson, ErrMsg, ErrCode, ErrDetails);
                    SetStructuredError(ErrCode, ErrMsg, ErrDetails);
                }



            }
        }
        catch (const std::exception& e)
        {
            SetStructuredError(TEXT("ERR_EXCEPTION"), UTF8_TO_TCHAR(e.what()), TEXT("std::exception"));
        }

        FString ResultString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
        FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
        Promise.SetValue(ResultString);
    });

    return Future.Get();
}
