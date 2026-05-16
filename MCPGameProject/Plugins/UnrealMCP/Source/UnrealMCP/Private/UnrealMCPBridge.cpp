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
#include "Commands/UnrealMCPMaterialCommands.h"
#include "Commands/UnrealMCPAudioCommands.h"
#include "Commands/UnrealMCPInputCommands.h"

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
    MaterialCommands = MakeShared<FUnrealMCPMaterialCommands>();
    AudioCommands = MakeShared<FUnrealMCPAudioCommands>();
    InputCommands = MakeShared<FUnrealMCPInputCommands>();
    AssetCommands = MakeShared<FUnrealMCPAssetCommands>();
    ActorQueryCommands = MakeShared<FUnrealMCPActorQueryCommands>();
    LevelCommands = MakeShared<FUnrealMCPLevelCommands>();
    AnimationCommands = MakeShared<FUnrealMCPAnimationCommands>();
    NiagaraCommands = MakeShared<FUnrealMCPNiagaraCommands>();
    WorldCommands = MakeShared<FUnrealMCPWorldCommands>();
    DataAssetCommands = MakeShared<FUnrealMCPDataAssetCommands>();
    AnimGraphCommands = MakeShared<FUnrealMCPAnimGraphCommands>();
    ChooserCommands = MakeShared<FUnrealMCPChooserCommands>();
}

UUnrealMCPBridge::~UUnrealMCPBridge()
{
    EditorCommands.Reset();
    BlueprintCommands.Reset();
    BlueprintNodeCommands.Reset();
    ProjectCommands.Reset();
    UMGCommands.Reset();
    MaterialCommands.Reset();
    AudioCommands.Reset();
    InputCommands.Reset();
    AssetCommands.Reset();
    ActorQueryCommands.Reset();
    LevelCommands.Reset();
    AnimationCommands.Reset();
    NiagaraCommands.Reset();
    WorldCommands.Reset();
    ChooserCommands.Reset();
}

// Initialize subsystem
void UUnrealMCPBridge::Initialize(FSubsystemCollectionBase& Collection)
{
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Initializing"));
    
    bIsRunning = false;
    ListenerSocket = nullptr;
    ConnectionSocket = nullptr;
    ServerThread = nullptr;
    Port = MCP_SERVER_PORT;
    FString PortArg;
    if (FParse::Value(FCommandLine::Get(), TEXT("-MCPPort="), PortArg))
    {
        Port = FCString::Atoi(*PortArg);
        UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Using command-line port %d"), Port);
    }
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
    TSharedPtr<FSocket> NewListenerSocket = MakeShareable(SocketSubsystem->CreateSocket(NAME_Stream, TEXT("UnrealMCPListener"), false));
    if (!NewListenerSocket.IsValid())
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
        return;
    }

    // Start listening
    if (!NewListenerSocket->Listen(5))
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to start listening"));
        return;
    }

    ListenerSocket = NewListenerSocket;
    bIsRunning = true;
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Server started on %s:%d"), *ServerAddress.ToString(), Port);

    // Start server thread
    ServerThread = FRunnableThread::Create(
        new FMCPServerRunnable(this, ListenerSocket),
        TEXT("UnrealMCPServerThread"),
        0, TPri_Normal
    );

    if (!ServerThread)
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to create server thread"));
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
    if (ServerThread)
    {
        ServerThread->Kill(true);
        delete ServerThread;
        ServerThread = nullptr;
    }

    // Close sockets
    if (ConnectionSocket.IsValid())
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ConnectionSocket.Get());
        ConnectionSocket.Reset();
    }

    if (ListenerSocket.IsValid())
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket.Get());
        ListenerSocket.Reset();
    }

    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Server stopped"));
}

