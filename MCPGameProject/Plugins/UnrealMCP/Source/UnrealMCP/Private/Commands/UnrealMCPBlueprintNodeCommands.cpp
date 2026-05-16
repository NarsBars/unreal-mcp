#include "Commands/UnrealMCPBlueprintNodeCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "UObject/FieldIterator.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_Select.h"
#include "K2Node_SwitchInteger.h"
#include "K2Node_SwitchString.h"
#include "K2Node_VariableSet.h"
#include "K2Node_CommutativeAssociativeBinaryOperator.h"
#include "Kismet/KismetMathLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "GameFramework/InputSettings.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "EdGraphSchema_K2.h"
#include "EditorAssetLibrary.h"

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
    else if (CommandType == TEXT("spawn_k2_node"))
    {
        return HandleSpawnK2Node(Params);
    }
    else if (CommandType == TEXT("smart_connect_pins"))
    {
        return HandleSmartConnectPins(Params);
    }
    else if (CommandType == TEXT("read_blueprint_graph"))
    {
        return HandleReadBlueprintGraph(Params);
    }
    else if (CommandType == TEXT("create_blueprint_function"))
    {
        return HandleCreateBlueprintFunction(Params);
    }
    else if (CommandType == TEXT("delete_blueprint_node"))
    {
        return HandleDeleteBlueprintNode(Params);
    }
    else if (CommandType == TEXT("disconnect_blueprint_pin"))
    {
        return HandleDisconnectBlueprintPin(Params);
    }
    else if (CommandType == TEXT("set_pin_default_value"))
    {
        return HandleSetPinDefaultValue(Params);
    }
    else if (CommandType == TEXT("remove_blueprint_variable"))
    {
        return HandleRemoveBlueprintVariable(Params);
    }
    else if (CommandType == TEXT("set_blueprint_variable_defaults"))
    {
        return HandleSetBlueprintVariableDefaults(Params);
    }
    else if (CommandType == TEXT("remove_blueprint_graph"))
    {
        return HandleRemoveBlueprintGraph(Params);
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

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Get target graph (defaults to EventGraph if graph_name not specified)
    FString TargetGraphName;
    Params->TryGetStringField(TEXT("graph_name"), TargetGraphName);
    UEdGraph* EventGraph = TargetGraphName.IsEmpty()
        ? FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint)
        : FUnrealMCPCommonUtils::FindGraphByName(Blueprint, TargetGraphName);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TargetGraphName.IsEmpty()
                ? FString(TEXT("Failed to get event graph"))
                : FString::Printf(TEXT("Graph not found: %s"), *TargetGraphName));
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

    // Connect the nodes
    if (FUnrealMCPCommonUtils::ConnectGraphNodes(EventGraph, SourceNode, SourcePinName, TargetNode, TargetPinName))
    {
        // Mark the blueprint as modified
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        bool bSave = true;
        if (Params->HasField(TEXT("save")))
        {
            bSave = Params->GetBoolField(TEXT("save"));
        }
        if (bSave && Blueprint)
        {
            FString SavePath = Blueprint->GetOutermost()->GetName();
            UEditorAssetLibrary::SaveAsset(SavePath, false);
        }

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("source_node_id"), SourceNodeId);
        ResultObj->SetStringField(TEXT("target_node_id"), TargetNodeId);
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

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Get target graph (defaults to EventGraph if graph_name not specified)
    FString TargetGraphName;
    Params->TryGetStringField(TEXT("graph_name"), TargetGraphName);
    UEdGraph* EventGraph = TargetGraphName.IsEmpty()
        ? FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint)
        : FUnrealMCPCommonUtils::FindGraphByName(Blueprint, TargetGraphName);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TargetGraphName.IsEmpty()
                ? FString(TEXT("Failed to get event graph"))
                : FString::Printf(TEXT("Graph not found: %s"), *TargetGraphName));
    }
    
    // We'll skip component verification since the GetAllNodes API may have changed in UE5.5
    
    // Create the variable get node directly
    UK2Node_VariableGet* GetComponentNode = NewObject<UK2Node_VariableGet>(EventGraph);
    if (!GetComponentNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create get component node"));
    }
    
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
    
    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    bool bSave = true;
    if (Params->HasField(TEXT("save")))
    {
        bSave = Params->GetBoolField(TEXT("save"));
    }
    if (bSave && Blueprint)
    {
        FString SavePath = Blueprint->GetOutermost()->GetName();
        UEditorAssetLibrary::SaveAsset(SavePath, false);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), GetComponentNode->NodeGuid.ToString());
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

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Get target graph (defaults to EventGraph if graph_name not specified)
    FString TargetGraphName;
    Params->TryGetStringField(TEXT("graph_name"), TargetGraphName);
    UEdGraph* EventGraph = TargetGraphName.IsEmpty()
        ? FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint)
        : FUnrealMCPCommonUtils::FindGraphByName(Blueprint, TargetGraphName);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TargetGraphName.IsEmpty()
                ? FString(TEXT("Failed to get event graph"))
                : FString::Printf(TEXT("Graph not found: %s"), *TargetGraphName));
    }

    // Create the event node
    UK2Node_Event* EventNode = FUnrealMCPCommonUtils::CreateEventNode(EventGraph, EventName, NodePosition);
    if (!EventNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create event node"));
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    bool bSave = true;
    if (Params->HasField(TEXT("save")))
    {
        bSave = Params->GetBoolField(TEXT("save"));
    }
    if (bSave && Blueprint)
    {
        FString SavePath = Blueprint->GetOutermost()->GetName();
        UEditorAssetLibrary::SaveAsset(SavePath, false);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), EventNode->NodeGuid.ToString());
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

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Get target graph (defaults to EventGraph if graph_name not specified)
    FString TargetGraphName;
    Params->TryGetStringField(TEXT("graph_name"), TargetGraphName);
    UEdGraph* EventGraph = TargetGraphName.IsEmpty()
        ? FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint)
        : FUnrealMCPCommonUtils::FindGraphByName(Blueprint, TargetGraphName);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TargetGraphName.IsEmpty()
                ? FString(TEXT("Failed to get event graph"))
                : FString::Printf(TEXT("Graph not found: %s"), *TargetGraphName));
    }

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
        TargetClass = FindObject<UClass>(nullptr, *Target);
        UE_LOG(LogTemp, Display, TEXT("Tried to find class '%s': %s"), 
               *Target, TargetClass ? TEXT("Found") : TEXT("Not found"));
        
        // If not found, try with U prefix (common convention for UE classes)
        if (!TargetClass && !Target.StartsWith(TEXT("U")))
        {
            FString TargetWithPrefix = FString(TEXT("U")) + Target;
            TargetClass = FindObject<UClass>(nullptr, *TargetWithPrefix);
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
                TargetClass = FindObject<UClass>(nullptr, *ClassName);
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
            TargetClass = FindObject<UClass>(nullptr, TEXT("UGameplayStatics"));
            if (!TargetClass)
            {
                // Try loading it from its known package
                TargetClass = LoadObject<UClass>(nullptr, TEXT("/Script/Engine.GameplayStatics"));
                UE_LOG(LogTemp, Display, TEXT("Explicitly loading GameplayStatics: %s"),
                       TargetClass ? TEXT("Success") : TEXT("Failed"));
            }
        }

        // Try loading as a Blueprint asset path (e.g., "/Game/Weather/BP_WeatherZone" or "/Game/.../BP_StylizedSky")
        if (!TargetClass && Target.StartsWith(TEXT("/Game/")))
        {
            // Try loading the Blueprint asset and getting its generated class
            FString BlueprintPath = Target;
            if (!BlueprintPath.EndsWith(TEXT("_C")))
            {
                // Try as Blueprint asset first
                UBlueprint* TargetBP = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
                if (!TargetBP)
                {
                    // Try with .BP suffix (e.g., /Game/Path/BP_Name.BP_Name)
                    FString AssetName = FPaths::GetBaseFilename(BlueprintPath);
                    FString FullPath = BlueprintPath + TEXT(".") + AssetName;
                    TargetBP = LoadObject<UBlueprint>(nullptr, *FullPath);
                }
                if (TargetBP && TargetBP->GeneratedClass)
                {
                    TargetClass = TargetBP->GeneratedClass;
                    UE_LOG(LogTemp, Display, TEXT("Found Blueprint class at path '%s': %s"), *BlueprintPath, *TargetClass->GetName());
                }
            }
            else
            {
                // Already a _C class path, try loading directly
                TargetClass = LoadObject<UClass>(nullptr, *BlueprintPath);
                UE_LOG(LogTemp, Display, TEXT("Tried loading class path '%s': %s"), *BlueprintPath, TargetClass ? TEXT("Found") : TEXT("Not found"));
            }
        }

        // Try resolving Target as a variable name on the calling Blueprint
        if (!TargetClass && Blueprint)
        {
            for (FBPVariableDescription& Var : Blueprint->NewVariables)
            {
                if (Var.VarName.ToString() == Target)
                {
                    // Get the variable's class type
                    if (Var.VarType.PinCategory == UEdGraphSchema_K2::PC_Object ||
                        Var.VarType.PinCategory == UEdGraphSchema_K2::PC_SoftObject)
                    {
                        UClass* VarClass = Cast<UClass>(Var.VarType.PinSubCategoryObject.Get());
                        if (VarClass)
                        {
                            TargetClass = VarClass;
                            UE_LOG(LogTemp, Display, TEXT("Resolved variable '%s' to class '%s'"), *Target, *TargetClass->GetName());
                        }
                    }
                    break;
                }
            }
        }

        // Try common /Script/ module paths for class resolution
        if (!TargetClass)
        {
            TArray<FString> ModulePaths;
            ModulePaths.Add(FString::Printf(TEXT("/Script/Engine.%s"), *Target));
            ModulePaths.Add(FString::Printf(TEXT("/Script/Angelscript.%s"), *Target));
            ModulePaths.Add(FString::Printf(TEXT("/Script/%s.%s"), FApp::GetProjectName(), *Target));
            ModulePaths.Add(FString::Printf(TEXT("/Script/CoreUObject.%s"), *Target));
            ModulePaths.Add(FString::Printf(TEXT("/Script/UMG.%s"), *Target));

            for (const FString& ModulePath : ModulePaths)
            {
                TargetClass = LoadObject<UClass>(nullptr, *ModulePath);
                if (TargetClass)
                {
                    UE_LOG(LogTemp, Display, TEXT("Found class at module path '%s'"), *ModulePath);
                    break;
                }
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
                            
                            UClass* Class = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::NativeFirst);

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

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    bool bSave = true;
    if (Params->HasField(TEXT("save")))
    {
        bSave = Params->GetBoolField(TEXT("save"));
    }
    if (bSave && Blueprint)
    {
        FString SavePath = Blueprint->GetOutermost()->GetName();
        UEditorAssetLibrary::SaveAsset(SavePath, false);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), FunctionNode->NodeGuid.ToString());
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

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

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

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    bool bSave = true;
    if (Params->HasField(TEXT("save")))
    {
        bSave = Params->GetBoolField(TEXT("save"));
    }
    if (bSave && Blueprint)
    {
        FString SavePath = Blueprint->GetOutermost()->GetName();
        UEditorAssetLibrary::SaveAsset(SavePath, false);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("variable_name"), VariableName);
    ResultObj->SetStringField(TEXT("variable_type"), VariableType);
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

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Get target graph (defaults to EventGraph if graph_name not specified)
    FString TargetGraphName;
    Params->TryGetStringField(TEXT("graph_name"), TargetGraphName);
    UEdGraph* EventGraph = TargetGraphName.IsEmpty()
        ? FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint)
        : FUnrealMCPCommonUtils::FindGraphByName(Blueprint, TargetGraphName);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TargetGraphName.IsEmpty()
                ? FString(TEXT("Failed to get event graph"))
                : FString::Printf(TEXT("Graph not found: %s"), *TargetGraphName));
    }

    // Create the input action node
    UK2Node_InputAction* InputActionNode = FUnrealMCPCommonUtils::CreateInputActionNode(EventGraph, ActionName, NodePosition);
    if (!InputActionNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create input action node"));
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    bool bSave = true;
    if (Params->HasField(TEXT("save")))
    {
        bSave = Params->GetBoolField(TEXT("save"));
    }
    if (bSave && Blueprint)
    {
        FString SavePath = Blueprint->GetOutermost()->GetName();
        UEditorAssetLibrary::SaveAsset(SavePath, false);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), InputActionNode->NodeGuid.ToString());
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

    // Get position parameters (optional)
    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Get target graph (defaults to EventGraph if graph_name not specified)
    FString TargetGraphName;
    Params->TryGetStringField(TEXT("graph_name"), TargetGraphName);
    UEdGraph* EventGraph = TargetGraphName.IsEmpty()
        ? FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint)
        : FUnrealMCPCommonUtils::FindGraphByName(Blueprint, TargetGraphName);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TargetGraphName.IsEmpty()
                ? FString(TEXT("Failed to get event graph"))
                : FString::Printf(TEXT("Graph not found: %s"), *TargetGraphName));
    }

    // Create the self node
    UK2Node_Self* SelfNode = FUnrealMCPCommonUtils::CreateSelfReferenceNode(EventGraph, NodePosition);
    if (!SelfNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create self node"));
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    bool bSave = true;
    if (Params->HasField(TEXT("save")))
    {
        bSave = Params->GetBoolField(TEXT("save"));
    }
    if (bSave && Blueprint)
    {
        FString SavePath = Blueprint->GetOutermost()->GetName();
        UEditorAssetLibrary::SaveAsset(SavePath, false);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), SelfNode->NodeGuid.ToString());
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

    // Get target graph (defaults to EventGraph if graph_name not specified)
    FString TargetGraphName;
    Params->TryGetStringField(TEXT("graph_name"), TargetGraphName);
    UEdGraph* EventGraph = TargetGraphName.IsEmpty()
        ? FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint)
        : FUnrealMCPCommonUtils::FindGraphByName(Blueprint, TargetGraphName);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TargetGraphName.IsEmpty()
                ? FString(TEXT("Failed to get event graph"))
                : FString::Printf(TEXT("Graph not found: %s"), *TargetGraphName));
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

    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleSpawnK2Node(const TSharedPtr<FJsonObject>& Params)
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

    // Get position parameters (optional)
    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Get target graph (defaults to EventGraph if graph_name not specified)
    FString TargetGraphName;
    Params->TryGetStringField(TEXT("graph_name"), TargetGraphName);
    UEdGraph* EventGraph = TargetGraphName.IsEmpty()
        ? FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint)
        : FUnrealMCPCommonUtils::FindGraphByName(Blueprint, TargetGraphName);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TargetGraphName.IsEmpty()
                ? FString(TEXT("Failed to get event graph"))
                : FString::Printf(TEXT("Graph not found: %s"), *TargetGraphName));
    }

    // Create the appropriate K2 node based on type
    UEdGraphNode* NewNode = nullptr;

    if (NodeType == TEXT("IfThenElse") || NodeType == TEXT("Branch"))
    {
        UK2Node_IfThenElse* IfNode = NewObject<UK2Node_IfThenElse>(EventGraph);
        IfNode->SetFlags(RF_Transactional);
        EventGraph->AddNode(IfNode, true, false);
        IfNode->CreateNewGuid();
        IfNode->PostPlacedNewNode();
        IfNode->AllocateDefaultPins();
        NewNode = IfNode;
    }
    else if (NodeType == TEXT("Sequence") || NodeType == TEXT("ExecutionSequence"))
    {
        UK2Node_ExecutionSequence* SeqNode = NewObject<UK2Node_ExecutionSequence>(EventGraph);
        SeqNode->SetFlags(RF_Transactional);
        EventGraph->AddNode(SeqNode, true, false);
        SeqNode->CreateNewGuid();
        SeqNode->PostPlacedNewNode();
        SeqNode->AllocateDefaultPins();
        NewNode = SeqNode;
    }
    else if (NodeType == TEXT("Select"))
    {
        UK2Node_Select* SelectNode = NewObject<UK2Node_Select>(EventGraph);
        SelectNode->SetFlags(RF_Transactional);
        EventGraph->AddNode(SelectNode, true, false);
        SelectNode->CreateNewGuid();
        SelectNode->PostPlacedNewNode();
        SelectNode->AllocateDefaultPins();
        NewNode = SelectNode;
    }
    else if (NodeType == TEXT("SwitchInteger") || NodeType == TEXT("SwitchInt"))
    {
        UK2Node_SwitchInteger* SwitchNode = NewObject<UK2Node_SwitchInteger>(EventGraph);
        SwitchNode->SetFlags(RF_Transactional);
        EventGraph->AddNode(SwitchNode, true, false);
        SwitchNode->CreateNewGuid();
        SwitchNode->PostPlacedNewNode();
        SwitchNode->AllocateDefaultPins();
        NewNode = SwitchNode;
    }
    else if (NodeType == TEXT("SwitchString"))
    {
        UK2Node_SwitchString* SwitchNode = NewObject<UK2Node_SwitchString>(EventGraph);
        SwitchNode->SetFlags(RF_Transactional);
        EventGraph->AddNode(SwitchNode, true, false);
        SwitchNode->CreateNewGuid();
        SwitchNode->PostPlacedNewNode();
        SwitchNode->AllocateDefaultPins();
        NewNode = SwitchNode;
    }
    else if (NodeType == TEXT("CallFunction"))
    {
        FString FunctionName;
        if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("CallFunction requires 'function_name' parameter"));
        }

        UK2Node_CallFunction* FuncNode = NewObject<UK2Node_CallFunction>(EventGraph);

        // Parse "ClassName::FunctionName" or just "FunctionName"
        FString ClassName, FuncName;
        if (FunctionName.Split(TEXT("::"), &ClassName, &FuncName))
        {
            UClass* OwnerClass = nullptr;
            // Try common module paths
            OwnerClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ClassName));
            if (!OwnerClass)
                OwnerClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/AnimGraphRuntime.%s"), *ClassName));
            if (!OwnerClass)
                OwnerClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/CoreUObject.%s"), *ClassName));
            if (!OwnerClass)
                OwnerClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Angelscript.%s"), *ClassName));
            if (!OwnerClass)
                OwnerClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/%s.%s"), FApp::GetProjectName(), *ClassName));

            // Try loading as a Blueprint asset path (e.g., "/Game/Path/BP_Name")
            if (!OwnerClass && ClassName.StartsWith(TEXT("/Game/")))
            {
                UBlueprint* TargetBP = LoadObject<UBlueprint>(nullptr, *ClassName);
                if (!TargetBP)
                {
                    FString AssetName = FPaths::GetBaseFilename(ClassName);
                    TargetBP = LoadObject<UBlueprint>(nullptr, *(ClassName + TEXT(".") + AssetName));
                }
                if (TargetBP && TargetBP->GeneratedClass)
                {
                    OwnerClass = TargetBP->GeneratedClass;
                }
            }

            // Fallback: iterate all loaded classes by short name
            if (!OwnerClass)
            {
                for (TObjectIterator<UClass> It; It; ++It)
                {
                    if (It->GetName() == ClassName)
                    {
                        OwnerClass = *It;
                        break;
                    }
                }
            }
            if (OwnerClass)
            {
                UFunction* Func = OwnerClass->FindFunctionByName(FName(*FuncName));
                // If not found by exact name, try case-insensitive and space-stripped search
                if (!Func)
                {
                    FString NormalizedName = FuncName.Replace(TEXT(" "), TEXT(""));
                    for (TFieldIterator<UFunction> FuncIt(OwnerClass); FuncIt; ++FuncIt)
                    {
                        FString IterName = FuncIt->GetName();
                        FString IterNormalized = IterName.Replace(TEXT(" "), TEXT(""));
                        if (IterName.Equals(FuncName, ESearchCase::IgnoreCase) ||
                            IterNormalized.Equals(NormalizedName, ESearchCase::IgnoreCase))
                        {
                            Func = *FuncIt;
                            break;
                        }
                    }
                }
                if (Func)
                {
                    FuncNode->SetFromFunction(Func);
                }
                else
                {
                    return FUnrealMCPCommonUtils::CreateErrorResponse(
                        FString::Printf(TEXT("Function '%s' not found on class '%s'"), *FuncName, *ClassName));
                }
            }
            else
            {
                return FUnrealMCPCommonUtils::CreateErrorResponse(
                    FString::Printf(TEXT("Class '%s' not found"), *ClassName));
            }
        }
        else
        {
            // Self member function
            FuncNode->FunctionReference.SetSelfMember(FName(*FunctionName));
        }

        FuncNode->SetFlags(RF_Transactional);
        EventGraph->AddNode(FuncNode, true, false);
        FuncNode->CreateNewGuid();
        FuncNode->PostPlacedNewNode();
        FuncNode->AllocateDefaultPins();
        NewNode = FuncNode;
    }
    else if (NodeType == TEXT("VariableGet") || NodeType == TEXT("GetVariable"))
    {
        FString VariableName;
        if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("VariableGet requires 'variable_name' parameter"));
        }

        // Optional: target_class for external member access (e.g., accessing a property on another class)
        // Can be at top level or inside pin_defaults (since MCP tool schema may not expose it at top level)
        FString TargetClassName;
        if (!Params->TryGetStringField(TEXT("target_class"), TargetClassName))
        {
            const TSharedPtr<FJsonObject>* PinDefaultsObj;
            if (Params->TryGetObjectField(TEXT("pin_defaults"), PinDefaultsObj))
            {
                (*PinDefaultsObj)->TryGetStringField(TEXT("target_class"), TargetClassName);
            }
        }

        UK2Node_VariableGet* VarNode = NewObject<UK2Node_VariableGet>(EventGraph);
        FMemberReference VarRef;

        if (!TargetClassName.IsEmpty())
        {
            // External member — find the target class
            UClass* TargetClass = nullptr;

            // Try /Game/ Blueprint path
            if (TargetClassName.StartsWith(TEXT("/Game/")))
            {
                UBlueprint* TargetBP = LoadObject<UBlueprint>(nullptr, *TargetClassName);
                if (!TargetBP)
                {
                    FString AssetName = FPaths::GetBaseFilename(TargetClassName);
                    TargetBP = LoadObject<UBlueprint>(nullptr, *(TargetClassName + TEXT(".") + AssetName));
                }
                if (TargetBP && TargetBP->GeneratedClass)
                    TargetClass = TargetBP->GeneratedClass;
            }

            // Try by class name (TObjectIterator)
            if (!TargetClass)
            {
                for (TObjectIterator<UClass> It; It; ++It)
                {
                    if (It->GetName() == TargetClassName)
                    {
                        TargetClass = *It;
                        break;
                    }
                }
            }

            // Try resolving as variable name on the Blueprint
            if (!TargetClass && Blueprint)
            {
                for (FBPVariableDescription& Var : Blueprint->NewVariables)
                {
                    if (Var.VarName.ToString() == TargetClassName)
                    {
                        if (Var.VarType.PinCategory == UEdGraphSchema_K2::PC_Object ||
                            Var.VarType.PinCategory == UEdGraphSchema_K2::PC_SoftObject)
                        {
                            TargetClass = Cast<UClass>(Var.VarType.PinSubCategoryObject.Get());
                        }
                        break;
                    }
                }
            }

            if (TargetClass)
            {
                VarRef.SetExternalMember(FName(*VariableName), TargetClass);
                UE_LOG(LogTemp, Display, TEXT("VariableGet: external member '%s' on class '%s'"), *VariableName, *TargetClass->GetName());
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("VariableGet: target_class '%s' not found, falling back to self member"), *TargetClassName);
                VarRef.SetSelfMember(FName(*VariableName));
            }
        }
        else
        {
            VarRef.SetSelfMember(FName(*VariableName));
        }

        VarNode->VariableReference = VarRef;
        VarNode->SetFlags(RF_Transactional);
        EventGraph->AddNode(VarNode, true, false);
        VarNode->CreateNewGuid();
        VarNode->PostPlacedNewNode();
        VarNode->AllocateDefaultPins();
        NewNode = VarNode;
    }
    else if (NodeType == TEXT("VariableSet") || NodeType == TEXT("SetVariable"))
    {
        FString VariableName;
        if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("VariableSet requires 'variable_name' parameter"));
        }

        UK2Node_VariableSet* VarNode = NewObject<UK2Node_VariableSet>(EventGraph);
        FMemberReference VarRef;
        VarRef.SetSelfMember(FName(*VariableName));
        VarNode->VariableReference = VarRef;
        VarNode->SetFlags(RF_Transactional);
        EventGraph->AddNode(VarNode, true, false);
        VarNode->CreateNewGuid();
        VarNode->PostPlacedNewNode();
        VarNode->AllocateDefaultPins();
        NewNode = VarNode;
    }
    else if (NodeType == TEXT("Less") || NodeType == TEXT("LessEqual") ||
             NodeType == TEXT("Greater") || NodeType == TEXT("GreaterEqual") ||
             NodeType == TEXT("Equal") || NodeType == TEXT("NotEqual"))
    {
        UK2Node_CallFunction* FuncNode = NewObject<UK2Node_CallFunction>(EventGraph);
        UFunction* MathFunc = nullptr;

        if (NodeType == TEXT("Less"))
            MathFunc = UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Less_DoubleDouble));
        else if (NodeType == TEXT("LessEqual"))
            MathFunc = UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, LessEqual_DoubleDouble));
        else if (NodeType == TEXT("Greater"))
            MathFunc = UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Greater_DoubleDouble));
        else if (NodeType == TEXT("GreaterEqual"))
            MathFunc = UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, GreaterEqual_DoubleDouble));
        else if (NodeType == TEXT("Equal"))
            MathFunc = UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_DoubleDouble));
        else if (NodeType == TEXT("NotEqual"))
            MathFunc = UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, NotEqual_DoubleDouble));

        if (MathFunc) FuncNode->SetFromFunction(MathFunc);
        FuncNode->SetFlags(RF_Transactional);
        EventGraph->AddNode(FuncNode, true, false);
        FuncNode->CreateNewGuid();
        FuncNode->PostPlacedNewNode();
        FuncNode->AllocateDefaultPins();
        NewNode = FuncNode;
    }
    else if (NodeType == TEXT("AND") || NodeType == TEXT("OR"))
    {
        UK2Node_CommutativeAssociativeBinaryOperator* OpNode =
            NewObject<UK2Node_CommutativeAssociativeBinaryOperator>(EventGraph);
        UFunction* BoolFunc = NodeType == TEXT("AND")
            ? UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanAND))
            : UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanOR));
        if (BoolFunc) OpNode->SetFromFunction(BoolFunc);
        OpNode->SetFlags(RF_Transactional);
        EventGraph->AddNode(OpNode, true, false);
        OpNode->CreateNewGuid();
        OpNode->PostPlacedNewNode();
        OpNode->AllocateDefaultPins();
        NewNode = OpNode;
    }
    else if (NodeType == TEXT("NOT"))
    {
        UK2Node_CallFunction* FuncNode = NewObject<UK2Node_CallFunction>(EventGraph);
        FuncNode->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Not_PreBool)));
        FuncNode->SetFlags(RF_Transactional);
        EventGraph->AddNode(FuncNode, true, false);
        FuncNode->CreateNewGuid();
        FuncNode->PostPlacedNewNode();
        FuncNode->AllocateDefaultPins();
        NewNode = FuncNode;
    }
    else if (NodeType == TEXT("K2Node_Event") || NodeType == TEXT("Event"))
    {
        // K2Node_Event with function_name — create a properly bound event override node
        FString FunctionName;
        Params->TryGetStringField(TEXT("function_name"), FunctionName);

        if (FunctionName.IsEmpty())
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                TEXT("K2Node_Event requires 'function_name' parameter (name of the event to override)"));
        }

        UK2Node_Event* EventNode = FUnrealMCPCommonUtils::CreateEventNode(
            EventGraph, FunctionName, NodePosition);

        if (!EventNode)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
                TEXT("Failed to create event node for '%s'. Function not found on class hierarchy."),
                *FunctionName));
        }
        NewNode = EventNode;
    }
    else
    {
        // Generic fallback: try to find a K2Node class by name
        FString K2ClassName = NodeType;
        if (!K2ClassName.StartsWith(TEXT("K2Node_")))
        {
            K2ClassName = FString::Printf(TEXT("K2Node_%s"), *NodeType);
        }

        UClass* NodeClass = nullptr;
        for (TObjectIterator<UClass> It; It; ++It)
        {
            if (It->IsChildOf(UK2Node::StaticClass()) && It->GetName() == K2ClassName)
            {
                NodeClass = *It;
                break;
            }
        }

        if (NodeClass)
        {
            UK2Node* K2Node = NewObject<UK2Node>(EventGraph, NodeClass);
            K2Node->SetFlags(RF_Transactional);
            EventGraph->AddNode(K2Node, true, false);
            K2Node->CreateNewGuid();
            K2Node->PostPlacedNewNode();
            K2Node->AllocateDefaultPins();
            NewNode = K2Node;
        }
        else
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
                TEXT("Unknown K2 node type: '%s'. Supported: IfThenElse, Branch, Sequence, Select, "
                     "SwitchInteger, SwitchString, CallFunction, VariableGet, VariableSet, "
                     "Less, LessEqual, Greater, GreaterEqual, Equal, NotEqual, AND, OR, NOT, "
                     "K2Node_Event (with function_name). "
                     "Or any K2Node_ class name (e.g. 'K2Node_EvaluateChooser2')."),
                *NodeType));
        }
    }

    if (NewNode)
    {
        NewNode->NodePosX = NodePosition.X;
        NewNode->NodePosY = NodePosition.Y;

        // Apply pin_defaults if provided
        const TSharedPtr<FJsonObject>* PinDefaults;
        if (Params->TryGetObjectField(TEXT("pin_defaults"), PinDefaults))
        {
            for (auto& KV : (*PinDefaults)->Values)
            {
                FString PinName = KV.Key;
                FString PinValue;
                if (KV.Value->TryGetString(PinValue))
                {
                    // Find and set pin default
                }
                else
                {
                    double NumVal;
                    if (KV.Value->TryGetNumber(NumVal))
                        PinValue = FString::SanitizeFloat(NumVal);
                    else
                    {
                        bool BoolVal;
                        if (KV.Value->TryGetBool(BoolVal))
                            PinValue = BoolVal ? TEXT("true") : TEXT("false");
                    }
                }

                if (!PinValue.IsEmpty())
                {
                    for (UEdGraphPin* Pin : NewNode->Pins)
                    {
                        if (Pin && Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
                        {
                            Pin->DefaultValue = PinValue;
                            break;
                        }
                    }
                }
            }
        }

        // Mark the blueprint as modified
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        bool bSave = true;
        if (Params->HasField(TEXT("save")))
        {
            bSave = Params->GetBoolField(TEXT("save"));
        }
        if (bSave && Blueprint)
        {
            FString SavePath = Blueprint->GetOutermost()->GetName();
            UEditorAssetLibrary::SaveAsset(SavePath, false);
        }

        // Build response with node info and pin names
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetBoolField(TEXT("success"), true);
        ResultObj->SetStringField(TEXT("node_id"), NewNode->NodeGuid.ToString());
        ResultObj->SetStringField(TEXT("node_type"), NodeType);

        // List pin names for easy connection
        TArray<TSharedPtr<FJsonValue>> PinArray;
        for (UEdGraphPin* Pin : NewNode->Pins)
        {
            TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
            PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
            PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
            PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
            PinArray.Add(MakeShared<FJsonValueObject>(PinObj));
        }
        ResultObj->SetArrayField(TEXT("pins"), PinArray);

        return ResultObj;
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create K2 node"));
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleSmartConnectPins(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
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

    // Get optional auto_convert parameter (default true)
    bool bAutoConvert = true;
    if (Params->HasField(TEXT("auto_convert")))
    {
        bAutoConvert = Params->GetBoolField(TEXT("auto_convert"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Get target graph (defaults to EventGraph if graph_name not specified)
    FString TargetGraphName;
    Params->TryGetStringField(TEXT("graph_name"), TargetGraphName);
    UEdGraph* EventGraph = TargetGraphName.IsEmpty()
        ? FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint)
        : FUnrealMCPCommonUtils::FindGraphByName(Blueprint, TargetGraphName);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TargetGraphName.IsEmpty()
                ? FString(TEXT("Failed to get event graph"))
                : FString::Printf(TEXT("Graph not found: %s"), *TargetGraphName));
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
        if (Node->NodeGuid.ToString() == TargetNodeId)
        {
            TargetNode = Node;
        }
    }

    if (!SourceNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Source node not found: %s"), *SourceNodeId));
    }
    if (!TargetNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Target node not found: %s"), *TargetNodeId));
    }

    // Find the pins
    UEdGraphPin* SourcePin = nullptr;
    UEdGraphPin* TargetPin = nullptr;

    for (UEdGraphPin* Pin : SourceNode->Pins)
    {
        if (Pin->PinName.ToString().Equals(SourcePinName, ESearchCase::IgnoreCase))
        {
            SourcePin = Pin;
            break;
        }
    }

    for (UEdGraphPin* Pin : TargetNode->Pins)
    {
        if (Pin->PinName.ToString().Equals(TargetPinName, ESearchCase::IgnoreCase))
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

    // Get the schema for smart connection
    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

    // Try direct connection first
    bool bSuccess = Schema->TryCreateConnection(SourcePin, TargetPin);
    bool bUsedConversion = false;

    // If failed and auto_convert enabled, try with conversion node
    if (!bSuccess && bAutoConvert)
    {
        bSuccess = Schema->CreateAutomaticConversionNodeAndConnections(SourcePin, TargetPin);
        if (bSuccess)
        {
            bUsedConversion = true;
            UE_LOG(LogTemp, Display, TEXT("Created automatic conversion between %s and %s"),
                   *SourcePinName, *TargetPinName);
        }
    }

    if (bSuccess)
    {
        // Mark the blueprint as modified
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        bool bSave = true;
        if (Params->HasField(TEXT("save")))
        {
            bSave = Params->GetBoolField(TEXT("save"));
        }
        if (bSave && Blueprint)
        {
            FString SavePath = Blueprint->GetOutermost()->GetName();
            UEditorAssetLibrary::SaveAsset(SavePath, false);
        }

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetBoolField(TEXT("success"), true);
        ResultObj->SetBoolField(TEXT("used_conversion"), bUsedConversion);
        ResultObj->SetStringField(TEXT("source_pin"), SourcePinName);
        ResultObj->SetStringField(TEXT("target_pin"), TargetPinName);
        return ResultObj;
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
        TEXT("Failed to connect pins: %s -> %s. Types may be incompatible."),
        *SourcePinName, *TargetPinName));
}

// HandleCreateCustomEnum and HandleCreateCustomStruct removed.
#if 0
    // Dead code block — original struct handler body follows, excluded from compilation.
    FString StructName;
    FString Path = TEXT("/Game/Structs");

    // Create the package path
    FString PackagePath = Path / StructName;
    FString PackageFileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());

    // Check if the struct already exists
    if (FPackageName::DoesPackageExist(PackagePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Struct '%s' already exists at path '%s'"), *StructName, *Path));
    }

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Create the package
    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package"));
    }

    // Create the struct using FStructureEditorUtils
    UUserDefinedStruct* NewStruct = FStructureEditorUtils::CreateUserDefinedStruct(
        Package, *StructName, RF_Public | RF_Transactional | RF_Standalone
    );

    if (!NewStruct)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create struct object"));
    }

    // Add members to the struct
    TArray<FString> AddedMembers;

    for (const TSharedPtr<FJsonValue>& MemberValue : *MembersArray)
    {
        const TSharedPtr<FJsonObject>* MemberObj;
        if (!MemberValue->TryGetObject(MemberObj))
        {
            UE_LOG(LogTemp, Warning, TEXT("Skipping invalid member (not an object)"));
            continue;
        }

        FString MemberName;
        FString MemberType;

        if (!(*MemberObj)->TryGetStringField(TEXT("name"), MemberName))
        {
            UE_LOG(LogTemp, Warning, TEXT("Skipping member without 'name' field"));
            continue;
        }

        if (!(*MemberObj)->TryGetStringField(TEXT("type"), MemberType))
        {
            UE_LOG(LogTemp, Warning, TEXT("Skipping member '%s' without 'type' field"), *MemberName);
            continue;
        }

        // Determine the pin type based on the member type
        FEdGraphPinType PinType;

        if (MemberType == TEXT("Boolean") || MemberType == TEXT("Bool"))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        }
        else if (MemberType == TEXT("Integer") || MemberType == TEXT("Int"))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
        }
        else if (MemberType == TEXT("Float"))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Float;
        }
        else if (MemberType == TEXT("Double"))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Double;
        }
        else if (MemberType == TEXT("String"))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_String;
        }
        else if (MemberType == TEXT("Name"))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
        }
        else if (MemberType == TEXT("Text"))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Text;
        }
        else if (MemberType == TEXT("Vector"))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        }
        else if (MemberType == TEXT("Rotator"))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
        }
        else if (MemberType == TEXT("Transform"))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            PinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
        }
        else if (MemberType == TEXT("Color") || MemberType == TEXT("LinearColor"))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            PinType.PinSubCategoryObject = TBaseStructure<FLinearColor>::Get();
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Unsupported member type '%s' for member '%s', defaulting to Float"), *MemberType, *MemberName);
            PinType.PinCategory = UEdGraphSchema_K2::PC_Float;
        }

        // Check for container type (Array, Set, Map)
        FString ContainerType;
        if ((*MemberObj)->TryGetStringField(TEXT("container_type"), ContainerType))
        {
            if (ContainerType == TEXT("Array"))
            {
                PinType.ContainerType = EPinContainerType::Array;
            }
            else if (ContainerType == TEXT("Set"))
            {
                PinType.ContainerType = EPinContainerType::Set;
            }
            else if (ContainerType == TEXT("Map"))
            {
                PinType.ContainerType = EPinContainerType::Map;
            }
        }

        // Count existing variables before adding
        int32 VarCountBefore = 0;
        for (TFieldIterator<FProperty> It(NewStruct); It; ++It)
        {
            VarCountBefore++;
        }

        // Add the variable - returns bool in UE 5.7
        bool bAddedVar = FStructureEditorUtils::AddVariable(NewStruct, PinType);

        if (bAddedVar)
        {
            // Find the newly added property (the one that wasn't there before)
            int32 CurrentIndex = 0;
            for (TFieldIterator<FProperty> It(NewStruct); It; ++It)
            {
                if (CurrentIndex >= VarCountBefore)
                {
                    // This is a newly added property - get its GUID for renaming
                    FProperty* NewProperty = *It;

                    // GetGuidFromPropertyName returns FGuid directly in UE 5.7
                    FGuid PropertyGuid = FStructureEditorUtils::GetGuidFromPropertyName(NewProperty->GetFName());
                    if (PropertyGuid.IsValid())
                    {
                        FStructureEditorUtils::RenameVariable(NewStruct, PropertyGuid, MemberName);
                        AddedMembers.Add(MemberName);
                        UE_LOG(LogTemp, Display, TEXT("Added struct member '%s' of type '%s'"), *MemberName, *MemberType);
                    }
                    else
                    {
                        // Fallback: just track that we added it with the default name
                        AddedMembers.Add(NewProperty->GetName());
                        UE_LOG(LogTemp, Display, TEXT("Added struct member '%s' (could not rename to '%s')"), *NewProperty->GetName(), *MemberName);
                    }
                    break;
                }
                CurrentIndex++;
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Failed to add struct member '%s'"), *MemberName);
        }
    }

    // Notify that the struct has changed
    FStructureEditorUtils::OnStructureChanged(NewStruct, FStructureEditorUtils::EStructureEditorChangeInfo::AddedVariable);

    // Register with asset registry
    FAssetRegistryModule::AssetCreated(NewStruct);

    // Mark package dirty and save
    Package->MarkPackageDirty();

    bool bSave = true;
    if (Params->HasField(TEXT("save")))
    {
        bSave = Params->GetBoolField(TEXT("save"));
    }
    if (bSave)
    {
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        UPackage::SavePackage(Package, NewStruct, *PackageFileName, SaveArgs);
    }

    // Build success response
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("name"), StructName);
    ResultObj->SetStringField(TEXT("path"), PackagePath);

    TArray<TSharedPtr<FJsonValue>> MembersResult;
    for (const FString& Member : AddedMembers)
    {
        MembersResult.Add(MakeShared<FJsonValueString>(Member));
    }
    ResultObj->SetArrayField(TEXT("members"), MembersResult);

    UE_LOG(LogTemp, Display, TEXT("Created custom struct '%s' with %d members at '%s'"), *StructName, AddedMembers.Num(), *PackagePath);

    return ResultObj;
}
#endif

// Helper: Convert FEdGraphPinType to human-readable type string
static FString PinTypeToString(const FEdGraphPinType& PinType)
{
    FString TypeStr = PinType.PinCategory.ToString();

    if (PinType.PinSubCategoryObject.IsValid())
    {
        TypeStr += TEXT(":") + PinType.PinSubCategoryObject->GetName();
    }
    else if (!PinType.PinSubCategory.IsNone())
    {
        TypeStr += TEXT(":") + PinType.PinSubCategory.ToString();
    }

    if (PinType.ContainerType == EPinContainerType::Array)
    {
        TypeStr = TEXT("TArray<") + TypeStr + TEXT(">");
    }
    else if (PinType.ContainerType == EPinContainerType::Set)
    {
        TypeStr = TEXT("TSet<") + TypeStr + TEXT(">");
    }
    else if (PinType.ContainerType == EPinContainerType::Map)
    {
        TypeStr = TEXT("TMap<") + TypeStr + TEXT(">");
    }

    if (PinType.bIsReference)
    {
        TypeStr += TEXT("&");
    }

    return TypeStr;
}

// Helper: Serialize FBPVariableDescription to JSON
static TSharedPtr<FJsonObject> VariableDescToJson(const FBPVariableDescription& VarDesc)
{
    TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
    VarObj->SetStringField(TEXT("name"), VarDesc.VarName.ToString());
    VarObj->SetStringField(TEXT("type"), PinTypeToString(VarDesc.VarType));
    VarObj->SetStringField(TEXT("default_value"), VarDesc.DefaultValue);
    VarObj->SetStringField(TEXT("category"), VarDesc.Category.ToString());
    VarObj->SetBoolField(TEXT("is_instance_editable"), VarDesc.PropertyFlags & CPF_Edit ? true : false);
    VarObj->SetBoolField(TEXT("is_replicated"), VarDesc.PropertyFlags & CPF_Net ? true : false);
    VarObj->SetStringField(TEXT("replication_condition"), VarDesc.RepNotifyFunc.IsNone() ? TEXT("") : VarDesc.RepNotifyFunc.ToString());
    VarObj->SetStringField(TEXT("friendly_name"), VarDesc.FriendlyName);
    return VarObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleReadBlueprintGraph(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString GraphFilter;
    Params->TryGetStringField(TEXT("graph_name"), GraphFilter);

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();

    // === Blueprint metadata ===
    ResultObj->SetStringField(TEXT("blueprint_name"), Blueprint->GetName());
    ResultObj->SetStringField(TEXT("blueprint_class"), Blueprint->GetClass()->GetName());
    ResultObj->SetStringField(TEXT("parent_class"), Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("None"));

    // === Interfaces ===
    TArray<TSharedPtr<FJsonValue>> InterfacesArray;
    for (const FBPInterfaceDescription& Interface : Blueprint->ImplementedInterfaces)
    {
        if (Interface.Interface)
        {
            InterfacesArray.Add(MakeShared<FJsonValueString>(Interface.Interface->GetName()));
        }
    }
    ResultObj->SetArrayField(TEXT("interfaces"), InterfacesArray);

    // === Member variables ===
    TArray<TSharedPtr<FJsonValue>> VariablesArray;
    for (const FBPVariableDescription& VarDesc : Blueprint->NewVariables)
    {
        VariablesArray.Add(MakeShared<FJsonValueObject>(VariableDescToJson(VarDesc)));
    }
    ResultObj->SetArrayField(TEXT("variables"), VariablesArray);

    // === Collect graphs ===
    struct FGraphInfo
    {
        UEdGraph* Graph;
        FString Type;
    };
    TArray<FGraphInfo> AllGraphs;

    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph)
        {
            AllGraphs.Add({Graph, TEXT("Ubergraph")});
        }
    }
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (Graph)
        {
            AllGraphs.Add({Graph, TEXT("Function")});
        }
    }
    for (UEdGraph* Graph : Blueprint->MacroGraphs)
    {
        if (Graph)
        {
            AllGraphs.Add({Graph, TEXT("Macro")});
        }
    }

    // === Serialize graphs ===
    TArray<TSharedPtr<FJsonValue>> GraphsArray;

    for (const FGraphInfo& GraphInfo : AllGraphs)
    {
        UEdGraph* Graph = GraphInfo.Graph;
        const FString GraphName = Graph->GetName();

        // Apply graph filter if specified
        if (!GraphFilter.IsEmpty() && !GraphName.Equals(GraphFilter, ESearchCase::IgnoreCase))
        {
            continue;
        }

        TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
        GraphObj->SetStringField(TEXT("graph_name"), GraphName);
        GraphObj->SetStringField(TEXT("graph_type"), GraphInfo.Type);

        // Build node index map for this graph
        TMap<FGuid, int32> NodeIndexMap;
        const TArray<UEdGraphNode*>& Nodes = Graph->Nodes;
        for (int32 i = 0; i < Nodes.Num(); ++i)
        {
            if (Nodes[i])
            {
                NodeIndexMap.Add(Nodes[i]->NodeGuid, i);
            }
        }

        // Serialize nodes
        TArray<TSharedPtr<FJsonValue>> NodesArray;
        for (int32 NodeIdx = 0; NodeIdx < Nodes.Num(); ++NodeIdx)
        {
            UEdGraphNode* Node = Nodes[NodeIdx];
            if (!Node)
            {
                continue;
            }

            TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
            NodeObj->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
            NodeObj->SetNumberField(TEXT("node_index"), NodeIdx);
            NodeObj->SetStringField(TEXT("node_type"), Node->GetClass()->GetName());
            NodeObj->SetStringField(TEXT("node_title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

            if (!Node->NodeComment.IsEmpty())
            {
                NodeObj->SetStringField(TEXT("node_comment"), Node->NodeComment);
            }

            // Serialize pins
            TArray<TSharedPtr<FJsonValue>> PinsArray;
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin || Pin->bHidden)
                {
                    continue;
                }

                TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
                PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
                PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
                PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());

                // Sub-type (class/struct name)
                if (Pin->PinType.PinSubCategoryObject.IsValid())
                {
                    PinObj->SetStringField(TEXT("sub_type"), Pin->PinType.PinSubCategoryObject->GetName());
                }

                // Default value
                if (!Pin->DefaultValue.IsEmpty())
                {
                    PinObj->SetStringField(TEXT("default_value"), Pin->DefaultValue);
                }
                else if (Pin->DefaultObject)
                {
                    PinObj->SetStringField(TEXT("default_value"), Pin->DefaultObject->GetPathName());
                }

                // Connections
                if (Pin->LinkedTo.Num() > 0)
                {
                    TArray<TSharedPtr<FJsonValue>> ConnectionsArray;
                    for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                    {
                        if (!LinkedPin || !LinkedPin->GetOwningNode())
                        {
                            continue;
                        }

                        TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
                        const int32* TargetIdx = NodeIndexMap.Find(LinkedPin->GetOwningNode()->NodeGuid);
                        if (TargetIdx)
                        {
                            ConnObj->SetNumberField(TEXT("node_index"), *TargetIdx);
                        }
                        else
                        {
                            ConnObj->SetStringField(TEXT("node_id"), LinkedPin->GetOwningNode()->NodeGuid.ToString());
                        }
                        ConnObj->SetStringField(TEXT("pin_name"), LinkedPin->PinName.ToString());
                        ConnectionsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
                    }
                    PinObj->SetArrayField(TEXT("connected_to"), ConnectionsArray);
                }

                PinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
            }
            NodeObj->SetArrayField(TEXT("pins"), PinsArray);

            NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
        }
        GraphObj->SetArrayField(TEXT("nodes"), NodesArray);

        // Local variables (from function entry node)
        TArray<TSharedPtr<FJsonValue>> LocalVarsArray;
        for (UEdGraphNode* Node : Nodes)
        {
            UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node);
            if (EntryNode)
            {
                for (const FBPVariableDescription& LocalVar : EntryNode->LocalVariables)
                {
                    LocalVarsArray.Add(MakeShared<FJsonValueObject>(VariableDescToJson(LocalVar)));
                }
                break;
            }
        }
        GraphObj->SetArrayField(TEXT("local_variables"), LocalVarsArray);

        GraphsArray.Add(MakeShared<FJsonValueObject>(GraphObj));
    }

    ResultObj->SetArrayField(TEXT("graphs"), GraphsArray);

    // Add compact pseudocode representation
    FString GraphPseudocode = BuildBlueprintPseudocode(Blueprint);
    if (!GraphPseudocode.IsEmpty())
    {
        ResultObj->SetStringField(TEXT("graph"), GraphPseudocode);
    }

    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleCreateBlueprintFunction(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString FunctionName;
    if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Check if function already exists
    for (UEdGraph* ExistingGraph : Blueprint->FunctionGraphs)
    {
        if (ExistingGraph && ExistingGraph->GetName() == FunctionName)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Function '%s' already exists in blueprint '%s'"), *FunctionName, *BlueprintName));
        }
    }

    // Create new function graph
    UEdGraph* FuncGraph = FBlueprintEditorUtils::CreateNewGraph(
        Blueprint,
        FName(*FunctionName),
        UEdGraph::StaticClass(),
        UEdGraphSchema_K2::StaticClass()
    );

    if (!FuncGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create function graph"));
    }

    FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, FuncGraph, /*bIsUserCreated=*/true, nullptr);

    // Find the entry node that AddFunctionGraph already created
    UK2Node_FunctionEntry* EntryNode = nullptr;
    for (UEdGraphNode* GNode : FuncGraph->Nodes)
    {
        EntryNode = Cast<UK2Node_FunctionEntry>(GNode);
        if (EntryNode) break;
    }

    if (!EntryNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to find entry node in created function graph"));
    }

    // Set function flags
    bool bIsPure = false;
    if (Params->HasField(TEXT("is_pure")))
    {
        bIsPure = Params->GetBoolField(TEXT("is_pure"));
    }
    if (bIsPure)
    {
        EntryNode->AddExtraFlags(FUNC_BlueprintPure);
    }

    FString Access = TEXT("Public");
    Params->TryGetStringField(TEXT("access"), Access);
    if (Access.Equals(TEXT("Protected"), ESearchCase::IgnoreCase))
    {
        EntryNode->AddExtraFlags(FUNC_Protected);
    }
    else if (Access.Equals(TEXT("Private"), ESearchCase::IgnoreCase))
    {
        EntryNode->AddExtraFlags(FUNC_Private);
    }
    else
    {
        EntryNode->AddExtraFlags(FUNC_Public);
    }

    // Helper lambda to convert type string to FEdGraphPinType
    auto StringToPinType = [](const FString& TypeStr) -> FEdGraphPinType
    {
        FEdGraphPinType PinType;
        if (TypeStr.Equals(TEXT("Boolean"), ESearchCase::IgnoreCase) || TypeStr.Equals(TEXT("Bool"), ESearchCase::IgnoreCase))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        }
        else if (TypeStr.Equals(TEXT("Integer"), ESearchCase::IgnoreCase) || TypeStr.Equals(TEXT("Int"), ESearchCase::IgnoreCase))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
        }
        else if (TypeStr.Equals(TEXT("Float"), ESearchCase::IgnoreCase) || TypeStr.Equals(TEXT("Real"), ESearchCase::IgnoreCase) || TypeStr.Equals(TEXT("Double"), ESearchCase::IgnoreCase))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
            PinType.PinSubCategory = TEXT("double");
        }
        else if (TypeStr.Equals(TEXT("String"), ESearchCase::IgnoreCase))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_String;
        }
        else if (TypeStr.Equals(TEXT("Name"), ESearchCase::IgnoreCase))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
        }
        else if (TypeStr.Equals(TEXT("Text"), ESearchCase::IgnoreCase))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Text;
        }
        else if (TypeStr.Equals(TEXT("Vector"), ESearchCase::IgnoreCase))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        }
        else if (TypeStr.Equals(TEXT("Rotator"), ESearchCase::IgnoreCase))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
        }
        else if (TypeStr.Equals(TEXT("Transform"), ESearchCase::IgnoreCase))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            PinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
        }
        else if (TypeStr.Equals(TEXT("Object"), ESearchCase::IgnoreCase))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
            PinType.PinSubCategoryObject = UObject::StaticClass();
        }
        else
        {
            // Default to wildcard for unknown types
            PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
        }
        return PinType;
    };

    // Add input parameters
    const TArray<TSharedPtr<FJsonValue>>* InputsArray;
    if (Params->TryGetArrayField(TEXT("inputs"), InputsArray))
    {
        for (const TSharedPtr<FJsonValue>& InputVal : *InputsArray)
        {
            const TSharedPtr<FJsonObject>& InputObj = InputVal->AsObject();
            if (!InputObj) continue;

            FString ParamName, ParamType;
            InputObj->TryGetStringField(TEXT("name"), ParamName);
            InputObj->TryGetStringField(TEXT("type"), ParamType);

            if (!ParamName.IsEmpty() && !ParamType.IsEmpty())
            {
                FEdGraphPinType PinType = StringToPinType(ParamType);
                EntryNode->CreateUserDefinedPin(FName(*ParamName), PinType, EGPD_Output);
            }
        }
    }

    // Find or create result node for outputs
    UK2Node_FunctionResult* ResultNode = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* OutputsArray;
    if (Params->TryGetArrayField(TEXT("outputs"), OutputsArray) && OutputsArray->Num() > 0)
    {
        // Find existing result node (AddFunctionGraph may have created one)
        for (UEdGraphNode* GNode : FuncGraph->Nodes)
        {
            ResultNode = Cast<UK2Node_FunctionResult>(GNode);
            if (ResultNode) break;
        }

        // Create one if not found
        if (!ResultNode)
        {
            ResultNode = NewObject<UK2Node_FunctionResult>(FuncGraph);
            FuncGraph->AddNode(ResultNode, false, false);
            ResultNode->NodePosX = 400;
            ResultNode->NodePosY = 0;
            ResultNode->AllocateDefaultPins();
        }

        for (const TSharedPtr<FJsonValue>& OutputVal : *OutputsArray)
        {
            const TSharedPtr<FJsonObject>& OutputObj = OutputVal->AsObject();
            if (!OutputObj) continue;

            FString ParamName, ParamType;
            OutputObj->TryGetStringField(TEXT("name"), ParamName);
            OutputObj->TryGetStringField(TEXT("type"), ParamType);

            if (!ParamName.IsEmpty() && !ParamType.IsEmpty())
            {
                FEdGraphPinType PinType = StringToPinType(ParamType);
                ResultNode->CreateUserDefinedPin(FName(*ParamName), PinType, EGPD_Input);
            }
        }
    }

    // Mark modified and compile
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    // Save
    bool bSave = true;
    if (Params->HasField(TEXT("save")))
    {
        bSave = Params->GetBoolField(TEXT("save"));
    }
    if (bSave)
    {
        FString SavePath = Blueprint->GetOutermost()->GetName();
        UEditorAssetLibrary::SaveAsset(SavePath, false);
    }

    // Build response
    TSharedPtr<FJsonObject> ResponseObj = MakeShared<FJsonObject>();
    ResponseObj->SetBoolField(TEXT("success"), true);
    ResponseObj->SetStringField(TEXT("function_name"), FunctionName);
    ResponseObj->SetStringField(TEXT("graph_name"), FuncGraph->GetName());
    ResponseObj->SetStringField(TEXT("entry_node_id"), EntryNode->NodeGuid.ToString());
    if (ResultNode)
    {
        ResponseObj->SetStringField(TEXT("result_node_id"), ResultNode->NodeGuid.ToString());
    }

    UE_LOG(LogTemp, Display, TEXT("Created function '%s' in blueprint '%s'"), *FunctionName, *BlueprintName);

    return ResponseObj;
}