// Execute a command received from a client
FString UUnrealMCPBridge::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Executing command: %s"), *CommandType);

    // Create a promise to wait for the result
    TPromise<FString> Promise;
    TFuture<FString> Future = Promise.GetFuture();

    double QueuedTime = FPlatformTime::Seconds();

    // Queue execution on Game Thread
    AsyncTask(ENamedThreads::GameThread, [this, CommandType, Params, Promise = MoveTemp(Promise), QueuedTime]() mutable
    {
        double PickupTime = FPlatformTime::Seconds();
        UE_LOG(LogTemp, Display, TEXT("MCP: Game thread picked up '%s' after %.2fs wait"), *CommandType, PickupTime - QueuedTime);

        TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject);

        try
        {
            TSharedPtr<FJsonObject> ResultJson;
            
            if (CommandType == TEXT("ping"))
            {
                ResultJson = MakeShareable(new FJsonObject);
                ResultJson->SetStringField(TEXT("message"), TEXT("pong"));
            }
            else if (CommandType == TEXT("get_project_info"))
            {
                ResultJson = MakeShareable(new FJsonObject);
                ResultJson->SetStringField(TEXT("project_name"), FApp::GetProjectName());
                ResultJson->SetStringField(TEXT("project_path"), FPaths::GetProjectFilePath());
                ResultJson->SetNumberField(TEXT("port"), Port);
                ResultJson->SetBoolField(TEXT("success"), true);
            }
            // Editor Commands (including actor manipulation)
            else if (CommandType == TEXT("get_actors_in_level") || 
                     CommandType == TEXT("find_actors_by_name") ||
                     CommandType == TEXT("spawn_actor") ||
                     CommandType == TEXT("create_actor") ||
                     CommandType == TEXT("delete_actor") || 
                     CommandType == TEXT("set_actor_transform") ||
                     CommandType == TEXT("get_actor_properties") ||
                     CommandType == TEXT("set_actor_property") ||
                     CommandType == TEXT("spawn_blueprint_actor") ||
                     CommandType == TEXT("focus_viewport") ||
                     CommandType == TEXT("take_screenshot") ||
                     CommandType == TEXT("start_pie") ||
                     CommandType == TEXT("stop_pie") ||
                     CommandType == TEXT("get_pie_state") ||
                     CommandType == TEXT("pie_drive_input_start") ||
                     CommandType == TEXT("pie_simulate_key_start") ||
                     CommandType == TEXT("pie_get_job_result") ||
                     CommandType == TEXT("pie_set_control_rotation") ||
                     CommandType == TEXT("pie_cancel_job") ||
                     CommandType == TEXT("execute_console_command") ||
                     CommandType == TEXT("get_editor_log") ||
                     CommandType == TEXT("execute_python") ||
                     CommandType == TEXT("close_editor"))
            {
                ResultJson = EditorCommands->HandleCommand(CommandType, Params);
            }
            // Blueprint Commands
            else if (CommandType == TEXT("create_blueprint") || 
                     CommandType == TEXT("add_component_to_blueprint") || 
                     CommandType == TEXT("set_component_property") || 
                     CommandType == TEXT("set_physics_properties") || 
                     CommandType == TEXT("compile_blueprint") || 
                     CommandType == TEXT("set_blueprint_property") || 
                     CommandType == TEXT("set_static_mesh_properties") ||
                     CommandType == TEXT("set_pawn_properties") ||
                     CommandType == TEXT("list_blueprint_components") ||
                     CommandType == TEXT("get_blueprint_component_properties") ||
                     CommandType == TEXT("remove_blueprint_component") ||
                     CommandType == TEXT("reparent_blueprint_component") ||
                     CommandType == TEXT("get_blueprint_class_settings") ||
                     CommandType == TEXT("add_blueprint_interface") ||
                     CommandType == TEXT("remove_blueprint_interface") ||
                     CommandType == TEXT("get_blueprint_defaults"))
            {
                ResultJson = BlueprintCommands->HandleCommand(CommandType, Params);
            }
            // Blueprint Node Commands
            else if (CommandType == TEXT("connect_blueprint_nodes") ||
                     CommandType == TEXT("add_blueprint_get_self_component_reference") ||
                     CommandType == TEXT("add_blueprint_self_reference") ||
                     CommandType == TEXT("find_blueprint_nodes") ||
                     CommandType == TEXT("add_blueprint_event_node") ||
                     CommandType == TEXT("add_blueprint_input_action_node") ||
                     CommandType == TEXT("add_blueprint_function_node") ||
                     CommandType == TEXT("add_blueprint_get_component_node") ||
                     CommandType == TEXT("add_blueprint_variable") ||
                     CommandType == TEXT("spawn_k2_node") ||
                     CommandType == TEXT("smart_connect_pins") ||
                     CommandType == TEXT("read_blueprint_graph") ||
                     CommandType == TEXT("create_blueprint_function") ||
                     CommandType == TEXT("delete_blueprint_node") ||
                     CommandType == TEXT("disconnect_blueprint_pin") ||
                     CommandType == TEXT("set_pin_default_value") ||
                     CommandType == TEXT("remove_blueprint_variable") ||
                     CommandType == TEXT("set_blueprint_variable_defaults") ||
                     CommandType == TEXT("remove_blueprint_graph"))
            {
                ResultJson = BlueprintNodeCommands->HandleCommand(CommandType, Params);
            }
            // Project Commands
            else if (CommandType == TEXT("create_input_mapping"))
            {
                ResultJson = ProjectCommands->HandleCommand(CommandType, Params);
            }
            // UMG Commands
            else if (CommandType == TEXT("create_umg_widget_blueprint") ||
                     CommandType == TEXT("add_text_block_to_widget") ||
                     CommandType == TEXT("add_button_to_widget") ||
                     CommandType == TEXT("bind_widget_event") ||
                     CommandType == TEXT("set_text_block_binding") ||
                     CommandType == TEXT("add_widget_to_viewport"))
            {
                ResultJson = UMGCommands->HandleCommand(CommandType, Params);
            }
            // Material Commands
            else if (CommandType == TEXT("create_material") ||
                     CommandType == TEXT("create_material_instance") ||
                     CommandType == TEXT("create_material_parameter_collection") ||
                     CommandType == TEXT("add_material_expression") ||
                     CommandType == TEXT("connect_material_expressions") ||
                     CommandType == TEXT("connect_material_to_property") ||
                     CommandType == TEXT("set_material_property") ||
                     CommandType == TEXT("recompile_material") ||
                     CommandType == TEXT("set_material_instance_scalar_parameter") ||
                     CommandType == TEXT("set_material_instance_vector_parameter") ||
                     CommandType == TEXT("set_material_instance_texture_parameter") ||
                     CommandType == TEXT("set_material_instance_static_switch_parameter") ||
                     CommandType == TEXT("get_material_info") ||
                     CommandType == TEXT("get_custom_expression_code") ||
                     CommandType == TEXT("set_custom_expression_code") ||
                     CommandType == TEXT("get_expression_properties") ||
                     CommandType == TEXT("set_expression_property") ||
                     CommandType == TEXT("disconnect_expression") ||
                     CommandType == TEXT("remove_expression"))
            {
                ResultJson = MaterialCommands->HandleCommand(CommandType, Params);
            }
            // Audio Commands
            else if (CommandType == TEXT("create_sound_class") ||
                     CommandType == TEXT("create_sound_mix") ||
                     CommandType == TEXT("set_sound_class_parent") ||
                     CommandType == TEXT("get_audio_info"))
            {
                ResultJson = AudioCommands->HandleCommand(CommandType, Params);
            }
            // Enhanced Input Commands
            else if (CommandType == TEXT("create_input_action") ||
                     CommandType == TEXT("create_input_mapping_context") ||
                     CommandType == TEXT("add_input_mapping") ||
                     CommandType == TEXT("get_input_info"))
            {
                ResultJson = InputCommands->HandleCommand(CommandType, Params);
            }
            // Asset Management Commands
            else if (CommandType == TEXT("search_assets") ||
                     CommandType == TEXT("import_asset") ||
                     CommandType == TEXT("duplicate_asset") ||
                     CommandType == TEXT("rename_asset") ||
                     CommandType == TEXT("move_asset") ||
                     CommandType == TEXT("delete_asset") ||
                     CommandType == TEXT("get_asset_dependencies") ||
                     CommandType == TEXT("save_asset"))
            {
                ResultJson = AssetCommands->HandleCommand(CommandType, Params);
            }
            // Actor Query Commands
            else if (CommandType == TEXT("get_actor_transform") ||
                     CommandType == TEXT("get_actor_components") ||
                     CommandType == TEXT("get_bounding_box") ||
                     CommandType == TEXT("attach_actor") ||
                     CommandType == TEXT("detach_actor") ||
                     CommandType == TEXT("set_actor_visibility") ||
                     CommandType == TEXT("duplicate_actor") ||
                     CommandType == TEXT("add_actor_tag") ||
                     CommandType == TEXT("remove_actor_tag") ||
                     CommandType == TEXT("find_actors_by_tag"))
            {
                ResultJson = ActorQueryCommands->HandleCommand(CommandType, Params);
            }
            // Level Management Commands
            else if (CommandType == TEXT("open_level") ||
                     CommandType == TEXT("save_level") ||
                     CommandType == TEXT("list_levels") ||
                     CommandType == TEXT("create_level"))
            {
                ResultJson = LevelCommands->HandleCommand(CommandType, Params);
            }
            // Animation + Sequence Commands
            else if (CommandType == TEXT("create_blend_space") ||
                     CommandType == TEXT("add_anim_notify") ||
                     CommandType == TEXT("add_anim_notify_state") ||
                     CommandType == TEXT("add_skeletal_mesh_socket") ||
                     CommandType == TEXT("play_animation") ||
                     CommandType == TEXT("create_anim_blueprint") ||
                     CommandType == TEXT("create_sequence") ||
                     CommandType == TEXT("add_actor_to_sequence") ||
                     CommandType == TEXT("play_sequence"))
            {
                ResultJson = AnimationCommands->HandleCommand(CommandType, Params);
            }
            // AnimGraph Commands (AnimGraph node manipulation in Animation Blueprints)
            else if (CommandType == TEXT("read_anim_graph") ||
                     CommandType == TEXT("add_anim_graph_node") ||
                     CommandType == TEXT("connect_anim_pins") ||
                     CommandType == TEXT("set_anim_node_property") ||
                     CommandType == TEXT("delete_anim_graph_node") ||
                     CommandType == TEXT("disconnect_anim_pin") ||
                     CommandType == TEXT("find_anim_graph_nodes") ||
                     // Tier 2 — State Machine
                     CommandType == TEXT("add_state_machine") ||
                     CommandType == TEXT("add_state") ||
                     CommandType == TEXT("add_state_transition") ||
                     CommandType == TEXT("set_state_animation") ||
                     CommandType == TEXT("read_state_machine") ||
                     // Tier 3 — Advanced
                     CommandType == TEXT("add_anim_layer") ||
                     CommandType == TEXT("add_blend_node") ||
                     CommandType == TEXT("add_blend_pose_pin") ||
                     CommandType == TEXT("set_anim_blueprint_parent") ||
                     CommandType == TEXT("compile_anim_blueprint") ||
                     CommandType == TEXT("rename_state") ||
                     // Tier 2.5 — State Machine utilities
                     CommandType == TEXT("set_state_entry") ||
                     CommandType == TEXT("bind_transition_condition") ||
                     CommandType == TEXT("connect_k2_pins") ||
                     // Tier 3.5 — Property Access binding & AnimNode function binding
                     CommandType == TEXT("bind_anim_pin_to_property") ||
                     CommandType == TEXT("bind_anim_node_function") ||
                     CommandType == TEXT("create_anim_graph_function") ||
                     // Tier 4 — PoseSearch
                     CommandType == TEXT("configure_motion_matching") ||
                     CommandType == TEXT("configure_history_collector"))
            {
                ResultJson = AnimGraphCommands->HandleCommand(CommandType, Params);
            }
            // Niagara/VFX Commands
            else if (CommandType == TEXT("create_niagara_system") ||
                     CommandType == TEXT("create_niagara_emitter"))
            {
                ResultJson = NiagaraCommands->HandleCommand(CommandType, Params);
            }
            // World/Environment Commands
            else if (CommandType == TEXT("add_foliage_type") ||
                     CommandType == TEXT("paint_foliage") ||
                     CommandType == TEXT("list_foliage_types") ||
                     CommandType == TEXT("get_landscape_info"))
            {
                ResultJson = WorldCommands->HandleCommand(CommandType, Params);
            }
            // DataAsset / Generic Asset Commands
            else if (CommandType == TEXT("create_data_asset") ||
                     CommandType == TEXT("create_asset") ||
                     CommandType == TEXT("set_data_asset_property") ||
                     CommandType == TEXT("get_data_asset_properties") ||
                     CommandType == TEXT("get_array_element") ||
                     CommandType == TEXT("set_array_element") ||
                     CommandType == TEXT("add_array_element") ||
                     CommandType == TEXT("remove_array_element") ||
                     CommandType == TEXT("get_array_length") ||
                     CommandType == TEXT("import_property_text"))
            {
                ResultJson = DataAssetCommands->HandleCommand(CommandType, Params);
            }
            // ChooserTable Commands
            else if (CommandType == TEXT("read_chooser_table") ||
                     CommandType == TEXT("set_chooser_column_value"))
            {
                ResultJson = ChooserCommands->HandleCommand(CommandType, Params);
            }
            else
            {
                ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
                ResponseJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown command: %s"), *CommandType));
                
                FString ResultString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
                FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
                Promise.SetValue(ResultString);
                return;
            }
            
            // Check if the result contains an error
            bool bSuccess = true;
            FString ErrorMessage;
            
            if (ResultJson->HasField(TEXT("success")))
            {
                bSuccess = ResultJson->GetBoolField(TEXT("success"));
                if (!bSuccess)
                {
                    if (ResultJson->HasField(TEXT("error")))
                    {
                        ErrorMessage = ResultJson->GetStringField(TEXT("error"));
                    }

                    // If no error field, try to extract from log array (e.g. execute_python)
                    if (ErrorMessage.IsEmpty() && ResultJson->HasField(TEXT("log")))
                    {
                        const TArray<TSharedPtr<FJsonValue>>* LogArray;
                        if (ResultJson->TryGetArrayField(TEXT("log"), LogArray))
                        {
                            for (const auto& Entry : *LogArray)
                            {
                                const TSharedPtr<FJsonObject>* EntryObj;
                                if (Entry->TryGetObject(EntryObj))
                                {
                                    FString EntryType;
                                    (*EntryObj)->TryGetStringField(TEXT("type"), EntryType);
                                    if (EntryType == TEXT("error"))
                                    {
                                        FString Msg;
                                        if ((*EntryObj)->TryGetStringField(TEXT("message"), Msg))
                                        {
                                            if (!ErrorMessage.IsEmpty()) ErrorMessage += TEXT("\n");
                                            ErrorMessage += Msg;
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Last resort: use the result field
                    if (ErrorMessage.IsEmpty() && ResultJson->HasField(TEXT("result")))
                    {
                        ErrorMessage = ResultJson->GetStringField(TEXT("result"));
                    }

                    if (ErrorMessage.IsEmpty())
                    {
                        ErrorMessage = TEXT("Command failed (no error details available)");
                    }
                }
            }
            
            if (bSuccess)
            {
                // Set success status and include the result
                ResponseJson->SetStringField(TEXT("status"), TEXT("success"));
                ResponseJson->SetObjectField(TEXT("result"), ResultJson);
            }
            else
            {
                // Set error status and include the error message
                ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
                ResponseJson->SetStringField(TEXT("error"), ErrorMessage);
            }
        }
        catch (const std::exception& e)
        {
            ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
            ResponseJson->SetStringField(TEXT("error"), UTF8_TO_TCHAR(e.what()));
        }
        
        FString ResultString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
        FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);

        double DoneTime = FPlatformTime::Seconds();
        UE_LOG(LogTemp, Display, TEXT("MCP: '%s' completed in %.2fs (%.2fs queue + %.2fs exec)"),
            *CommandType, DoneTime - QueuedTime, PickupTime - QueuedTime, DoneTime - PickupTime);

        Promise.SetValue(ResultString);
    });

    return Future.Get();
}