// ---------------------------------------------------------------------------
// delete_blueprint_node
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleDeleteBlueprintNode(const TSharedPtr<FJsonObject>& Params)
{
    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString NodeId;
    if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id' parameter"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Determine which graph to search
    FString GraphName;
    Params->TryGetStringField(TEXT("graph_name"), GraphName);

    // Collect candidate graphs
    TArray<UEdGraph*> CandidateGraphs;
    if (!GraphName.IsEmpty())
    {
        UEdGraph* Graph = FUnrealMCPCommonUtils::FindGraphByName(Blueprint, GraphName);
        if (!Graph)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Graph not found: %s"), *GraphName));
        }
        CandidateGraphs.Add(Graph);
    }
    else
    {
        for (UEdGraph* G : Blueprint->UbergraphPages)  { if (G) CandidateGraphs.Add(G); }
        for (UEdGraph* G : Blueprint->FunctionGraphs)   { if (G) CandidateGraphs.Add(G); }
        for (UEdGraph* G : Blueprint->MacroGraphs)       { if (G) CandidateGraphs.Add(G); }
    }

    // Find the node by GUID
    UEdGraphNode* FoundNode = nullptr;
    for (UEdGraph* Graph : CandidateGraphs)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node && Node->NodeGuid.ToString() == NodeId)
            {
                FoundNode = Node;
                break;
            }
        }
        if (FoundNode) break;
    }

    if (!FoundNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Node not found: %s"), *NodeId));
    }

    FBlueprintEditorUtils::RemoveNode(Blueprint, FoundNode);
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    bool bSave = true;
    if (Params->HasField(TEXT("save")))
    {
        bSave = Params->GetBoolField(TEXT("save"));
    }
    if (bSave)
    {
        FString SavePath = Blueprint->GetOutermost()->GetName();
        UEditorAssetLibrary::SaveAsset(SavePath, false);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("removed_node_id"), NodeId);
    return ResultObj;
}

// ---------------------------------------------------------------------------
// disconnect_blueprint_pin
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleDisconnectBlueprintPin(const TSharedPtr<FJsonObject>& Params)
{
    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString NodeId;
    if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id' parameter"));
    }

    FString PinName;
    if (!Params->TryGetStringField(TEXT("pin_name"), PinName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pin_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Determine which graph to search
    FString GraphName;
    Params->TryGetStringField(TEXT("graph_name"), GraphName);

    TArray<UEdGraph*> CandidateGraphs;
    if (!GraphName.IsEmpty())
    {
        UEdGraph* Graph = FUnrealMCPCommonUtils::FindGraphByName(Blueprint, GraphName);
        if (!Graph)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Graph not found: %s"), *GraphName));
        }
        CandidateGraphs.Add(Graph);
    }
    else
    {
        for (UEdGraph* G : Blueprint->UbergraphPages)  { if (G) CandidateGraphs.Add(G); }
        for (UEdGraph* G : Blueprint->FunctionGraphs)   { if (G) CandidateGraphs.Add(G); }
        for (UEdGraph* G : Blueprint->MacroGraphs)       { if (G) CandidateGraphs.Add(G); }
    }

    // Find the node by GUID
    UEdGraphNode* FoundNode = nullptr;
    for (UEdGraph* Graph : CandidateGraphs)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node && Node->NodeGuid.ToString() == NodeId)
            {
                FoundNode = Node;
                break;
            }
        }
        if (FoundNode) break;
    }

    if (!FoundNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Node not found: %s"), *NodeId));
    }

    // Find the pin (direction agnostic — EGPD_MAX means any direction)
    UEdGraphPin* Pin = FUnrealMCPCommonUtils::FindPin(FoundNode, PinName);
    if (!Pin)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Pin not found: %s"), *PinName));
    }

    int32 BrokenCount = 0;

    // Check if a specific target is specified
    FString TargetNodeId, TargetPinName;
    Params->TryGetStringField(TEXT("target_node_id"), TargetNodeId);
    Params->TryGetStringField(TEXT("target_pin_name"), TargetPinName);

    if (TargetNodeId.IsEmpty() != TargetPinName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Both 'target_node_id' and 'target_pin_name' must be provided together, or neither"));
    }

    if (!TargetNodeId.IsEmpty() && !TargetPinName.IsEmpty())
    {
        // Find the target node across the same candidate graphs
        UEdGraphNode* TargetNode = nullptr;
        for (UEdGraph* Graph : CandidateGraphs)
        {
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (Node && Node->NodeGuid.ToString() == TargetNodeId)
                {
                    TargetNode = Node;
                    break;
                }
            }
            if (TargetNode) break;
        }

        if (!TargetNode)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Target node not found: %s"), *TargetNodeId));
        }

        UEdGraphPin* TargetPin = FUnrealMCPCommonUtils::FindPin(TargetNode, TargetPinName);
        if (!TargetPin)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Target pin not found: %s"), *TargetPinName));
        }

        if (Pin->LinkedTo.Contains(TargetPin))
        {
            Pin->BreakLinkTo(TargetPin);
            BrokenCount = 1;
        }
    }
    else
    {
        BrokenCount = Pin->LinkedTo.Num();
        Pin->BreakAllPinLinks();
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    bool bSave = true;
    if (Params->HasField(TEXT("save")))
    {
        bSave = Params->GetBoolField(TEXT("save"));
    }
    if (bSave)
    {
        FString SavePath = Blueprint->GetOutermost()->GetName();
        UEditorAssetLibrary::SaveAsset(SavePath, false);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetNumberField(TEXT("connections_broken"), BrokenCount);
    return ResultObj;
}

// ---------------------------------------------------------------------------
// set_pin_default_value
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleSetPinDefaultValue(const TSharedPtr<FJsonObject>& Params)
{
    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString NodeId;
    if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id' parameter"));
    }

    FString PinName;
    if (!Params->TryGetStringField(TEXT("pin_name"), PinName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pin_name' parameter"));
    }

    FString Value;
    if (!Params->TryGetStringField(TEXT("value"), Value))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' parameter"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Determine which graph to search
    FString GraphName;
    Params->TryGetStringField(TEXT("graph_name"), GraphName);

    TArray<UEdGraph*> CandidateGraphs;
    if (!GraphName.IsEmpty())
    {
        UEdGraph* Graph = FUnrealMCPCommonUtils::FindGraphByName(Blueprint, GraphName);
        if (!Graph)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Graph not found: %s"), *GraphName));
        }
        CandidateGraphs.Add(Graph);
    }
    else
    {
        for (UEdGraph* G : Blueprint->UbergraphPages)  { if (G) CandidateGraphs.Add(G); }
        for (UEdGraph* G : Blueprint->FunctionGraphs)   { if (G) CandidateGraphs.Add(G); }
        for (UEdGraph* G : Blueprint->MacroGraphs)       { if (G) CandidateGraphs.Add(G); }
    }

    // Find the node by GUID
    UEdGraphNode* FoundNode = nullptr;
    UEdGraph* OwningGraph = nullptr;
    for (UEdGraph* Graph : CandidateGraphs)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node && Node->NodeGuid.ToString() == NodeId)
            {
                FoundNode = Node;
                OwningGraph = Graph;
                break;
            }
        }
        if (FoundNode) break;
    }

    if (!FoundNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Node not found: %s"), *NodeId));
    }

    // Find the input pin
    UEdGraphPin* Pin = FUnrealMCPCommonUtils::FindPin(FoundNode, PinName, EGPD_Input);
    if (!Pin)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Input pin not found: %s"), *PinName));
    }

    // Use the K2 schema to set the default value
    const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(OwningGraph->GetSchema());
    if (!Schema)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get K2 schema for graph"));
    }

    Schema->TrySetDefaultValue(*Pin, Value);

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    bool bSave = true;
    if (Params->HasField(TEXT("save")))
    {
        bSave = Params->GetBoolField(TEXT("save"));
    }
    if (bSave)
    {
        FString SavePath = Blueprint->GetOutermost()->GetName();
        UEditorAssetLibrary::SaveAsset(SavePath, false);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("pin_name"), PinName);
    ResultObj->SetStringField(TEXT("value"), Value);
    return ResultObj;
}

// ---------------------------------------------------------------------------
// remove_blueprint_variable
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleRemoveBlueprintVariable(const TSharedPtr<FJsonObject>& Params)
{
    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString VariableName;
    if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'variable_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Verify the variable exists
    bool bFound = false;
    for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        if (Variable.VarName == FName(*VariableName))
        {
            bFound = true;
            break;
        }
    }

    if (!bFound)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Variable not found: %s"), *VariableName));
    }

    FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, FName(*VariableName));

    bool bSave = true;
    if (Params->HasField(TEXT("save")))
    {
        bSave = Params->GetBoolField(TEXT("save"));
    }
    if (bSave)
    {
        FString SavePath = Blueprint->GetOutermost()->GetName();
        UEditorAssetLibrary::SaveAsset(SavePath, false);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("variable_name"), VariableName);
    return ResultObj;
}

// ---------------------------------------------------------------------------
// set_blueprint_variable_defaults
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleSetBlueprintVariableDefaults(const TSharedPtr<FJsonObject>& Params)
{
    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString VariableName;
    if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'variable_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Find the variable description
    FBPVariableDescription* Var = nullptr;
    for (FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        if (Variable.VarName == FName(*VariableName))
        {
            Var = &Variable;
            break;
        }
    }

    if (!Var)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Variable not found: %s"), *VariableName));
    }

    TArray<TSharedPtr<FJsonValue>> ModifiedFields;

    // default_value
    FString DefaultValue;
    if (Params->TryGetStringField(TEXT("default_value"), DefaultValue))
    {
        Var->DefaultValue = DefaultValue;
        ModifiedFields.Add(MakeShared<FJsonValueString>(TEXT("default_value")));
    }

    // is_instance_editable
    if (Params->HasField(TEXT("is_instance_editable")))
    {
        bool bEditable = Params->GetBoolField(TEXT("is_instance_editable"));
        if (bEditable)
        {
            Var->PropertyFlags |= CPF_Edit;
        }
        else
        {
            Var->PropertyFlags &= ~CPF_Edit;
        }
        ModifiedFields.Add(MakeShared<FJsonValueString>(TEXT("is_instance_editable")));
    }

    // is_replicated
    if (Params->HasField(TEXT("is_replicated")))
    {
        bool bReplicated = Params->GetBoolField(TEXT("is_replicated"));
        if (bReplicated)
        {
            Var->PropertyFlags |= CPF_Net;
        }
        else
        {
            Var->PropertyFlags &= ~CPF_Net;
        }
        ModifiedFields.Add(MakeShared<FJsonValueString>(TEXT("is_replicated")));
    }

    // rep_notify_func
    FString RepNotifyFunc;
    if (Params->TryGetStringField(TEXT("rep_notify_func"), RepNotifyFunc))
    {
        FBlueprintEditorUtils::SetBlueprintVariableRepNotifyFunc(Blueprint, FName(*VariableName), FName(*RepNotifyFunc));
        ModifiedFields.Add(MakeShared<FJsonValueString>(TEXT("rep_notify_func")));
    }

    // category
    FString Category;
    if (Params->TryGetStringField(TEXT("category"), Category))
    {
        Var->Category = FText::FromString(Category);
        ModifiedFields.Add(MakeShared<FJsonValueString>(TEXT("category")));
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    bool bSave = true;
    if (Params->HasField(TEXT("save")))
    {
        bSave = Params->GetBoolField(TEXT("save"));
    }
    if (bSave)
    {
        FString SavePath = Blueprint->GetOutermost()->GetName();
        UEditorAssetLibrary::SaveAsset(SavePath, false);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("variable_name"), VariableName);
    ResultObj->SetArrayField(TEXT("modified_fields"), ModifiedFields);
    return ResultObj;
}

// ---------------------------------------------------------------------------
// remove_blueprint_graph
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleRemoveBlueprintGraph(const TSharedPtr<FJsonObject>& Params)
{
    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString GraphName;
    if (!Params->TryGetStringField(TEXT("graph_name"), GraphName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'graph_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Reject EventGraph deletion early (before FindGraphByName which may create it)
    if (GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Cannot delete UbergraphPage: EventGraph"));
    }

    // Find the graph
    UEdGraph* Graph = FUnrealMCPCommonUtils::FindGraphByName(Blueprint, GraphName);
    if (!Graph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Graph not found: %s"), *GraphName));
    }

    // Reject deletion of any UbergraphPages
    for (UEdGraph* UberGraph : Blueprint->UbergraphPages)
    {
        if (UberGraph == Graph)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Cannot delete UbergraphPage: %s"), *GraphName));
        }
    }

    FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    bool bSave = true;
    if (Params->HasField(TEXT("save")))
    {
        bSave = Params->GetBoolField(TEXT("save"));
    }
    if (bSave)
    {
        FString SavePath = Blueprint->GetOutermost()->GetName();
        UEditorAssetLibrary::SaveAsset(SavePath, false);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("graph_name"), GraphName);
    return ResultObj;
}

// ---------------------------------------------------------------------------
// Blueprint Graph Pseudocode
// ---------------------------------------------------------------------------

FString FUnrealMCPBlueprintNodeCommands::ShortenNodeTitle(const FString& Title)
{
    FString Short = Title;
    Short.RemoveFromStart(TEXT("Event "));
    Short.RemoveFromStart(TEXT("EnhancedInputAction "));
    Short.TrimStartAndEndInline();
    // For multiline titles like "Queue Ability\nTarget is BP Player Character",
    // keep the first line (function name) and discard the target class line
    int32 NewlineIdx;
    if (Short.FindChar(TEXT('\n'), NewlineIdx))
    {
        Short.LeftInline(NewlineIdx);
        Short.TrimEndInline();
    }
    return Short;
}

FString FUnrealMCPBlueprintNodeCommands::FormatNodeLine(
    int32 NodeIndex, UEdGraphNode* Node, FBPGraphContext& Ctx)
{
    FString Title = ShortenNodeTitle(Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

    // Collect data input arguments
    FString Args;
    const TArray<FBPGraphContext::FDataInput>* Inputs = Ctx.DataInputs.Find(NodeIndex);
    if (Inputs)
    {
        for (const FBPGraphContext::FDataInput& Input : *Inputs)
        {
            if (Input.PinName == TEXT("self") || Input.PinName == TEXT("WorldContextObject"))
                continue;

            if (!Args.IsEmpty()) Args += TEXT(", ");

            if (Input.bConnected)
            {
                FString SourceTitle;
                if (Input.SourceNodeIndex >= 0 && Input.SourceNodeIndex < Ctx.Nodes.Num()
                    && Ctx.Nodes[Input.SourceNodeIndex])
                {
                    SourceTitle = ShortenNodeTitle(
                        Ctx.Nodes[Input.SourceNodeIndex]->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
                }
                else
                {
                    SourceTitle = TEXT("???");
                }

                FString SrcPin;
                if (!Input.SourcePinName.IsEmpty() && Input.SourcePinName != TEXT("ReturnValue"))
                {
                    SrcPin = TEXT(".") + Input.SourcePinName;
                }

                Args += FString::Printf(TEXT("%s: [%d] %s%s"),
                    *Input.PinName, Input.SourceNodeIndex, *SourceTitle, *SrcPin);
            }
            else
            {
                Args += FString::Printf(TEXT("%s: %s"), *Input.PinName, *Input.DefaultValue);
            }
        }
    }

    // For event/entry nodes with no data inputs, show output parameter names
    if (Args.IsEmpty())
    {
        bool bIsEventOrEntry = Node->IsA<UK2Node_Event>() || Node->IsA<UK2Node_FunctionEntry>();
        if (bIsEventOrEntry)
        {
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin && !Pin->bHidden
                    && Pin->Direction == EGPD_Output
                    && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec
                    && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Delegate
                    && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_MCDelegate)
                {
                    if (!Args.IsEmpty()) Args += TEXT(", ");
                    Args += Pin->PinName.ToString();
                }
            }
        }
    }

    if (Args.IsEmpty())
        return FString::Printf(TEXT("[%d] %s"), NodeIndex, *Title);
    return FString::Printf(TEXT("[%d] %s(%s)"), NodeIndex, *Title, *Args);
}

void FUnrealMCPBlueprintNodeCommands::BuildGraphAdjacency(
    const TArray<UEdGraphNode*>& Nodes, FBPGraphContext& Ctx) const
{
    Ctx.Nodes = Nodes;
    Ctx.ExecOutgoing.Empty();
    Ctx.DataInputs.Empty();
    Ctx.HasIncomingExec.Empty();
    Ctx.Visited.Empty();

    TMap<UEdGraphNode*, int32> PtrToIndex;
    for (int32 i = 0; i < Nodes.Num(); ++i)
    {
        if (Nodes[i]) PtrToIndex.Add(Nodes[i], i);
    }

    for (int32 NodeIdx = 0; NodeIdx < Nodes.Num(); ++NodeIdx)
    {
        UEdGraphNode* Node = Nodes[NodeIdx];
        if (!Node) continue;

        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin || Pin->bHidden) continue;

            const bool bIsExec = (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);

            if (bIsExec && Pin->Direction == EGPD_Output)
            {
                // Record exec output connections
                for (UEdGraphPin* Linked : Pin->LinkedTo)
                {
                    if (!Linked || !Linked->GetOwningNode()) continue;
                    const int32* TargetIdx = PtrToIndex.Find(Linked->GetOwningNode());
                    if (TargetIdx)
                    {
                        Ctx.ExecOutgoing.FindOrAdd(NodeIdx).Add(
                            {Pin->PinName.ToString(), *TargetIdx});
                        Ctx.HasIncomingExec.Add(*TargetIdx);
                    }
                }
            }
            else if (!bIsExec && Pin->Direction == EGPD_Input)
            {
                // Record data input
                FBPGraphContext::FDataInput DI;
                DI.PinName = Pin->PinName.ToString();
                DI.SourceNodeIndex = -1;
                DI.bConnected = false;

                if (Pin->LinkedTo.Num() > 0 && Pin->LinkedTo[0]
                    && Pin->LinkedTo[0]->GetOwningNode())
                {
                    const int32* SrcIdx = PtrToIndex.Find(Pin->LinkedTo[0]->GetOwningNode());
                    if (SrcIdx)
                    {
                        DI.SourceNodeIndex = *SrcIdx;
                        DI.SourcePinName = Pin->LinkedTo[0]->PinName.ToString();
                        DI.bConnected = true;
                    }
                }

                if (!DI.bConnected)
                {
                    if (!Pin->DefaultValue.IsEmpty())
                        DI.DefaultValue = Pin->DefaultValue;
                    else if (Pin->DefaultObject)
                        DI.DefaultValue = Pin->DefaultObject->GetName();
                }

                if (DI.bConnected || !DI.DefaultValue.IsEmpty())
                {
                    Ctx.DataInputs.FindOrAdd(NodeIdx).Add(DI);
                }
            }
        }
    }
}

FString FUnrealMCPBlueprintNodeCommands::FormatExecChain(
    int32 NodeIndex, int32 Depth, FBPGraphContext& Ctx) const
{
    if (Ctx.Visited.Contains(NodeIndex)) return FString();
    if (NodeIndex < 0 || NodeIndex >= Ctx.Nodes.Num()) return FString();

    UEdGraphNode* Node = Ctx.Nodes[NodeIndex];
    if (!Node) return FString();

    Ctx.Visited.Add(NodeIndex);

    FString Indent;
    for (int32 i = 0; i < Depth; ++i) Indent += TEXT("  ");

    FString NodeLine = FormatNodeLine(NodeIndex, Node, Ctx);

    FString Result;
    if (Depth == 0)
        Result = NodeLine + TEXT("\n");
    else
        Result = Indent + TEXT("-> ") + NodeLine + TEXT("\n");

    // Follow exec outputs
    const TArray<FBPGraphContext::FExecTarget>* ExecOuts = Ctx.ExecOutgoing.Find(NodeIndex);
    if (!ExecOuts || ExecOuts->Num() == 0) return Result;

    if (ExecOuts->Num() == 1)
    {
        // Single continuation
        Result += FormatExecChain((*ExecOuts)[0].TargetNodeIndex, Depth + 1, Ctx);
    }
    else
    {
        // Multiple exec outputs — label each path
        for (const FBPGraphContext::FExecTarget& ExecOut : *ExecOuts)
        {
            FString Label = ExecOut.PinName;
            if (Label == TEXT("Then")) Label = TEXT("True");
            else if (Label == TEXT("Else")) Label = TEXT("False");
            else if (Label.StartsWith(TEXT("then_")))
                Label = FString::Printf(TEXT("[%s]"), *Label.Mid(5));

            Result += Indent + TEXT("  ") + Label + TEXT(":\n");
            Result += FormatExecChain(ExecOut.TargetNodeIndex, Depth + 2, Ctx);
        }
    }

    return Result;
}

FString FUnrealMCPBlueprintNodeCommands::BuildBlueprintPseudocode(UBlueprint* Blueprint) const
{
    if (!Blueprint) return FString();

    FString Output;

    // Collect all graphs
    struct FGraphEntry { UEdGraph* Graph; FString Type; };
    TArray<FGraphEntry> AllGraphs;

    for (UEdGraph* Graph : Blueprint->UbergraphPages)
        if (Graph) AllGraphs.Add({Graph, TEXT("Ubergraph")});
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        if (Graph) AllGraphs.Add({Graph, TEXT("Function")});
    for (UEdGraph* Graph : Blueprint->MacroGraphs)
        if (Graph) AllGraphs.Add({Graph, TEXT("Macro")});

    for (const FGraphEntry& Entry : AllGraphs)
    {
        UEdGraph* Graph = Entry.Graph;

        // Build adjacency
        FBPGraphContext Ctx;
        BuildGraphAdjacency(Graph->Nodes, Ctx);

        // Find root nodes: have exec pins but no incoming exec
        TArray<int32> Roots;
        for (int32 i = 0; i < Ctx.Nodes.Num(); ++i)
        {
            UEdGraphNode* Node = Ctx.Nodes[i];
            if (!Node) continue;
            if (Ctx.HasIncomingExec.Contains(i)) continue;

            bool bHasExec = false;
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                {
                    bHasExec = true;
                    break;
                }
            }
            if (bHasExec) Roots.Add(i);
        }

        if (Roots.Num() == 0) continue;

        // Graph header
        Output += FString::Printf(TEXT("%s [%s]:\n"), *Graph->GetName(), *Entry.Type);

        // Walk each root
        for (int32 RootIdx : Roots)
        {
            FString ChainText = FormatExecChain(RootIdx, 0, Ctx);
            // Indent every line under the graph header
            TArray<FString> Lines;
            ChainText.ParseIntoArrayLines(Lines);
            for (const FString& Line : Lines)
            {
                if (!Line.IsEmpty())
                    Output += TEXT("  ") + Line + TEXT("\n");
            }
            Output += TEXT("\n");
        }
    }

    // Variables section
    if (Blueprint->NewVariables.Num() > 0)
    {
        Output += TEXT("Variables:\n");
        for (const FBPVariableDescription& Var : Blueprint->NewVariables)
        {
            FString Flags;
            if (Var.PropertyFlags & CPF_Edit) Flags += TEXT(" [editable]");
            if (Var.PropertyFlags & CPF_Net) Flags += TEXT(" [replicated]");

            FString Default;
            if (!Var.DefaultValue.IsEmpty()) Default = TEXT(" = ") + Var.DefaultValue;

            Output += FString::Printf(TEXT("  %s: %s%s%s\n"),
                *Var.VarName.ToString(),
                *PinTypeToString(Var.VarType),
                *Default,
                *Flags);
        }
    }

    Output.TrimEndInline();
    return Output;
}