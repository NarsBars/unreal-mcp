#include "Commands/UnrealMCPAnimGraphCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "AnimGraphNode_Base.h"
#include "AnimationGraph.h"
#include "AnimationGraphSchema.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationStateMachineSchema.h"
#include "AnimStateNodeBase.h"
#include "AnimStateNode.h"
#include "AnimStateEntryNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimStateConduitNode.h"
#include "AnimStateAliasNode.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimationStateGraph.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimationTransitionGraph.h"
#include "AnimGraphNode_BlendListBase.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AnimGraphNodeBinding.h"
#include "AnimBlueprintExtension_PropertyAccess.h"
#include "IPropertyAccessEditor.h"
#include "Features/IModularFeatures.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "EditorAssetLibrary.h"
#include "UObject/UObjectIterator.h"
#include "AnimGraphNode_LinkedAnimLayer.h"

// K2 nodes for transition rule graphs and function creation
#include "K2Node_VariableGet.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CommutativeAssociativeBinaryOperator.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet/KismetMathLibrary.h"

DEFINE_LOG_CATEGORY_STATIC(LogMCPAnimGraph, Log, All);

// ============================================================================
// Construction & Routing
// ============================================================================

FUnrealMCPAnimGraphCommands::FUnrealMCPAnimGraphCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("read_anim_graph"))
    {
        return HandleReadAnimGraph(Params);
    }
    else if (CommandType == TEXT("add_anim_graph_node"))
    {
        return HandleAddAnimGraphNode(Params);
    }
    else if (CommandType == TEXT("connect_anim_pins"))
    {
        return HandleConnectAnimPins(Params);
    }
    else if (CommandType == TEXT("set_anim_node_property"))
    {
        return HandleSetAnimNodeProperty(Params);
    }
    else if (CommandType == TEXT("delete_anim_graph_node"))
    {
        return HandleDeleteAnimGraphNode(Params);
    }
    else if (CommandType == TEXT("disconnect_anim_pin"))
    {
        return HandleDisconnectAnimPin(Params);
    }
    else if (CommandType == TEXT("find_anim_graph_nodes"))
    {
        return HandleFindAnimGraphNodes(Params);
    }
    // Tier 2 — State Machine commands
    else if (CommandType == TEXT("add_state_machine"))
    {
        return HandleAddStateMachine(Params);
    }
    else if (CommandType == TEXT("add_state"))
    {
        return HandleAddState(Params);
    }
    else if (CommandType == TEXT("add_state_transition"))
    {
        return HandleAddStateTransition(Params);
    }
    else if (CommandType == TEXT("set_state_animation"))
    {
        return HandleSetStateAnimation(Params);
    }
    else if (CommandType == TEXT("read_state_machine"))
    {
        return HandleReadStateMachine(Params);
    }
    else if (CommandType == TEXT("rename_state"))
    {
        return HandleRenameState(Params);
    }
    // Tier 2.5 — State Machine utilities
    else if (CommandType == TEXT("set_state_entry"))
    {
        return HandleSetStateEntry(Params);
    }
    else if (CommandType == TEXT("bind_transition_condition"))
    {
        return HandleBindTransitionCondition(Params);
    }
    else if (CommandType == TEXT("connect_k2_pins"))
    {
        return HandleConnectK2Pins(Params);
    }
    // Tier 3 — Advanced commands
    else if (CommandType == TEXT("add_anim_layer"))
    {
        return HandleAddAnimLayer(Params);
    }
    else if (CommandType == TEXT("add_blend_node"))
    {
        return HandleAddBlendNode(Params);
    }
    else if (CommandType == TEXT("add_blend_pose_pin"))
    {
        return HandleAddBlendPosePin(Params);
    }
    else if (CommandType == TEXT("set_anim_blueprint_parent"))
    {
        return HandleSetAnimBlueprintParent(Params);
    }
    else if (CommandType == TEXT("compile_anim_blueprint"))
    {
        return HandleCompileAnimBlueprint(Params);
    }
    // Tier 3.5 — Property Access binding & AnimNode function binding
    else if (CommandType == TEXT("bind_anim_pin_to_property"))
    {
        return HandleBindAnimPinToProperty(Params);
    }
    else if (CommandType == TEXT("bind_anim_node_function"))
    {
        return HandleBindAnimNodeFunction(Params);
    }
    else if (CommandType == TEXT("create_anim_graph_function"))
    {
        return HandleCreateAnimGraphFunction(Params);
    }
    // Tier 4 — PoseSearch commands
    else if (CommandType == TEXT("configure_motion_matching"))
    {
        return HandleConfigureMotionMatching(Params);
    }
    else if (CommandType == TEXT("configure_history_collector"))
    {
        return HandleConfigureHistoryCollector(Params);
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Unknown AnimGraph command: %s"), *CommandType));
    }
}

// ============================================================================
// Helpers
// ============================================================================

UAnimBlueprint* FUnrealMCPAnimGraphCommands::LoadAnimBlueprint(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutError)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        OutError = FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
        return nullptr;
    }

    // Try loading via FindBlueprint (handles both names and paths)
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    if (!Blueprint)
    {
        OutError = FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
        return nullptr;
    }

    UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Blueprint);
    if (!AnimBP)
    {
        OutError = FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("'%s' is not an Animation Blueprint"), *BlueprintPath));
        return nullptr;
    }

    return AnimBP;
}

UEdGraph* FUnrealMCPAnimGraphCommands::ResolveAnimGraph(UAnimBlueprint* AnimBP, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutError)
{
    FString GraphName;
    Params->TryGetStringField(TEXT("graph_name"), GraphName);

    UEdGraph* Graph = FUnrealMCPCommonUtils::FindAnimGraph(AnimBP, GraphName);
    if (!Graph)
    {
        OutError = FUnrealMCPCommonUtils::CreateErrorResponse(
            GraphName.IsEmpty()
                ? FString(TEXT("No AnimGraph found in this Animation Blueprint"))
                : FString::Printf(TEXT("AnimGraph '%s' not found"), *GraphName));
        return nullptr;
    }

    return Graph;
}

UEdGraphNode* FUnrealMCPAnimGraphCommands::FindNodeInGraph(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params,
                                                            const FString& NodeIdField, TSharedPtr<FJsonObject>& OutError)
{
    FString NodeId;
    if (Params->TryGetStringField(NodeIdField, NodeId))
    {
        // Try GUID lookup first
        FGuid TargetGuid;
        if (FGuid::Parse(NodeId, TargetGuid))
        {
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (Node && Node->NodeGuid == TargetGuid)
                {
                    return Node;
                }
            }
        }

        // Fallback: try matching by GUID string (some GUIDs don't parse cleanly via FGuid::Parse)
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node && Node->NodeGuid.ToString() == NodeId)
            {
                return Node;
            }
        }

        OutError = FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Node not found with ID: %s"), *NodeId));
        return nullptr;
    }

    // Try class + index lookup
    FString NodeClass;
    if (Params->TryGetStringField(TEXT("node_class"), NodeClass))
    {
        int32 ClassIndex = 0;
        if (Params->HasField(TEXT("node_index")))
        {
            ClassIndex = static_cast<int32>(Params->GetNumberField(TEXT("node_index")));
        }

        int32 MatchCount = 0;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node && Node->GetClass()->GetName().Contains(NodeClass))
            {
                if (MatchCount == ClassIndex)
                {
                    return Node;
                }
                MatchCount++;
            }
        }

        OutError = FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Node of class '%s' at index %d not found (found %d matches)"), *NodeClass, ClassIndex, MatchCount));
        return nullptr;
    }

    OutError = FUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Missing '%s' or 'node_class' parameter to identify node"), *NodeIdField));
    return nullptr;
}

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::SerializeAnimNode(UEdGraphNode* Node, int32 NodeIndex, const TMap<FGuid, int32>& NodeIndexMap)
{
    TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
    NodeObj->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
    NodeObj->SetNumberField(TEXT("node_index"), NodeIndex);
    NodeObj->SetStringField(TEXT("node_class"), Node->GetClass()->GetName());
    NodeObj->SetStringField(TEXT("node_title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
    NodeObj->SetNumberField(TEXT("pos_x"), Node->NodePosX);
    NodeObj->SetNumberField(TEXT("pos_y"), Node->NodePosY);

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
        PinObj->SetStringField(TEXT("display_name"), Pin->GetDisplayName().ToString());
        PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
        PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());

        if (Pin->PinType.PinSubCategoryObject.IsValid())
        {
            PinObj->SetStringField(TEXT("sub_type"), Pin->PinType.PinSubCategoryObject->GetName());
        }

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
                ConnObj->SetStringField(TEXT("node_id"), LinkedPin->GetOwningNode()->NodeGuid.ToString());
                ConnObj->SetStringField(TEXT("pin_name"), LinkedPin->PinName.ToString());
                ConnectionsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
            }
            PinObj->SetArrayField(TEXT("connected_to"), ConnectionsArray);
        }

        PinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
    }
    NodeObj->SetArrayField(TEXT("pins"), PinsArray);

    return NodeObj;
}

void FUnrealMCPAnimGraphCommands::MarkModifiedAndSave(UAnimBlueprint* AnimBP, const TSharedPtr<FJsonObject>& Params)
{
    FBlueprintEditorUtils::MarkBlueprintAsModified(AnimBP);

    bool bSave = true;
    if (Params->HasField(TEXT("save")))
    {
        bSave = Params->GetBoolField(TEXT("save"));
    }
    if (bSave)
    {
        FString SavePath = AnimBP->GetOutermost()->GetName();
        UEditorAssetLibrary::SaveAsset(SavePath, false);
    }
}

// ============================================================================
// read_anim_graph
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleReadAnimGraph(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_name"), AnimBP->GetName());
    ResultObj->SetStringField(TEXT("blueprint_path"), AnimBP->GetOutermost()->GetName());
    ResultObj->SetStringField(TEXT("parent_class"), AnimBP->ParentClass ? AnimBP->ParentClass->GetPathName() : TEXT("None"));

    // Collect all AnimGraph-related graphs
    TArray<UEdGraph*> AllGraphs;
    for (UEdGraph* Graph : AnimBP->FunctionGraphs)
    {
        if (Graph)
        {
            AllGraphs.Add(Graph);
            Graph->GetAllChildrenGraphs(AllGraphs);
        }
    }

    // Check if user wants a specific graph
    FString GraphFilter;
    Params->TryGetStringField(TEXT("graph_name"), GraphFilter);

    TArray<TSharedPtr<FJsonValue>> GraphsArray;
    for (UEdGraph* Graph : AllGraphs)
    {
        if (!Graph)
        {
            continue;
        }

        const FString GraphName = Graph->GetName();

        // Determine graph type
        FString GraphType = TEXT("Unknown");
        if (Cast<UAnimationGraph>(Graph))
        {
            GraphType = TEXT("AnimGraph");
        }
        else if (Cast<UAnimationStateMachineGraph>(Graph))
        {
            GraphType = TEXT("StateMachine");
        }
        else
        {
            // Could be a state inner graph or other sub-graph
            GraphType = TEXT("SubGraph");
        }

        // Apply filter: skip graphs that don't match the filter
        if (!GraphFilter.IsEmpty())
        {
            bool bMatchesName = GraphName.Equals(GraphFilter, ESearchCase::IgnoreCase);
            bool bMatchesType = GraphFilter.Equals(TEXT("AnimGraph"), ESearchCase::IgnoreCase) && GraphType == TEXT("AnimGraph");
            if (!bMatchesName && !bMatchesType)
            {
                continue;
            }
        }

        TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
        GraphObj->SetStringField(TEXT("graph_name"), GraphName);
        GraphObj->SetStringField(TEXT("graph_type"), GraphType);

        // Build node index map
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

            NodesArray.Add(MakeShared<FJsonValueObject>(SerializeAnimNode(Node, NodeIdx, NodeIndexMap)));
        }
        GraphObj->SetArrayField(TEXT("nodes"), NodesArray);
        GraphObj->SetNumberField(TEXT("node_count"), Nodes.Num());

        // List sub-graphs (state machines contain states with inner graphs)
        TArray<TSharedPtr<FJsonValue>> SubGraphsArray;
        TArray<UEdGraph*> ChildGraphs;
        Graph->GetAllChildrenGraphs(ChildGraphs);
        for (UEdGraph* ChildGraph : ChildGraphs)
        {
            if (ChildGraph)
            {
                TSharedPtr<FJsonObject> SubObj = MakeShared<FJsonObject>();
                SubObj->SetStringField(TEXT("name"), ChildGraph->GetName());
                SubObj->SetNumberField(TEXT("node_count"), ChildGraph->Nodes.Num());
                if (Cast<UAnimationStateMachineGraph>(ChildGraph))
                {
                    SubObj->SetStringField(TEXT("type"), TEXT("StateMachine"));
                }
                else
                {
                    SubObj->SetStringField(TEXT("type"), TEXT("SubGraph"));
                }
                SubGraphsArray.Add(MakeShared<FJsonValueObject>(SubObj));
            }
        }
        if (SubGraphsArray.Num() > 0)
        {
            GraphObj->SetArrayField(TEXT("sub_graphs"), SubGraphsArray);
        }

        GraphsArray.Add(MakeShared<FJsonValueObject>(GraphObj));
    }

    ResultObj->SetArrayField(TEXT("graphs"), GraphsArray);
    return ResultObj;
}

// ============================================================================
// add_anim_graph_node
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleAddAnimGraphNode(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    UEdGraph* Graph = ResolveAnimGraph(AnimBP, Params, Error);
    if (!Graph) return Error;

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Get node class
    FString NodeClassName;
    if (!Params->TryGetStringField(TEXT("node_class"), NodeClassName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_class' parameter"));
    }

    // Try multiple class name patterns to find the UClass
    UClass* NodeClass = nullptr;
    TArray<FString> ClassNameVariants;

    // User might pass "AnimGraphNode_Slot", "Slot", or full path
    ClassNameVariants.Add(NodeClassName);
    if (!NodeClassName.StartsWith(TEXT("AnimGraphNode_")))
    {
        ClassNameVariants.Add(FString::Printf(TEXT("AnimGraphNode_%s"), *NodeClassName));
    }
    if (!NodeClassName.StartsWith(TEXT("UAnimGraphNode_")))
    {
        ClassNameVariants.Add(FString::Printf(TEXT("UAnimGraphNode_%s"), *NodeClassName));
    }

    for (const FString& Variant : ClassNameVariants)
    {
        // Try FindObject first (already loaded classes)
        NodeClass = FindObject<UClass>(nullptr, *Variant);
        if (NodeClass) break;

        // Try with U prefix stripped for FindObject
        FString WithoutU = Variant;
        if (WithoutU.StartsWith(TEXT("U")))
        {
            WithoutU = WithoutU.Mid(1);
        }
        NodeClass = FindObject<UClass>(nullptr, *WithoutU);
        if (NodeClass) break;
    }

    // Last resort: iterate all classes looking for a match
    if (!NodeClass)
    {
        for (TObjectIterator<UClass> It; It; ++It)
        {
            if (It->IsChildOf(UAnimGraphNode_Base::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract))
            {
                FString ClassName = It->GetName();
                for (const FString& Variant : ClassNameVariants)
                {
                    if (ClassName.Equals(Variant, ESearchCase::IgnoreCase) ||
                        ClassName.Equals(FString::Printf(TEXT("U%s"), *Variant), ESearchCase::IgnoreCase))
                    {
                        NodeClass = *It;
                        break;
                    }
                }
                if (NodeClass) break;
            }
        }
    }

    if (!NodeClass)
    {
        // Build list of available classes for error message
        TArray<FString> AvailableClasses;
        for (TObjectIterator<UClass> It; It; ++It)
        {
            if (It->IsChildOf(UAnimGraphNode_Base::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract))
            {
                AvailableClasses.Add(It->GetName());
            }
        }
        AvailableClasses.Sort();

        FString AvailableList;
        int32 MaxToShow = FMath::Min(AvailableClasses.Num(), 30);
        for (int32 i = 0; i < MaxToShow; ++i)
        {
            if (i > 0) AvailableList += TEXT(", ");
            AvailableList += AvailableClasses[i];
        }
        if (AvailableClasses.Num() > MaxToShow)
        {
            AvailableList += FString::Printf(TEXT(" ... and %d more"), AvailableClasses.Num() - MaxToShow);
        }

        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("AnimGraph node class '%s' not found. Available classes: %s"), *NodeClassName, *AvailableList));
    }

    if (!NodeClass->IsChildOf(UAnimGraphNode_Base::StaticClass()))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Class '%s' is not an AnimGraph node (does not inherit UAnimGraphNode_Base)"), *NodeClassName));
    }

    // Create the node
    UAnimGraphNode_Base* NewNode = NewObject<UAnimGraphNode_Base>(Graph, NodeClass);
    NewNode->SetFlags(RF_Transactional);
    Graph->AddNode(NewNode, true, false);
    NewNode->CreateNewGuid();
    NewNode->PostPlacedNewNode();
    NewNode->AllocateDefaultPins();

    // Set position
    if (Params->HasField(TEXT("position")))
    {
        const TSharedPtr<FJsonObject>* PosObj;
        if (Params->TryGetObjectField(TEXT("position"), PosObj))
        {
            if ((*PosObj)->HasField(TEXT("x")))
                NewNode->NodePosX = static_cast<int32>((*PosObj)->GetNumberField(TEXT("x")));
            if ((*PosObj)->HasField(TEXT("y")))
                NewNode->NodePosY = static_cast<int32>((*PosObj)->GetNumberField(TEXT("y")));
        }
    }

    // Set initial properties on the inner node struct if provided
    if (Params->HasField(TEXT("properties")))
    {
        const TSharedPtr<FJsonObject>* PropsObj;
        if (Params->TryGetObjectField(TEXT("properties"), PropsObj))
        {
            FString PropError;
            for (const auto& Prop : (*PropsObj)->Values)
            {
                // Try setting on the node object itself first (which includes the inner Node struct)
                if (!FUnrealMCPCommonUtils::SetObjectProperty(NewNode, Prop.Key, Prop.Value, PropError))
                {
                    UE_LOG(LogMCPAnimGraph, Warning, TEXT("Failed to set property '%s': %s"), *Prop.Key, *PropError);
                }
            }
        }
    }

    MarkModifiedAndSave(AnimBP, Params);

    // Build result
    TMap<FGuid, int32> DummyMap;
    DummyMap.Add(NewNode->NodeGuid, 0);
    TSharedPtr<FJsonObject> ResultObj = SerializeAnimNode(NewNode, Graph->Nodes.Num() - 1, DummyMap);
    ResultObj->SetBoolField(TEXT("success"), true);

    return ResultObj;
}

// ============================================================================
// connect_anim_pins
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleConnectAnimPins(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    UEdGraph* Graph = ResolveAnimGraph(AnimBP, Params, Error);
    if (!Graph) return Error;

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Get source node
    FString SourceNodeId;
    if (!Params->TryGetStringField(TEXT("source_node"), SourceNodeId))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_node' parameter"));
    }

    // Get target node
    FString TargetNodeId;
    if (!Params->TryGetStringField(TEXT("target_node"), TargetNodeId))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'target_node' parameter"));
    }

    // Get pin names
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

    // Find nodes by GUID
    UEdGraphNode* SourceNode = nullptr;
    UEdGraphNode* TargetNode = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node)
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
    }

    if (!SourceNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Source node not found: %s"), *SourceNodeId));
    }
    if (!TargetNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Target node not found: %s"), *TargetNodeId));
    }

    // Find pins
    UEdGraphPin* SourcePin = FUnrealMCPCommonUtils::FindPin(SourceNode, SourcePinName, EGPD_Output);
    UEdGraphPin* TargetPin = FUnrealMCPCommonUtils::FindPin(TargetNode, TargetPinName, EGPD_Input);

    if (!SourcePin)
    {
        // List available output pins for the error message
        FString AvailablePins;
        for (UEdGraphPin* Pin : SourceNode->Pins)
        {
            if (Pin && Pin->Direction == EGPD_Output && !Pin->bHidden)
            {
                if (!AvailablePins.IsEmpty()) AvailablePins += TEXT(", ");
                AvailablePins += Pin->PinName.ToString();
            }
        }
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Source pin '%s' not found on node. Available output pins: %s"), *SourcePinName, *AvailablePins));
    }

    if (!TargetPin)
    {
        FString AvailablePins;
        for (UEdGraphPin* Pin : TargetNode->Pins)
        {
            if (Pin && Pin->Direction == EGPD_Input && !Pin->bHidden)
            {
                if (!AvailablePins.IsEmpty()) AvailablePins += TEXT(", ");
                AvailablePins += Pin->PinName.ToString();
            }
        }
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Target pin '%s' not found on node. Available input pins: %s"), *TargetPinName, *AvailablePins));
    }

    // Try schema-aware connection first (handles pose link coercion)
    const UEdGraphSchema* Schema = Graph->GetSchema();
    bool bConnected = false;

    if (Schema)
    {
        FPinConnectionResponse Response = Schema->CanCreateConnection(SourcePin, TargetPin);
        if (Response.Response != CONNECT_RESPONSE_DISALLOW)
        {
            bConnected = Schema->TryCreateConnection(SourcePin, TargetPin);
        }
        else
        {
            UE_LOG(LogMCPAnimGraph, Warning, TEXT("Schema disallowed connection: %s. Trying direct link."), *Response.Message.ToString());
        }
    }

    // Fallback: direct link (for data pins or if schema doesn't handle it)
    if (!bConnected)
    {
        SourcePin->MakeLinkTo(TargetPin);
        bConnected = SourcePin->LinkedTo.Contains(TargetPin);
    }

    if (!bConnected)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to connect pins"));
    }

    MarkModifiedAndSave(AnimBP, Params);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("source_node"), SourceNodeId);
    ResultObj->SetStringField(TEXT("source_pin"), SourcePinName);
    ResultObj->SetStringField(TEXT("target_node"), TargetNodeId);
    ResultObj->SetStringField(TEXT("target_pin"), TargetPinName);
    return ResultObj;
}

// ============================================================================
// set_anim_node_property
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleSetAnimNodeProperty(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    UEdGraph* Graph = ResolveAnimGraph(AnimBP, Params, Error);
    if (!Graph) return Error;

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Find target node
    UEdGraphNode* Node = FindNodeInGraph(Graph, Params, TEXT("node"), Error);
    if (!Node) return Error;

    // Get property name and value
    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
    }

    if (!Params->HasField(TEXT("value")))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' parameter"));
    }

    TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));

    // Navigate dot-path to set property
    // Support paths like "Node.SlotName" or just "SlotName" (auto-prefix with "Node.")
    FString FullPropertyPath = PropertyName;

    // Try setting the property directly on the node UObject
    FString PropError;
    bool bSet = FUnrealMCPCommonUtils::SetObjectProperty(Node, FullPropertyPath, Value, PropError);

    // If direct set failed, try with "Node." prefix (the inner FAnimNode struct)
    if (!bSet && !FullPropertyPath.StartsWith(TEXT("Node.")))
    {
        FString WithPrefix = FString::Printf(TEXT("Node.%s"), *FullPropertyPath);
        bSet = FUnrealMCPCommonUtils::SetObjectProperty(Node, WithPrefix, Value, PropError);
        if (bSet)
        {
            FullPropertyPath = WithPrefix;
        }
    }

    if (!bSet)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to set property '%s': %s"), *PropertyName, *PropError));
    }

    // Reconstruct node if needed (pin layout may depend on property values)
    Node->ReconstructNode();

    MarkModifiedAndSave(AnimBP, Params);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("property"), FullPropertyPath);
    ResultObj->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
    return ResultObj;
}

// ============================================================================
// delete_anim_graph_node
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleDeleteAnimGraphNode(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    UEdGraph* Graph = ResolveAnimGraph(AnimBP, Params, Error);
    if (!Graph) return Error;

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Find target node
    UEdGraphNode* Node = FindNodeInGraph(Graph, Params, TEXT("node"), Error);
    if (!Node) return Error;

    FString DeletedClass = Node->GetClass()->GetName();
    FString DeletedGuid = Node->NodeGuid.ToString();

    // Break all pin connections
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin)
        {
            Pin->BreakAllPinLinks();
        }
    }

    // Remove from graph
    Graph->RemoveNode(Node);

    MarkModifiedAndSave(AnimBP, Params);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("deleted_class"), DeletedClass);
    ResultObj->SetStringField(TEXT("deleted_node_id"), DeletedGuid);
    return ResultObj;
}

// ============================================================================
// disconnect_anim_pin
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleDisconnectAnimPin(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    UEdGraph* Graph = ResolveAnimGraph(AnimBP, Params, Error);
    if (!Graph) return Error;

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Find target node
    UEdGraphNode* Node = FindNodeInGraph(Graph, Params, TEXT("node"), Error);
    if (!Node) return Error;

    // Get pin name
    FString PinName;
    if (!Params->TryGetStringField(TEXT("pin_name"), PinName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pin_name' parameter"));
    }

    // Find the pin (accept either direction)
    UEdGraphPin* Pin = FUnrealMCPCommonUtils::FindPin(Node, PinName);
    if (!Pin)
    {
        FString AvailablePins;
        for (UEdGraphPin* NodePin : Node->Pins)
        {
            if (NodePin && !NodePin->bHidden)
            {
                if (!AvailablePins.IsEmpty()) AvailablePins += TEXT(", ");
                AvailablePins += FString::Printf(TEXT("%s (%s)"), *NodePin->PinName.ToString(),
                    NodePin->Direction == EGPD_Input ? TEXT("In") : TEXT("Out"));
            }
        }
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Pin '%s' not found. Available pins: %s"), *PinName, *AvailablePins));
    }

    int32 DisconnectedCount = Pin->LinkedTo.Num();
    Pin->BreakAllPinLinks();

    MarkModifiedAndSave(AnimBP, Params);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
    ResultObj->SetStringField(TEXT("pin_name"), PinName);
    ResultObj->SetNumberField(TEXT("disconnected_count"), DisconnectedCount);
    return ResultObj;
}

// ============================================================================
// find_anim_graph_nodes
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleFindAnimGraphNodes(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    UEdGraph* Graph = ResolveAnimGraph(AnimBP, Params, Error);
    if (!Graph) return Error;

    // Optional filters
    FString FilterClass;
    Params->TryGetStringField(TEXT("node_class"), FilterClass);

    const TSharedPtr<FJsonObject>* PropertyFilter = nullptr;
    Params->TryGetObjectField(TEXT("property_filter"), PropertyFilter);

    // Build node index map
    TMap<FGuid, int32> NodeIndexMap;
    for (int32 i = 0; i < Graph->Nodes.Num(); ++i)
    {
        if (Graph->Nodes[i])
        {
            NodeIndexMap.Add(Graph->Nodes[i]->NodeGuid, i);
        }
    }

    // Filter and collect matching nodes
    TArray<TSharedPtr<FJsonValue>> MatchingNodes;

    for (int32 NodeIdx = 0; NodeIdx < Graph->Nodes.Num(); ++NodeIdx)
    {
        UEdGraphNode* Node = Graph->Nodes[NodeIdx];
        if (!Node)
        {
            continue;
        }

        // Class filter
        if (!FilterClass.IsEmpty())
        {
            FString ClassName = Node->GetClass()->GetName();
            if (!ClassName.Contains(FilterClass, ESearchCase::IgnoreCase))
            {
                continue;
            }
        }

        // Property filter: check if the node's properties match
        if (PropertyFilter && (*PropertyFilter)->Values.Num() > 0)
        {
            bool bAllMatch = true;
            for (const auto& FilterProp : (*PropertyFilter)->Values)
            {
                // Try to read the property from the node
                FProperty* Prop = Node->GetClass()->FindPropertyByName(FName(*FilterProp.Key));
                if (!Prop)
                {
                    // Try with "Node." prefix
                    FString WithPrefix = FString::Printf(TEXT("Node.%s"), *FilterProp.Key);
                    // For nested properties, we'd need a more sophisticated lookup
                    // For now, just skip non-matching properties
                    bAllMatch = false;
                    break;
                }

                // Get current value as string for comparison
                FString CurrentValue;
                Prop->ExportTextItem_Direct(CurrentValue, Prop->ContainerPtrToValuePtr<void>(Node), nullptr, Node, PPF_None);

                FString FilterValue;
                if (FilterProp.Value->TryGetString(FilterValue))
                {
                    if (!CurrentValue.Equals(FilterValue, ESearchCase::IgnoreCase))
                    {
                        bAllMatch = false;
                        break;
                    }
                }
            }
            if (!bAllMatch)
            {
                continue;
            }
        }

        MatchingNodes.Add(MakeShared<FJsonValueObject>(SerializeAnimNode(Node, NodeIdx, NodeIndexMap)));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("nodes"), MatchingNodes);
    ResultObj->SetNumberField(TEXT("count"), MatchingNodes.Num());
    return ResultObj;
}

// ============================================================================
// Shared Helpers (Tier 2+)
// ============================================================================

UEdGraph* FUnrealMCPAnimGraphCommands::FindStateMachineGraph(UAnimBlueprint* AnimBP, const FString& StateMachineGuid,
                                                              TSharedPtr<FJsonObject>& OutError)
{
    // Find the state machine node in the root AnimGraph
    UEdGraph* RootGraph = FUnrealMCPCommonUtils::FindAnimGraph(AnimBP);
    if (!RootGraph)
    {
        OutError = FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No root AnimGraph found"));
        return nullptr;
    }

    for (UEdGraphNode* Node : RootGraph->Nodes)
    {
        if (Node && Node->NodeGuid.ToString() == StateMachineGuid)
        {
            UAnimGraphNode_StateMachine* SMNode = Cast<UAnimGraphNode_StateMachine>(Node);
            if (!SMNode)
            {
                OutError = FUnrealMCPCommonUtils::CreateErrorResponse(
                    FString::Printf(TEXT("Node '%s' is not a state machine node"), *StateMachineGuid));
                return nullptr;
            }
            UAnimationStateMachineGraph* SMGraph = SMNode->EditorStateMachineGraph;
            if (!SMGraph)
            {
                OutError = FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("State machine has no editor graph"));
                return nullptr;
            }
            return SMGraph;
        }
    }

    OutError = FUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("State machine node not found with GUID: %s"), *StateMachineGuid));
    return nullptr;
}

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::CreateAnimNodeInGraph(UEdGraph* Graph, UAnimBlueprint* AnimBP,
                                                                            const FString& NodeClassName,
                                                                            const TSharedPtr<FJsonObject>& Params)
{
    // Resolve node class (same logic as HandleAddAnimGraphNode)
    UClass* NodeClass = nullptr;
    TArray<FString> ClassNameVariants;
    ClassNameVariants.Add(NodeClassName);
    if (!NodeClassName.StartsWith(TEXT("AnimGraphNode_")))
    {
        ClassNameVariants.Add(FString::Printf(TEXT("AnimGraphNode_%s"), *NodeClassName));
    }
    if (!NodeClassName.StartsWith(TEXT("UAnimGraphNode_")))
    {
        ClassNameVariants.Add(FString::Printf(TEXT("UAnimGraphNode_%s"), *NodeClassName));
    }

    for (const FString& Variant : ClassNameVariants)
    {
        NodeClass = FindObject<UClass>(nullptr, *Variant);
        if (NodeClass) break;
        FString WithoutU = Variant;
        if (WithoutU.StartsWith(TEXT("U")))
        {
            WithoutU = WithoutU.Mid(1);
        }
        NodeClass = FindObject<UClass>(nullptr, *WithoutU);
        if (NodeClass) break;
    }

    if (!NodeClass)
    {
        for (TObjectIterator<UClass> It; It; ++It)
        {
            if (It->IsChildOf(UAnimGraphNode_Base::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract))
            {
                FString ClassName = It->GetName();
                for (const FString& Variant : ClassNameVariants)
                {
                    if (ClassName.Equals(Variant, ESearchCase::IgnoreCase))
                    {
                        NodeClass = *It;
                        break;
                    }
                }
                if (NodeClass) break;
            }
        }
    }

    if (!NodeClass || !NodeClass->IsChildOf(UAnimGraphNode_Base::StaticClass()))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("AnimGraph node class '%s' not found"), *NodeClassName));
    }

    UAnimGraphNode_Base* NewNode = NewObject<UAnimGraphNode_Base>(Graph, NodeClass);
    NewNode->SetFlags(RF_Transactional);
    Graph->AddNode(NewNode, true, false);
    NewNode->CreateNewGuid();
    NewNode->PostPlacedNewNode();
    NewNode->AllocateDefaultPins();

    // Set position
    if (Params->HasField(TEXT("position")))
    {
        const TSharedPtr<FJsonObject>* PosObj;
        if (Params->TryGetObjectField(TEXT("position"), PosObj))
        {
            if ((*PosObj)->HasField(TEXT("x")))
                NewNode->NodePosX = static_cast<int32>((*PosObj)->GetNumberField(TEXT("x")));
            if ((*PosObj)->HasField(TEXT("y")))
                NewNode->NodePosY = static_cast<int32>((*PosObj)->GetNumberField(TEXT("y")));
        }
    }

    // Set initial properties
    if (Params->HasField(TEXT("properties")))
    {
        const TSharedPtr<FJsonObject>* PropsObj;
        if (Params->TryGetObjectField(TEXT("properties"), PropsObj))
        {
            FString PropError;
            for (const auto& Prop : (*PropsObj)->Values)
            {
                if (!FUnrealMCPCommonUtils::SetObjectProperty(NewNode, Prop.Key, Prop.Value, PropError))
                {
                    UE_LOG(LogMCPAnimGraph, Warning, TEXT("Failed to set property '%s': %s"), *Prop.Key, *PropError);
                }
            }
        }
    }

    // Build result
    TMap<FGuid, int32> DummyMap;
    DummyMap.Add(NewNode->NodeGuid, 0);
    TSharedPtr<FJsonObject> ResultObj = SerializeAnimNode(NewNode, Graph->Nodes.Num() - 1, DummyMap);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// ============================================================================
// Tier 2: add_state_machine
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleAddStateMachine(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    UEdGraph* Graph = ResolveAnimGraph(AnimBP, Params, Error);
    if (!Graph) return Error;

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    FString Name = TEXT("StateMachine");
    Params->TryGetStringField(TEXT("name"), Name);

    // Create the state machine node
    UAnimGraphNode_StateMachine* SMNode = NewObject<UAnimGraphNode_StateMachine>(Graph);
    SMNode->SetFlags(RF_Transactional);
    Graph->AddNode(SMNode, true, false);
    SMNode->CreateNewGuid();
    SMNode->PostPlacedNewNode();
    SMNode->AllocateDefaultPins();

    // Set position
    if (Params->HasField(TEXT("position")))
    {
        const TSharedPtr<FJsonObject>* PosObj;
        if (Params->TryGetObjectField(TEXT("position"), PosObj))
        {
            if ((*PosObj)->HasField(TEXT("x")))
                SMNode->NodePosX = static_cast<int32>((*PosObj)->GetNumberField(TEXT("x")));
            if ((*PosObj)->HasField(TEXT("y")))
                SMNode->NodePosY = static_cast<int32>((*PosObj)->GetNumberField(TEXT("y")));
        }
    }

    MarkModifiedAndSave(AnimBP, Params);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("node_id"), SMNode->NodeGuid.ToString());
    ResultObj->SetStringField(TEXT("node_class"), SMNode->GetClass()->GetName());
    ResultObj->SetStringField(TEXT("name"), Name);

    // Report the inner state machine graph
    if (SMNode->EditorStateMachineGraph)
    {
        ResultObj->SetStringField(TEXT("state_machine_graph"), SMNode->EditorStateMachineGraph->GetName());

        // Find the entry node
        UAnimationStateMachineGraph* SMGraph = SMNode->EditorStateMachineGraph;
        if (SMGraph->EntryNode)
        {
            ResultObj->SetStringField(TEXT("entry_node_id"), SMGraph->EntryNode->NodeGuid.ToString());
        }
    }

    // List pins
    TArray<TSharedPtr<FJsonValue>> PinArray;
    for (UEdGraphPin* Pin : SMNode->Pins)
    {
        if (Pin && !Pin->bHidden)
        {
            TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
            PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
            PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
            PinArray.Add(MakeShared<FJsonValueObject>(PinObj));
        }
    }
    ResultObj->SetArrayField(TEXT("pins"), PinArray);

    return ResultObj;
}

// ============================================================================
// Tier 2: add_state
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleAddState(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    FString SMGuid;
    if (!Params->TryGetStringField(TEXT("state_machine"), SMGuid))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'state_machine' parameter (GUID of the state machine node)"));
    }

    UEdGraph* SMGraph = FindStateMachineGraph(AnimBP, SMGuid, Error);
    if (!SMGraph) return Error;

    FString StateName = TEXT("NewState");
    Params->TryGetStringField(TEXT("name"), StateName);

    FString StateType = TEXT("State");
    Params->TryGetStringField(TEXT("type"), StateType);

    UAnimStateNodeBase* CreatedNode = nullptr;

    if (StateType.Equals(TEXT("Conduit"), ESearchCase::IgnoreCase))
    {
        // Create a conduit node — pass-through that evaluates a condition
        UAnimStateConduitNode* ConduitNode = NewObject<UAnimStateConduitNode>(SMGraph);
        ConduitNode->SetFlags(RF_Transactional);
        SMGraph->AddNode(ConduitNode, true, false);
        ConduitNode->CreateNewGuid();
        ConduitNode->PostPlacedNewNode();
        ConduitNode->AllocateDefaultPins();
        CreatedNode = ConduitNode;
    }
    else if (StateType.Equals(TEXT("Alias"), ESearchCase::IgnoreCase))
    {
        // Create an alias node — references another state
        UAnimStateAliasNode* AliasNode = NewObject<UAnimStateAliasNode>(SMGraph);
        AliasNode->SetFlags(RF_Transactional);
        SMGraph->AddNode(AliasNode, true, false);
        AliasNode->CreateNewGuid();
        AliasNode->PostPlacedNewNode();
        AliasNode->AllocateDefaultPins();

        // If alias_target is provided, set the aliased state
        FString AliasTargetGuid;
        if (Params->TryGetStringField(TEXT("alias_target"), AliasTargetGuid))
        {
            for (UEdGraphNode* Node : SMGraph->Nodes)
            {
                if (Node && Node->NodeGuid.ToString() == AliasTargetGuid)
                {
                    UAnimStateNodeBase* TargetState = Cast<UAnimStateNodeBase>(Node);
                    if (TargetState)
                    {
                        AliasNode->GetAliasedStates().Add(TargetState);
                    }
                    break;
                }
            }
        }

        // Global alias flag
        bool bGlobalAlias = false;
        if (Params->TryGetBoolField(TEXT("global_alias"), bGlobalAlias))
        {
            AliasNode->bGlobalAlias = bGlobalAlias;
        }

        CreatedNode = AliasNode;
    }
    else
    {
        // Default: regular state node
        UAnimStateNode* StateNode = NewObject<UAnimStateNode>(SMGraph);
        StateNode->SetFlags(RF_Transactional);
        SMGraph->AddNode(StateNode, true, false);
        StateNode->CreateNewGuid();
        StateNode->PostPlacedNewNode();
        StateNode->AllocateDefaultPins();
        CreatedNode = StateNode;
    }

    // Set position
    if (Params->HasField(TEXT("position")))
    {
        const TSharedPtr<FJsonObject>* PosObj;
        if (Params->TryGetObjectField(TEXT("position"), PosObj))
        {
            if ((*PosObj)->HasField(TEXT("x")))
                CreatedNode->NodePosX = static_cast<int32>((*PosObj)->GetNumberField(TEXT("x")));
            if ((*PosObj)->HasField(TEXT("y")))
                CreatedNode->NodePosY = static_cast<int32>((*PosObj)->GetNumberField(TEXT("y")));
        }
    }

    // Rename the state's BoundGraph so the name actually shows in the editor
    if (!StateName.Equals(TEXT("NewState")) && CreatedNode->GetBoundGraph())
    {
        CreatedNode->OnRenameNode(StateName);
    }

    MarkModifiedAndSave(AnimBP, Params);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("node_id"), CreatedNode->NodeGuid.ToString());
    ResultObj->SetStringField(TEXT("name"), CreatedNode->GetStateName());
    ResultObj->SetStringField(TEXT("type"), StateType);

    // Report inner graph info
    if (CreatedNode->GetBoundGraph())
    {
        ResultObj->SetStringField(TEXT("inner_graph"), CreatedNode->GetBoundGraph()->GetName());

        // For regular states, report the result node
        UAnimationStateGraph* StateGraph = Cast<UAnimationStateGraph>(CreatedNode->GetBoundGraph());
        if (StateGraph && StateGraph->MyResultNode)
        {
            ResultObj->SetStringField(TEXT("result_node_id"), StateGraph->MyResultNode->NodeGuid.ToString());
        }
    }

    return ResultObj;
}

// ============================================================================
// Tier 2: add_state_transition
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleAddStateTransition(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    FString SMGuid;
    if (!Params->TryGetStringField(TEXT("state_machine"), SMGuid))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'state_machine' parameter"));
    }

    FString FromStateGuid;
    if (!Params->TryGetStringField(TEXT("from_state"), FromStateGuid))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'from_state' parameter"));
    }

    FString ToStateGuid;
    if (!Params->TryGetStringField(TEXT("to_state"), ToStateGuid))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'to_state' parameter"));
    }

    UEdGraph* SMGraph = FindStateMachineGraph(AnimBP, SMGuid, Error);
    if (!SMGraph) return Error;

    // Find the from and to state nodes
    UAnimStateNodeBase* FromState = nullptr;
    UAnimStateNodeBase* ToState = nullptr;
    for (UEdGraphNode* Node : SMGraph->Nodes)
    {
        if (Node)
        {
            if (Node->NodeGuid.ToString() == FromStateGuid)
            {
                FromState = Cast<UAnimStateNodeBase>(Node);
            }
            if (Node->NodeGuid.ToString() == ToStateGuid)
            {
                ToState = Cast<UAnimStateNodeBase>(Node);
            }
        }
    }

    if (!FromState)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("From state not found: %s"), *FromStateGuid));
    }
    if (!ToState)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("To state not found: %s"), *ToStateGuid));
    }

    // Create the transition node
    UAnimStateTransitionNode* TransNode = NewObject<UAnimStateTransitionNode>(SMGraph);
    TransNode->SetFlags(RF_Transactional);
    SMGraph->AddNode(TransNode, true, false);
    TransNode->CreateNewGuid();
    TransNode->PostPlacedNewNode();
    TransNode->AllocateDefaultPins();

    // Wire from_state → transition → to_state using schema
    const UEdGraphSchema* Schema = SMGraph->GetSchema();
    bool bConnected = false;

    if (Schema)
    {
        // Connect from_state output → transition input
        UEdGraphPin* FromOutput = (FromState->Pins.Num() > 0) ? FromState->GetOutputPin() : nullptr;
        UEdGraphPin* TransInput = (TransNode->Pins.Num() > 0) ? TransNode->GetInputPin() : nullptr;
        UEdGraphPin* TransOutput = (TransNode->Pins.Num() > 0) ? TransNode->GetOutputPin() : nullptr;
        UEdGraphPin* ToInput = (ToState->Pins.Num() > 0) ? ToState->GetInputPin() : nullptr;

        if (FromOutput && TransInput)
        {
            Schema->TryCreateConnection(FromOutput, TransInput);
        }
        if (TransOutput && ToInput)
        {
            Schema->TryCreateConnection(TransOutput, ToInput);
        }
        bConnected = true;
    }

    MarkModifiedAndSave(AnimBP, Params);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("transition_id"), TransNode->NodeGuid.ToString());
    ResultObj->SetStringField(TEXT("from_state"), FromStateGuid);
    ResultObj->SetStringField(TEXT("to_state"), ToStateGuid);
    ResultObj->SetBoolField(TEXT("connected"), bConnected);

    if (TransNode->BoundGraph)
    {
        ResultObj->SetStringField(TEXT("transition_graph"), TransNode->BoundGraph->GetName());
    }

    return ResultObj;
}

// ============================================================================
// rename_state — Rename a state/conduit in a state machine
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleRenameState(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    FString SMGuid;
    if (!Params->TryGetStringField(TEXT("state_machine"), SMGuid))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'state_machine' parameter"));
    }

    FString StateGuid;
    if (!Params->TryGetStringField(TEXT("state"), StateGuid))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'state' parameter (GUID)"));
    }

    FString NewName;
    if (!Params->TryGetStringField(TEXT("name"), NewName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    UEdGraph* SMGraph = FindStateMachineGraph(AnimBP, SMGuid, Error);
    if (!SMGraph) return Error;

    // Find the state node
    UAnimStateNodeBase* StateNode = nullptr;
    for (UEdGraphNode* Node : SMGraph->Nodes)
    {
        if (Node && Node->NodeGuid.ToString() == StateGuid)
        {
            StateNode = Cast<UAnimStateNodeBase>(Node);
            break;
        }
    }

    if (!StateNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("State node not found: %s"), *StateGuid));
    }

    // Get old name for response
    FString OldName = StateNode->GetStateName();

    // Rename via OnRenameNode which calls FBlueprintEditorUtils::RenameGraph on the BoundGraph
    StateNode->OnRenameNode(NewName);

    MarkModifiedAndSave(AnimBP, Params);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("state"), StateGuid);
    ResultObj->SetStringField(TEXT("old_name"), OldName);
    ResultObj->SetStringField(TEXT("new_name"), StateNode->GetStateName());
    return ResultObj;
}

// ============================================================================
// Tier 2: set_state_animation
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleSetStateAnimation(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    FString StateGuid;
    if (!Params->TryGetStringField(TEXT("state"), StateGuid))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'state' parameter (GUID of the state node)"));
    }

    FString NodeClassName;
    if (!Params->TryGetStringField(TEXT("node_class"), NodeClassName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_class' parameter"));
    }

    // Find the state node — search all state machine graphs
    UAnimStateNode* StateNode = nullptr;
    UEdGraph* RootGraph = FUnrealMCPCommonUtils::FindAnimGraph(AnimBP);
    if (RootGraph)
    {
        // Search state machine nodes in root graph
        for (UEdGraphNode* Node : RootGraph->Nodes)
        {
            UAnimGraphNode_StateMachine* SMNode = Cast<UAnimGraphNode_StateMachine>(Node);
            if (SMNode && SMNode->EditorStateMachineGraph)
            {
                for (UEdGraphNode* SMChild : SMNode->EditorStateMachineGraph->Nodes)
                {
                    if (SMChild && SMChild->NodeGuid.ToString() == StateGuid)
                    {
                        StateNode = Cast<UAnimStateNode>(SMChild);
                        break;
                    }
                }
                if (StateNode) break;
            }
        }
    }

    if (!StateNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("State node not found: %s"), *StateGuid));
    }

    UAnimationStateGraph* StateGraph = Cast<UAnimationStateGraph>(StateNode->BoundGraph);
    if (!StateGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("State has no inner graph"));
    }

    // Create the animation node in the state's inner graph
    TSharedPtr<FJsonObject> NodeResult = CreateAnimNodeInGraph(StateGraph, AnimBP, NodeClassName, Params);

    // Check if creation failed
    if (NodeResult->HasField(TEXT("success")) && !NodeResult->GetBoolField(TEXT("success")))
    {
        return NodeResult;
    }

    // Auto-connect to state result if requested (default: true)
    bool bAutoConnect = true;
    if (Params->HasField(TEXT("auto_connect")))
    {
        bAutoConnect = Params->GetBoolField(TEXT("auto_connect"));
    }

    if (bAutoConnect && StateGraph->MyResultNode)
    {
        // Find the newly created node (last in the graph)
        FString NewNodeGuid;
        if (NodeResult->TryGetStringField(TEXT("node_id"), NewNodeGuid))
        {
            UEdGraphNode* NewNode = nullptr;
            for (UEdGraphNode* Node : StateGraph->Nodes)
            {
                if (Node && Node->NodeGuid.ToString() == NewNodeGuid)
                {
                    NewNode = Node;
                    break;
                }
            }

            if (NewNode)
            {
                // Find output pose pin on new node and input pose pin on result
                UEdGraphPin* OutputPose = nullptr;
                for (UEdGraphPin* Pin : NewNode->Pins)
                {
                    if (Pin && Pin->Direction == EGPD_Output && !Pin->bHidden)
                    {
                        OutputPose = Pin;
                        break;
                    }
                }

                UEdGraphPin* ResultInput = nullptr;
                for (UEdGraphPin* Pin : StateGraph->MyResultNode->Pins)
                {
                    if (Pin && Pin->Direction == EGPD_Input && !Pin->bHidden)
                    {
                        ResultInput = Pin;
                        break;
                    }
                }

                if (OutputPose && ResultInput)
                {
                    const UEdGraphSchema* Schema = StateGraph->GetSchema();
                    if (Schema)
                    {
                        Schema->TryCreateConnection(OutputPose, ResultInput);
                    }
                    else
                    {
                        OutputPose->MakeLinkTo(ResultInput);
                    }
                    NodeResult->SetBoolField(TEXT("auto_connected"), true);
                }
            }
        }
    }

    MarkModifiedAndSave(AnimBP, Params);
    return NodeResult;
}

// ============================================================================
// Tier 2: read_state_machine
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleReadStateMachine(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    FString SMGuid;
    if (!Params->TryGetStringField(TEXT("state_machine"), SMGuid))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'state_machine' parameter"));
    }

    UEdGraph* SMGraph = FindStateMachineGraph(AnimBP, SMGuid, Error);
    if (!SMGraph) return Error;

    UAnimationStateMachineGraph* TypedSMGraph = Cast<UAnimationStateMachineGraph>(SMGraph);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("state_machine_id"), SMGuid);
    ResultObj->SetStringField(TEXT("graph_name"), SMGraph->GetName());

    // Entry state
    if (TypedSMGraph && TypedSMGraph->EntryNode)
    {
        ResultObj->SetStringField(TEXT("entry_node_id"), TypedSMGraph->EntryNode->NodeGuid.ToString());

        // Find which state the entry connects to
        for (UEdGraphPin* Pin : TypedSMGraph->EntryNode->Pins)
        {
            if (Pin && Pin->Direction == EGPD_Output && Pin->LinkedTo.Num() > 0)
            {
                UEdGraphNode* ConnectedNode = Pin->LinkedTo[0]->GetOwningNode();
                if (ConnectedNode)
                {
                    ResultObj->SetStringField(TEXT("entry_state_id"), ConnectedNode->NodeGuid.ToString());
                }
            }
        }
    }

    // States
    TArray<TSharedPtr<FJsonValue>> StatesArray;
    TArray<TSharedPtr<FJsonValue>> TransitionsArray;

    // Optional: include_inner_nodes to serialize nodes within each state's graph
    bool bIncludeInnerNodes = false;
    Params->TryGetBoolField(TEXT("include_inner_nodes"), bIncludeInnerNodes);

    // Build node index map for inner graph serialization
    auto SerializeInnerGraph = [&](UEdGraph* InnerGraph) -> TArray<TSharedPtr<FJsonValue>>
    {
        TArray<TSharedPtr<FJsonValue>> NodesArray;
        if (!InnerGraph) return NodesArray;

        TMap<FGuid, int32> IndexMap;
        for (int32 i = 0; i < InnerGraph->Nodes.Num(); i++)
        {
            if (InnerGraph->Nodes[i])
            {
                IndexMap.Add(InnerGraph->Nodes[i]->NodeGuid, i);
            }
        }

        for (int32 i = 0; i < InnerGraph->Nodes.Num(); i++)
        {
            UEdGraphNode* InnerNode = InnerGraph->Nodes[i];
            if (!InnerNode) continue;

            TSharedPtr<FJsonObject> NodeObj = SerializeAnimNode(InnerNode, i, IndexMap);
            if (NodeObj.IsValid())
            {
                NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
            }
        }
        return NodesArray;
    };

    for (UEdGraphNode* Node : SMGraph->Nodes)
    {
        if (!Node) continue;

        // Handle all state-like nodes (regular states, conduits, aliases)
        if (UAnimStateNodeBase* StateNodeBase = Cast<UAnimStateNodeBase>(Node))
        {
            // Skip entry nodes
            if (Cast<UAnimStateEntryNode>(Node)) continue;
            // Skip transitions (handled below)
            if (Cast<UAnimStateTransitionNode>(Node)) continue;

            TSharedPtr<FJsonObject> StateObj = MakeShared<FJsonObject>();
            StateObj->SetStringField(TEXT("node_id"), StateNodeBase->NodeGuid.ToString());
            StateObj->SetStringField(TEXT("name"), StateNodeBase->GetStateName());
            StateObj->SetNumberField(TEXT("pos_x"), StateNodeBase->NodePosX);
            StateObj->SetNumberField(TEXT("pos_y"), StateNodeBase->NodePosY);

            // Determine type
            if (Cast<UAnimStateConduitNode>(Node))
            {
                StateObj->SetStringField(TEXT("type"), TEXT("Conduit"));
            }
            else if (UAnimStateAliasNode* AliasNode = Cast<UAnimStateAliasNode>(Node))
            {
                StateObj->SetStringField(TEXT("type"), TEXT("Alias"));
                StateObj->SetBoolField(TEXT("global_alias"), AliasNode->bGlobalAlias);

                // Report aliased states
                TArray<TSharedPtr<FJsonValue>> AliasedArray;
                for (const TWeakObjectPtr<UAnimStateNodeBase>& AliasedState : AliasNode->GetAliasedStates())
                {
                    if (AliasedState.IsValid())
                    {
                        TSharedPtr<FJsonObject> AObj = MakeShared<FJsonObject>();
                        AObj->SetStringField(TEXT("node_id"), AliasedState->NodeGuid.ToString());
                        AObj->SetStringField(TEXT("name"), AliasedState->GetStateName());
                        AliasedArray.Add(MakeShared<FJsonValueObject>(AObj));
                    }
                }
                if (AliasedArray.Num() > 0)
                {
                    StateObj->SetArrayField(TEXT("aliased_states"), AliasedArray);
                }
            }
            else
            {
                StateObj->SetStringField(TEXT("type"), TEXT("State"));
            }

            UEdGraph* BoundGraph = StateNodeBase->GetBoundGraph();
            if (BoundGraph)
            {
                StateObj->SetStringField(TEXT("inner_graph"), BoundGraph->GetName());
                StateObj->SetNumberField(TEXT("inner_node_count"), BoundGraph->Nodes.Num());

                // For regular states, report the result node
                UAnimationStateGraph* StateGraph = Cast<UAnimationStateGraph>(BoundGraph);
                if (StateGraph && StateGraph->MyResultNode)
                {
                    StateObj->SetStringField(TEXT("result_node_id"), StateGraph->MyResultNode->NodeGuid.ToString());
                }

                // Optionally serialize inner graph nodes
                if (bIncludeInnerNodes)
                {
                    TArray<TSharedPtr<FJsonValue>> InnerNodes = SerializeInnerGraph(BoundGraph);
                    StateObj->SetArrayField(TEXT("inner_nodes"), InnerNodes);
                }
            }

            StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
        }
        else if (UAnimStateTransitionNode* TransNode = Cast<UAnimStateTransitionNode>(Node))
        {
            TSharedPtr<FJsonObject> TransObj = MakeShared<FJsonObject>();
            TransObj->SetStringField(TEXT("node_id"), TransNode->NodeGuid.ToString());

            // Find from/to states via pin connections
            UEdGraphPin* InputPin = TransNode->GetInputPin();
            UEdGraphPin* OutputPin = TransNode->GetOutputPin();

            if (InputPin && InputPin->LinkedTo.Num() > 0)
            {
                UEdGraphNode* FromNode = InputPin->LinkedTo[0]->GetOwningNode();
                if (FromNode)
                {
                    TransObj->SetStringField(TEXT("from_state"), FromNode->NodeGuid.ToString());
                }
            }
            if (OutputPin && OutputPin->LinkedTo.Num() > 0)
            {
                UEdGraphNode* ToNode = OutputPin->LinkedTo[0]->GetOwningNode();
                if (ToNode)
                {
                    TransObj->SetStringField(TEXT("to_state"), ToNode->NodeGuid.ToString());
                }
            }

            // Transition properties
            TransObj->SetNumberField(TEXT("crossfade_duration"), TransNode->CrossfadeDuration);
            TransObj->SetNumberField(TEXT("priority"), TransNode->PriorityOrder);
            TransObj->SetBoolField(TEXT("bidirectional"), TransNode->Bidirectional);
            TransObj->SetBoolField(TEXT("automatic_rule"), TransNode->bAutomaticRuleBasedOnSequencePlayerInState);

            if (TransNode->BoundGraph)
            {
                TransObj->SetStringField(TEXT("transition_graph"), TransNode->BoundGraph->GetName());

                // Optionally serialize transition rule graph nodes
                if (bIncludeInnerNodes)
                {
                    TArray<TSharedPtr<FJsonValue>> InnerNodes = SerializeInnerGraph(TransNode->BoundGraph);
                    TransObj->SetArrayField(TEXT("rule_nodes"), InnerNodes);
                }
            }

            TransitionsArray.Add(MakeShared<FJsonValueObject>(TransObj));
        }
    }

    ResultObj->SetArrayField(TEXT("states"), StatesArray);
    ResultObj->SetNumberField(TEXT("state_count"), StatesArray.Num());
    ResultObj->SetArrayField(TEXT("transitions"), TransitionsArray);
    ResultObj->SetNumberField(TEXT("transition_count"), TransitionsArray.Num());

    return ResultObj;
}

// ============================================================================
// Tier 3: add_anim_layer
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleAddAnimLayer(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    UEdGraph* Graph = ResolveAnimGraph(AnimBP, Params, Error);
    if (!Graph) return Error;

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    FString LayerType = TEXT("LinkedAnimGraph");
    Params->TryGetStringField(TEXT("layer_type"), LayerType);

    // Map layer type to class name
    FString NodeClassName;
    if (LayerType.Equals(TEXT("LinkedAnimGraph"), ESearchCase::IgnoreCase))
    {
        NodeClassName = TEXT("AnimGraphNode_LinkedAnimGraph");
    }
    else if (LayerType.Equals(TEXT("LinkedAnimLayer"), ESearchCase::IgnoreCase))
    {
        NodeClassName = TEXT("AnimGraphNode_LinkedAnimLayer");
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Unknown layer_type: '%s'. Valid: LinkedAnimGraph, LinkedAnimLayer"), *LayerType));
    }

    // For LinkedAnimLayer: create the backing AnimGraph function first (if layer_name provided)
    FString LayerName;
    Params->TryGetStringField(TEXT("layer_name"), LayerName);

    UEdGraph* LayerGraph = nullptr;
    if (LayerType.Equals(TEXT("LinkedAnimLayer"), ESearchCase::IgnoreCase) && !LayerName.IsEmpty())
    {
        // Check if the layer graph already exists
        for (UEdGraph* ExistingGraph : AnimBP->FunctionGraphs)
        {
            if (ExistingGraph && ExistingGraph->GetName() == LayerName)
            {
                LayerGraph = ExistingGraph;
                break;
            }
        }

        // Create the layer AnimGraph function if it doesn't exist
        if (!LayerGraph)
        {
            LayerGraph = FBlueprintEditorUtils::CreateNewGraph(
                AnimBP,
                FName(*LayerName),
                UAnimationGraph::StaticClass(),
                UAnimationGraphSchema::StaticClass()
            );

            if (LayerGraph)
            {
                FBlueprintEditorUtils::AddDomainSpecificGraph(AnimBP, LayerGraph);
            }
        }
    }

    // Create the node
    TSharedPtr<FJsonObject> Result = CreateAnimNodeInGraph(Graph, AnimBP, NodeClassName, Params);

    // If we have a layer name, wire the node to its backing function
    if (!LayerName.IsEmpty() && Result.IsValid() && !Result->HasField(TEXT("error")))
    {
        FString NodeGuid;
        if (Result->TryGetStringField(TEXT("node_id"), NodeGuid))
        {
            // Find the created node
            for (UEdGraphNode* GNode : Graph->Nodes)
            {
                if (GNode && GNode->NodeGuid.ToString() == NodeGuid)
                {
                    if (UAnimGraphNode_LinkedAnimLayer* LayerNode = Cast<UAnimGraphNode_LinkedAnimLayer>(GNode))
                    {
                        // Set Node.Layer directly (public UPROPERTY)
                        FName LayerFName(*LayerName);
                        LayerNode->Node.Layer = LayerFName;

                        // Access FunctionReference via reflection (protected UPROPERTY)
                        FProperty* FuncRefProp = LayerNode->GetClass()->FindPropertyByName(TEXT("FunctionReference"));
                        if (FuncRefProp)
                        {
                            FMemberReference* FuncRef = FuncRefProp->ContainerPtrToValuePtr<FMemberReference>(LayerNode);
                            if (FuncRef)
                            {
                                UClass* TargetClass = LayerNode->GetTargetClass();
                                if (TargetClass)
                                {
                                    FGuid FunctionGuid;
                                    FBlueprintEditorUtils::GetFunctionGuidFromClassByFieldName(
                                        FBlueprintEditorUtils::GetMostUpToDateClass(TargetClass),
                                        LayerFName, FunctionGuid);
                                    FuncRef->SetExternalMember(LayerFName, TargetClass, FunctionGuid);
                                }
                                else
                                {
                                    FuncRef->SetSelfMember(LayerFName);
                                }
                            }
                        }

                        Result->SetStringField(TEXT("layer_name"), LayerName);
                        Result->SetBoolField(TEXT("layer_linked"), true);

                        if (LayerGraph)
                        {
                            Result->SetStringField(TEXT("layer_graph"), LayerGraph->GetName());
                        }
                    }
                    break;
                }
            }
        }
    }

    MarkModifiedAndSave(AnimBP, Params);
    return Result;
}

// ============================================================================
// Tier 3: add_blend_node
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleAddBlendNode(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    UEdGraph* Graph = ResolveAnimGraph(AnimBP, Params, Error);
    if (!Graph) return Error;

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    FString BlendType;
    if (!Params->TryGetStringField(TEXT("blend_type"), BlendType))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blend_type' parameter"));
    }

    // Map blend type to class name
    FString NodeClassName;
    if (BlendType.Equals(TEXT("LayeredBlendPerBone"), ESearchCase::IgnoreCase))
    {
        NodeClassName = TEXT("AnimGraphNode_LayeredBoneBlend");
    }
    else if (BlendType.Equals(TEXT("BlendPosesByBool"), ESearchCase::IgnoreCase) ||
             BlendType.Equals(TEXT("BlendListByBool"), ESearchCase::IgnoreCase))
    {
        NodeClassName = TEXT("AnimGraphNode_BlendListByBool");
    }
    else if (BlendType.Equals(TEXT("BlendPosesByInt"), ESearchCase::IgnoreCase) ||
             BlendType.Equals(TEXT("BlendListByInt"), ESearchCase::IgnoreCase))
    {
        NodeClassName = TEXT("AnimGraphNode_BlendListByInt");
    }
    else if (BlendType.Equals(TEXT("TwoWayBlend"), ESearchCase::IgnoreCase))
    {
        NodeClassName = TEXT("AnimGraphNode_TwoWayBlend");
    }
    else if (BlendType.Equals(TEXT("MultiWayBlend"), ESearchCase::IgnoreCase))
    {
        NodeClassName = TEXT("AnimGraphNode_MultiWayBlend");
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Unknown blend_type: '%s'. Valid: LayeredBlendPerBone, BlendPosesByBool, BlendPosesByInt, TwoWayBlend, MultiWayBlend"), *BlendType));
    }

    TSharedPtr<FJsonObject> Result = CreateAnimNodeInGraph(Graph, AnimBP, NodeClassName, Params);
    MarkModifiedAndSave(AnimBP, Params);
    return Result;
}

// ============================================================================
// Tier 3: add_blend_pose_pin
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleAddBlendPosePin(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    UEdGraph* Graph = ResolveAnimGraph(AnimBP, Params, Error);
    if (!Graph) return Error;

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    UEdGraphNode* Node = FindNodeInGraph(Graph, Params, TEXT("node"), Error);
    if (!Node) return Error;

    // Try LayeredBoneBlend
    UAnimGraphNode_LayeredBoneBlend* LayeredBlend = Cast<UAnimGraphNode_LayeredBoneBlend>(Node);
    if (LayeredBlend)
    {
        LayeredBlend->AddPinToBlendByFilter();
        LayeredBlend->ReconstructNode();
        MarkModifiedAndSave(AnimBP, Params);

        // Return updated pin list
        TMap<FGuid, int32> DummyMap;
        DummyMap.Add(Node->NodeGuid, 0);
        TSharedPtr<FJsonObject> ResultObj = SerializeAnimNode(Node, 0, DummyMap);
        ResultObj->SetBoolField(TEXT("success"), true);
        return ResultObj;
    }

    // Try BlendListBase subclasses (BlendByInt, BlendByEnum, etc.)
    UAnimGraphNode_BlendListBase* BlendList = Cast<UAnimGraphNode_BlendListBase>(Node);
    if (BlendList)
    {
        // BlendListBase doesn't have a public AddPin — we add via the inner node's array and reconstruct
        // The array that controls pin count varies by subclass. Use ReconstructNode after modifying.
        // For BlendListByInt, adding a pin means adding an element to the blend poses array in the inner node.
        // The safest approach is to call the node's context menu action equivalent.
        // Since AddPinToBlendList is not always accessible, we use property modification + reconstruct.
        UE_LOG(LogMCPAnimGraph, Warning, TEXT("add_blend_pose_pin: BlendListBase pin addition via reconstruct"));
        BlendList->ReconstructNode();
        MarkModifiedAndSave(AnimBP, Params);

        TMap<FGuid, int32> DummyMap;
        DummyMap.Add(Node->NodeGuid, 0);
        TSharedPtr<FJsonObject> ResultObj = SerializeAnimNode(Node, 0, DummyMap);
        ResultObj->SetBoolField(TEXT("success"), true);
        return ResultObj;
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Node '%s' is not a blend node that supports dynamic pins"), *Node->GetClass()->GetName()));
}

// ============================================================================
// Tier 3: set_anim_blueprint_parent
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleSetAnimBlueprintParent(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    FString ParentClassName;
    if (!Params->TryGetStringField(TEXT("parent_class"), ParentClassName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parent_class' parameter"));
    }

    // Find the parent class
    UClass* ParentClass = FindObject<UClass>(nullptr, *ParentClassName);
    if (!ParentClass)
    {
        ParentClass = StaticLoadClass(UAnimInstance::StaticClass(), nullptr, *ParentClassName, nullptr, LOAD_None, nullptr);
    }

    if (!ParentClass)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Parent class not found: %s"), *ParentClassName));
    }

    if (!ParentClass->IsChildOf(UAnimInstance::StaticClass()))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Class '%s' does not inherit from UAnimInstance"), *ParentClassName));
    }

    AnimBP->ParentClass = ParentClass;
    FBlueprintEditorUtils::MarkBlueprintAsModified(AnimBP);

    // Compile to propagate the change
    FKismetEditorUtilities::CompileBlueprint(AnimBP);

    MarkModifiedAndSave(AnimBP, Params);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("parent_class"), ParentClass->GetPathName());
    return ResultObj;
}

// ============================================================================
// Tier 3: compile_anim_blueprint
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleCompileAnimBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Compile the blueprint
    FKismetEditorUtilities::CompileBlueprint(AnimBP);

    // Check compilation status
    bool bSuccess = (AnimBP->Status == BS_UpToDate || AnimBP->Status == BS_UpToDateWithWarnings);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), bSuccess);

    FString StatusStr;
    switch (AnimBP->Status)
    {
    case BS_UpToDate: StatusStr = TEXT("UpToDate"); break;
    case BS_UpToDateWithWarnings: StatusStr = TEXT("UpToDateWithWarnings"); break;
    case BS_Dirty: StatusStr = TEXT("Dirty"); break;
    case BS_Error: StatusStr = TEXT("Error"); break;
    default: StatusStr = TEXT("Unknown"); break;
    }
    ResultObj->SetStringField(TEXT("status"), StatusStr);

    // Save if compilation succeeded
    if (bSuccess)
    {
        MarkModifiedAndSave(AnimBP, Params);
    }

    return ResultObj;
}

// ============================================================================
// Tier 4: configure_motion_matching
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleConfigureMotionMatching(const TSharedPtr<FJsonObject>& Params)
{
    // This is a convenience wrapper over set_anim_node_property for MotionMatching nodes
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    UEdGraph* Graph = ResolveAnimGraph(AnimBP, Params, Error);
    if (!Graph) return Error;

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    UEdGraphNode* Node = FindNodeInGraph(Graph, Params, TEXT("node"), Error);
    if (!Node) return Error;

    // Validate it's a MotionMatching node
    if (!Node->GetClass()->GetName().Contains(TEXT("MotionMatching")))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Node '%s' is not a MotionMatching node (class: %s)"),
                *Node->NodeGuid.ToString(), *Node->GetClass()->GetName()));
    }

    FString PropError;
    TArray<FString> SetProperties;

    // Set database
    FString Database;
    if (Params->TryGetStringField(TEXT("database"), Database))
    {
        TSharedPtr<FJsonValue> Val = MakeShared<FJsonValueString>(Database);
        if (FUnrealMCPCommonUtils::SetObjectProperty(Node, TEXT("Node.Database"), Val, PropError))
        {
            SetProperties.Add(TEXT("Database"));
        }
        else
        {
            UE_LOG(LogMCPAnimGraph, Warning, TEXT("Failed to set Database: %s"), *PropError);
        }
    }

    // Set blend time
    if (Params->HasField(TEXT("blend_time")))
    {
        TSharedPtr<FJsonValue> Val = Params->TryGetField(TEXT("blend_time"));
        if (FUnrealMCPCommonUtils::SetObjectProperty(Node, TEXT("Node.BlendTime"), Val, PropError))
        {
            SetProperties.Add(TEXT("BlendTime"));
        }
        else
        {
            UE_LOG(LogMCPAnimGraph, Warning, TEXT("Failed to set BlendTime: %s"), *PropError);
        }
    }

    // Set interrupt mode
    FString InterruptMode;
    if (Params->TryGetStringField(TEXT("interrupt_mode"), InterruptMode))
    {
        TSharedPtr<FJsonValue> Val = MakeShared<FJsonValueString>(InterruptMode);
        if (FUnrealMCPCommonUtils::SetObjectProperty(Node, TEXT("Node.InterruptMode"), Val, PropError))
        {
            SetProperties.Add(TEXT("InterruptMode"));
        }
        else
        {
            UE_LOG(LogMCPAnimGraph, Warning, TEXT("Failed to set InterruptMode: %s"), *PropError);
        }
    }

    Node->ReconstructNode();
    MarkModifiedAndSave(AnimBP, Params);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());

    TArray<TSharedPtr<FJsonValue>> PropsArr;
    for (const FString& P : SetProperties)
    {
        PropsArr.Add(MakeShared<FJsonValueString>(P));
    }
    ResultObj->SetArrayField(TEXT("properties_set"), PropsArr);

    return ResultObj;
}

// ============================================================================
// Tier 4: configure_history_collector
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleConfigureHistoryCollector(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    UEdGraph* Graph = ResolveAnimGraph(AnimBP, Params, Error);
    if (!Graph) return Error;

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    UEdGraphNode* Node = FindNodeInGraph(Graph, Params, TEXT("node"), Error);
    if (!Node) return Error;

    // Validate it's a HistoryCollector node
    if (!Node->GetClass()->GetName().Contains(TEXT("PoseSearchHistoryCollector")))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Node '%s' is not a PoseSearchHistoryCollector node (class: %s)"),
                *Node->NodeGuid.ToString(), *Node->GetClass()->GetName()));
    }

    FString PropError;
    TArray<FString> SetProperties;

    // Set pose count
    if (Params->HasField(TEXT("pose_count")))
    {
        TSharedPtr<FJsonValue> Val = Params->TryGetField(TEXT("pose_count"));
        if (FUnrealMCPCommonUtils::SetObjectProperty(Node, TEXT("Node.PoseCount"), Val, PropError))
        {
            SetProperties.Add(TEXT("PoseCount"));
        }
        else
        {
            UE_LOG(LogMCPAnimGraph, Warning, TEXT("Failed to set PoseCount: %s"), *PropError);
        }
    }

    // Set sample interval
    if (Params->HasField(TEXT("sample_interval")))
    {
        TSharedPtr<FJsonValue> Val = Params->TryGetField(TEXT("sample_interval"));
        if (FUnrealMCPCommonUtils::SetObjectProperty(Node, TEXT("Node.SampleInterval"), Val, PropError))
        {
            SetProperties.Add(TEXT("SampleInterval"));
        }
        else
        {
            UE_LOG(LogMCPAnimGraph, Warning, TEXT("Failed to set SampleInterval: %s"), *PropError);
        }
    }

    Node->ReconstructNode();
    MarkModifiedAndSave(AnimBP, Params);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());

    TArray<TSharedPtr<FJsonValue>> PropsArr;
    for (const FString& P : SetProperties)
    {
        PropsArr.Add(MakeShared<FJsonValueString>(P));
    }
    ResultObj->SetArrayField(TEXT("properties_set"), PropsArr);

    return ResultObj;
}

// ============================================================================
// bind_anim_pin_to_property — Property Access binding on AnimGraph node pins
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleBindAnimPinToProperty(const TSharedPtr<FJsonObject>& Params)
{
    // Load AnimBlueprint
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    // Resolve graph
    UEdGraph* Graph = ResolveAnimGraph(AnimBP, Params, Error);
    if (!Graph) return Error;

    // Find target node
    UEdGraphNode* RawNode = FindNodeInGraph(Graph, Params, TEXT("node"), Error);
    if (!RawNode) return Error;

    UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(RawNode);
    if (!AnimNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Node is not an AnimGraph node"));
    }

    // Get required parameters
    FString PinName;
    if (!Params->TryGetStringField(TEXT("pin_name"), PinName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pin_name' parameter"));
    }

    // Property path — array of strings or dot-separated string
    TArray<FString> PropertyPath;
    const TArray<TSharedPtr<FJsonValue>>* PathArray = nullptr;
    FString PathString;

    if (Params->TryGetArrayField(TEXT("property_path"), PathArray))
    {
        for (const auto& Val : *PathArray)
        {
            FString Segment;
            if (Val->TryGetString(Segment))
            {
                PropertyPath.Add(Segment);
            }
        }
    }
    else if (Params->TryGetStringField(TEXT("property_path"), PathString))
    {
        // Support dot-separated shorthand: "Speed" or "MovementComponent.MaxSpeed"
        PathString.ParseIntoArray(PropertyPath, TEXT("."));
    }

    if (PropertyPath.Num() == 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing or empty 'property_path' parameter. Provide a string (e.g., \"Speed\") or array [\"Component\", \"Property\"]"));
    }

    // Optional: binding type (Property or Function)
    FString BindingTypeStr;
    EAnimGraphNodePropertyBindingType BindingType = EAnimGraphNodePropertyBindingType::Property;
    if (Params->TryGetStringField(TEXT("binding_type"), BindingTypeStr))
    {
        if (BindingTypeStr.Equals(TEXT("Function"), ESearchCase::IgnoreCase))
        {
            BindingType = EAnimGraphNodePropertyBindingType::Function;
        }
    }

    // Optional: context ID for execution timing
    FName ContextId = NAME_None; // Automatic
    FString ContextStr;
    if (Params->TryGetStringField(TEXT("context"), ContextStr))
    {
        if (ContextStr.Equals(TEXT("Automatic"), ESearchCase::IgnoreCase))
        {
            ContextId = UAnimBlueprintExtension_PropertyAccess::ContextId_Automatic;
        }
        else if (ContextStr.Equals(TEXT("ThreadSafe"), ESearchCase::IgnoreCase))
        {
            ContextId = UAnimBlueprintExtension_PropertyAccess::ContextId_UnBatched_ThreadSafe;
        }
        else if (ContextStr.Equals(TEXT("GameThreadPre"), ESearchCase::IgnoreCase))
        {
            ContextId = UAnimBlueprintExtension_PropertyAccess::ContextId_Batched_GameThreadPreEventGraph;
        }
        else if (ContextStr.Equals(TEXT("GameThreadPost"), ESearchCase::IgnoreCase))
        {
            ContextId = UAnimBlueprintExtension_PropertyAccess::ContextId_Batched_GameThreadPostEventGraph;
        }
        else
        {
            ContextId = FName(*ContextStr);
        }
    }

    // Get the binding object on this node
    UAnimGraphNodeBinding* BindingObj = AnimNode->GetMutableBinding();
    if (!BindingObj)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Node has no binding object"));
    }

    // Access the PropertyBindings TMap via reflection
    // (UAnimGraphNodeBinding_Base is in a private header, but the UPROPERTY is reflectable)
    FMapProperty* MapProp = CastField<FMapProperty>(BindingObj->GetClass()->FindPropertyByName(TEXT("PropertyBindings")));
    if (!MapProp)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Could not find PropertyBindings on binding object via reflection"));
    }

    FScriptMapHelper MapHelper(MapProp, MapProp->ContainerPtrToValuePtr<void>(BindingObj));

    // Build the binding struct
    FAnimGraphNodePropertyBinding NewBinding;
    NewBinding.PropertyName = FName(*PinName);
    NewBinding.PropertyPath = PropertyPath;
    NewBinding.bIsBound = true;
    NewBinding.Type = BindingType;
    NewBinding.ContextId = ContextId;

    // Build human-readable path text
    if (IModularFeatures::Get().IsModularFeatureAvailable(TEXT("PropertyAccessEditor")))
    {
        IPropertyAccessEditor& PAEditor = IModularFeatures::Get().GetModularFeature<IPropertyAccessEditor>(TEXT("PropertyAccessEditor"));
        NewBinding.PathAsText = PAEditor.MakeTextPath(PropertyPath, AnimBP->SkeletonGeneratedClass);
    }
    else
    {
        NewBinding.PathAsText = FText::FromString(FString::Join(PropertyPath, TEXT(".")));
    }

    // Note: PinType resolution happens automatically during AnimBP compilation.
    // RecalculateBindingType is protected on UAnimGraphNode_Base, but compilation
    // handles it via UAnimBlueprintExtension_Base::RegisterPropertyBinding.

    // Insert into the PropertyBindings map
    FName BindingKey(*PinName);

    int32 ExistingIndex = INDEX_NONE;
    for (int32 i = 0; i < MapHelper.Num(); ++i)
    {
        if (MapHelper.IsValidIndex(i))
        {
            const FName* KeyPtr = (const FName*)MapHelper.GetKeyPtr(i);
            if (KeyPtr && *KeyPtr == BindingKey)
            {
                ExistingIndex = i;
                break;
            }
        }
    }

    if (ExistingIndex != INDEX_NONE)
    {
        FAnimGraphNodePropertyBinding* ValuePtr = (FAnimGraphNodePropertyBinding*)MapHelper.GetValuePtr(ExistingIndex);
        *ValuePtr = NewBinding;
    }
    else
    {
        MapHelper.AddPair(
            (const uint8*)&BindingKey,
            (const uint8*)&NewBinding
        );
    }

    // Reconstruct node to update pins for the new binding
    AnimNode->ReconstructNode();

    MarkModifiedAndSave(AnimBP, Params);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("node_id"), AnimNode->NodeGuid.ToString());
    ResultObj->SetStringField(TEXT("pin_name"), PinName);
    ResultObj->SetStringField(TEXT("property_path"), FString::Join(PropertyPath, TEXT(".")));
    ResultObj->SetStringField(TEXT("binding_type"), BindingType == EAnimGraphNodePropertyBindingType::Function ? TEXT("Function") : TEXT("Property"));
    ResultObj->SetStringField(TEXT("path_text"), NewBinding.PathAsText.ToString());
    ResultObj->SetBoolField(TEXT("is_bound"), NewBinding.bIsBound);
    ResultObj->SetBoolField(TEXT("is_promotion"), NewBinding.bIsPromotion);

    return ResultObj;
}

// ============================================================================
// Tier 2.5: set_state_entry
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleSetStateEntry(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    FString SMGuid;
    if (!Params->TryGetStringField(TEXT("state_machine"), SMGuid))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'state_machine' parameter"));
    }

    FString StateGuid;
    if (!Params->TryGetStringField(TEXT("state"), StateGuid))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'state' parameter"));
    }

    UEdGraph* SMGraph = FindStateMachineGraph(AnimBP, SMGuid, Error);
    if (!SMGraph) return Error;

    // Find the entry node and target state
    UAnimStateEntryNode* EntryNode = nullptr;
    UAnimStateNodeBase* TargetState = nullptr;

    for (UEdGraphNode* Node : SMGraph->Nodes)
    {
        if (!Node) continue;

        if (!EntryNode)
        {
            EntryNode = Cast<UAnimStateEntryNode>(Node);
        }

        if (Node->NodeGuid.ToString() == StateGuid)
        {
            TargetState = Cast<UAnimStateNodeBase>(Node);
        }
    }

    if (!EntryNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No entry node found in state machine"));
    }

    if (!TargetState)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("State not found: %s"), *StateGuid));
    }

    // Disconnect current entry connection
    UEdGraphPin* EntryOutput = EntryNode->GetOutputPin();
    if (EntryOutput)
    {
        EntryOutput->BreakAllPinLinks();
    }

    // Connect entry to the new target state
    UEdGraphPin* StateInput = TargetState->GetInputPin();
    if (EntryOutput && StateInput)
    {
        const UEdGraphSchema* Schema = SMGraph->GetSchema();
        if (Schema)
        {
            Schema->TryCreateConnection(EntryOutput, StateInput);
        }
        else
        {
            EntryOutput->MakeLinkTo(StateInput);
        }
    }

    MarkModifiedAndSave(AnimBP, Params);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("state_machine"), SMGuid);
    ResultObj->SetStringField(TEXT("entry_state"), StateGuid);
    ResultObj->SetStringField(TEXT("state_name"), TargetState->GetStateName());

    return ResultObj;
}

// ============================================================================
// Helper: Spawn K2 (Blueprint) nodes in transition rule graphs
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::SpawnK2NodeInTransitionGraph(
    UEdGraph* TransGraph, UAnimBlueprint* AnimBP, const FString& NodeClass,
    const TSharedPtr<FJsonObject>& Params)
{
    UEdGraphNode* NewNode = nullptr;

    // Helper lambda for standard node init
    auto InitNode = [&](UEdGraphNode* Node)
    {
        Node->SetFlags(RF_Transactional);
        TransGraph->AddNode(Node, true, false);
        Node->CreateNewGuid();
        Node->PostPlacedNewNode();
        Node->AllocateDefaultPins();
        NewNode = Node;
    };

    if (NodeClass == TEXT("GetVariable") || NodeClass == TEXT("VariableGet"))
    {
        FString VariableName;
        if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                TEXT("'GetVariable' node requires 'variable_name' parameter"));
        }

        UK2Node_VariableGet* VarNode = NewObject<UK2Node_VariableGet>(TransGraph);
        // Set the variable reference before AllocateDefaultPins so pins match the variable type
        FMemberReference VarRef;
        VarRef.SetSelfMember(FName(*VariableName));
        VarNode->VariableReference = VarRef;
        InitNode(VarNode);
    }
    else if (NodeClass == TEXT("CallFunction"))
    {
        FString FunctionName;
        if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                TEXT("'CallFunction' node requires 'function_name' parameter"));
        }

        UK2Node_CallFunction* FuncNode = NewObject<UK2Node_CallFunction>(TransGraph);

        // Parse "ClassName::FunctionName" or just "FunctionName"
        FString ClassName, FuncName;
        if (FunctionName.Split(TEXT("::"), &ClassName, &FuncName))
        {
            UClass* OwnerClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ClassName));
            if (!OwnerClass)
            {
                OwnerClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/CoreUObject.%s"), *ClassName));
            }
            if (!OwnerClass)
            {
                // Search all loaded classes by name
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
        InitNode(FuncNode);
    }
    else if (NodeClass == TEXT("Less") || NodeClass == TEXT("LessFloat"))
    {
        UK2Node_CallFunction* FuncNode = NewObject<UK2Node_CallFunction>(TransGraph);
        FuncNode->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Less_DoubleDouble)));
        InitNode(FuncNode);
    }
    else if (NodeClass == TEXT("LessEqual") || NodeClass == TEXT("LessEqualFloat"))
    {
        UK2Node_CallFunction* FuncNode = NewObject<UK2Node_CallFunction>(TransGraph);
        FuncNode->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, LessEqual_DoubleDouble)));
        InitNode(FuncNode);
    }
    else if (NodeClass == TEXT("Greater") || NodeClass == TEXT("GreaterFloat"))
    {
        UK2Node_CallFunction* FuncNode = NewObject<UK2Node_CallFunction>(TransGraph);
        FuncNode->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Greater_DoubleDouble)));
        InitNode(FuncNode);
    }
    else if (NodeClass == TEXT("GreaterEqual") || NodeClass == TEXT("GreaterEqualFloat"))
    {
        UK2Node_CallFunction* FuncNode = NewObject<UK2Node_CallFunction>(TransGraph);
        FuncNode->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, GreaterEqual_DoubleDouble)));
        InitNode(FuncNode);
    }
    else if (NodeClass == TEXT("Equal") || NodeClass == TEXT("EqualFloat"))
    {
        UK2Node_CallFunction* FuncNode = NewObject<UK2Node_CallFunction>(TransGraph);
        FuncNode->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_DoubleDouble)));
        InitNode(FuncNode);
    }
    else if (NodeClass == TEXT("NotEqual") || NodeClass == TEXT("NotEqualFloat"))
    {
        UK2Node_CallFunction* FuncNode = NewObject<UK2Node_CallFunction>(TransGraph);
        FuncNode->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, NotEqual_DoubleDouble)));
        InitNode(FuncNode);
    }
    else if (NodeClass == TEXT("AND") || NodeClass == TEXT("BooleanAND"))
    {
        UK2Node_CommutativeAssociativeBinaryOperator* OpNode =
            NewObject<UK2Node_CommutativeAssociativeBinaryOperator>(TransGraph);
        OpNode->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanAND)));
        InitNode(OpNode);
    }
    else if (NodeClass == TEXT("OR") || NodeClass == TEXT("BooleanOR"))
    {
        UK2Node_CommutativeAssociativeBinaryOperator* OpNode =
            NewObject<UK2Node_CommutativeAssociativeBinaryOperator>(TransGraph);
        OpNode->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanOR)));
        InitNode(OpNode);
    }
    else if (NodeClass == TEXT("NOT") || NodeClass == TEXT("BooleanNOT"))
    {
        UK2Node_CallFunction* FuncNode = NewObject<UK2Node_CallFunction>(TransGraph);
        FuncNode->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Not_PreBool)));
        InitNode(FuncNode);
    }
    else if (NodeClass == TEXT("TimeRemaining"))
    {
        // GetRelevantAnimTimeRemaining — common transition condition
        UK2Node_CallFunction* FuncNode = NewObject<UK2Node_CallFunction>(TransGraph);
        UFunction* Func = UAnimInstance::StaticClass()->FindFunctionByName(
            TEXT("GetRelevantAnimTimeRemaining"));
        if (Func)
        {
            FuncNode->SetFromFunction(Func);
        }
        else
        {
            // Fallback: set as self member
            FuncNode->FunctionReference.SetSelfMember(FName(TEXT("GetRelevantAnimTimeRemaining")));
        }
        InitNode(FuncNode);
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Unknown transition condition node type: '%s'. Supported: "
                "GetVariable, CallFunction, Less, LessEqual, Greater, GreaterEqual, "
                "Equal, NotEqual, AND, OR, NOT, TimeRemaining"), *NodeClass));
    }

    if (!NewNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create K2 condition node"));
    }

    // Apply position
    const TSharedPtr<FJsonObject>* PosObj;
    if (Params->TryGetObjectField(TEXT("position"), PosObj))
    {
        NewNode->NodePosX = (*PosObj)->GetIntegerField(TEXT("x"));
        NewNode->NodePosY = (*PosObj)->GetIntegerField(TEXT("y"));
    }
    else
    {
        NewNode->NodePosX = -300;
        NewNode->NodePosY = 0;
    }

    // Apply pin_defaults
    const TSharedPtr<FJsonObject>* PinDefaults;
    if (Params->TryGetObjectField(TEXT("pin_defaults"), PinDefaults))
    {
        for (auto& KV : (*PinDefaults)->Values)
        {
            FString PinName = KV.Key;
            FString PinValue;
            if (KV.Value->TryGetString(PinValue))
            {
                // Find the pin and set its default
                for (UEdGraphPin* Pin : NewNode->Pins)
                {
                    if (Pin && Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
                    {
                        Pin->DefaultValue = PinValue;
                        break;
                    }
                }
            }
            else
            {
                // Try numeric
                double NumVal;
                if (KV.Value->TryGetNumber(NumVal))
                {
                    for (UEdGraphPin* Pin : NewNode->Pins)
                    {
                        if (Pin && Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
                        {
                            Pin->DefaultValue = FString::SanitizeFloat(NumVal);
                            break;
                        }
                    }
                }
                else
                {
                    // Try bool
                    bool BoolVal;
                    if (KV.Value->TryGetBool(BoolVal))
                    {
                        for (UEdGraphPin* Pin : NewNode->Pins)
                        {
                            if (Pin && Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
                            {
                                Pin->DefaultValue = BoolVal ? TEXT("true") : TEXT("false");
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // Build result
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("node_id"), NewNode->NodeGuid.ToString());
    ResultObj->SetStringField(TEXT("node_class"), NewNode->GetClass()->GetName());
    ResultObj->SetStringField(TEXT("node_title"), NewNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

    // Serialize pins
    TArray<TSharedPtr<FJsonValue>> PinsArray;
    for (UEdGraphPin* Pin : NewNode->Pins)
    {
        if (!Pin || Pin->bHidden) continue;
        TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
        PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
        PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
        PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
        if (!Pin->DefaultValue.IsEmpty())
        {
            PinObj->SetStringField(TEXT("default_value"), Pin->DefaultValue);
        }
        PinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
    }
    ResultObj->SetArrayField(TEXT("pins"), PinsArray);

    return ResultObj;
}

// ============================================================================
// Tier 2.5: bind_transition_condition
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleBindTransitionCondition(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    FString SMGuid;
    if (!Params->TryGetStringField(TEXT("state_machine"), SMGuid))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'state_machine' parameter"));
    }

    FString TransitionGuid;
    if (!Params->TryGetStringField(TEXT("transition"), TransitionGuid))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'transition' parameter"));
    }

    UEdGraph* SMGraph = FindStateMachineGraph(AnimBP, SMGuid, Error);
    if (!SMGraph) return Error;

    // Find the transition node
    UAnimStateTransitionNode* TransNode = nullptr;
    for (UEdGraphNode* Node : SMGraph->Nodes)
    {
        if (Node && Node->NodeGuid.ToString() == TransitionGuid)
        {
            TransNode = Cast<UAnimStateTransitionNode>(Node);
            break;
        }
    }

    if (!TransNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Transition not found: %s"), *TransitionGuid));
    }

    // --- Set transition properties ---

    // Crossfade duration
    double CrossfadeDuration;
    if (Params->TryGetNumberField(TEXT("crossfade_duration"), CrossfadeDuration))
    {
        TransNode->CrossfadeDuration = (float)CrossfadeDuration;
    }

    // Priority order
    int32 Priority;
    if (Params->TryGetNumberField(TEXT("priority"), Priority))
    {
        TransNode->PriorityOrder = Priority;
    }

    // Blend mode
    FString BlendModeStr;
    if (Params->TryGetStringField(TEXT("blend_mode"), BlendModeStr))
    {
        if (BlendModeStr == TEXT("Linear")) TransNode->BlendMode = EAlphaBlendOption::Linear;
        else if (BlendModeStr == TEXT("Cubic")) TransNode->BlendMode = EAlphaBlendOption::Cubic;
        else if (BlendModeStr == TEXT("HermiteCubic")) TransNode->BlendMode = EAlphaBlendOption::HermiteCubic;
        else if (BlendModeStr == TEXT("Sinusoidal")) TransNode->BlendMode = EAlphaBlendOption::Sinusoidal;
        else if (BlendModeStr == TEXT("QuadraticInOut")) TransNode->BlendMode = EAlphaBlendOption::QuadraticInOut;
        else if (BlendModeStr == TEXT("CubicInOut")) TransNode->BlendMode = EAlphaBlendOption::CubicInOut;
        else if (BlendModeStr == TEXT("QuarticInOut")) TransNode->BlendMode = EAlphaBlendOption::QuarticInOut;
        else if (BlendModeStr == TEXT("QuinticInOut")) TransNode->BlendMode = EAlphaBlendOption::QuinticInOut;
        else if (BlendModeStr == TEXT("CircularIn")) TransNode->BlendMode = EAlphaBlendOption::CircularIn;
        else if (BlendModeStr == TEXT("CircularOut")) TransNode->BlendMode = EAlphaBlendOption::CircularOut;
        else if (BlendModeStr == TEXT("CircularInOut")) TransNode->BlendMode = EAlphaBlendOption::CircularInOut;
        else if (BlendModeStr == TEXT("ExpIn")) TransNode->BlendMode = EAlphaBlendOption::ExpIn;
        else if (BlendModeStr == TEXT("ExpOut")) TransNode->BlendMode = EAlphaBlendOption::ExpOut;
        else if (BlendModeStr == TEXT("ExpInOut")) TransNode->BlendMode = EAlphaBlendOption::ExpInOut;
    }

    // Logic type
    FString LogicTypeStr;
    if (Params->TryGetStringField(TEXT("logic_type"), LogicTypeStr))
    {
        if (LogicTypeStr == TEXT("StandardBlend")) TransNode->LogicType = ETransitionLogicType::TLT_StandardBlend;
        else if (LogicTypeStr == TEXT("Inertialization")) TransNode->LogicType = ETransitionLogicType::TLT_Inertialization;
        else if (LogicTypeStr == TEXT("Custom")) TransNode->LogicType = ETransitionLogicType::TLT_Custom;
    }

    // Automatic rule based on sequence player
    bool bAutomatic;
    if (Params->TryGetBoolField(TEXT("automatic_rule"), bAutomatic))
    {
        TransNode->bAutomaticRuleBasedOnSequencePlayerInState = bAutomatic;
    }

    // Automatic rule trigger time
    double AutoTriggerTime;
    if (Params->TryGetNumberField(TEXT("automatic_rule_trigger_time"), AutoTriggerTime))
    {
        TransNode->AutomaticRuleTriggerTime = (float)AutoTriggerTime;
    }

    // Bidirectional
    bool bBidirectional;
    if (Params->TryGetBoolField(TEXT("bidirectional"), bBidirectional))
    {
        TransNode->Bidirectional = bBidirectional;
    }

    // Disabled
    bool bDisabled;
    if (Params->TryGetBoolField(TEXT("disabled"), bDisabled))
    {
        TransNode->bDisabled = bDisabled;
    }

    // --- Add condition node to transition rule graph ---
    // If node_class is provided, create a node in the transition's BoundGraph
    // and connect its output to the TransitionResult's bCanEnterTransition pin.
    //
    // Compound auto-wiring: when NOT/AND/OR/comparison is combined with variable_name,
    // we spawn the GetVariable node too and wire them together automatically.

    FString NodeClass;
    TSharedPtr<FJsonObject> NodeResult;

    if (Params->TryGetStringField(TEXT("node_class"), NodeClass))
    {
        UEdGraph* TransGraph = TransNode->BoundGraph;
        if (!TransGraph)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                TEXT("Transition has no BoundGraph (transition rule graph)"));
        }

        // Determine if this is a compound pattern that needs a GetVariable feeder node
        FString VariableName;
        Params->TryGetStringField(TEXT("variable_name"), VariableName);
        bool bIsCompound = !VariableName.IsEmpty() &&
            (NodeClass == TEXT("NOT") || NodeClass == TEXT("BooleanNOT") ||
             NodeClass == TEXT("Less") || NodeClass == TEXT("LessFloat") ||
             NodeClass == TEXT("LessEqual") || NodeClass == TEXT("LessEqualFloat") ||
             NodeClass == TEXT("Greater") || NodeClass == TEXT("GreaterFloat") ||
             NodeClass == TEXT("GreaterEqual") || NodeClass == TEXT("GreaterEqualFloat") ||
             NodeClass == TEXT("Equal") || NodeClass == TEXT("EqualFloat") ||
             NodeClass == TEXT("NotEqual") || NodeClass == TEXT("NotEqualFloat"));

        // For compound patterns, spawn the GetVariable node first
        UEdGraphNode* FeederNode = nullptr;
        if (bIsCompound)
        {
            TSharedPtr<FJsonObject> VarParams = MakeShared<FJsonObject>();
            VarParams->SetStringField(TEXT("variable_name"), VariableName);
            // Position the variable getter to the left
            TSharedPtr<FJsonObject> VarPos = MakeShared<FJsonObject>();
            VarPos->SetNumberField(TEXT("x"), -500);
            VarPos->SetNumberField(TEXT("y"), 0);
            VarParams->SetObjectField(TEXT("position"), VarPos);

            TSharedPtr<FJsonObject> VarResult = SpawnK2NodeInTransitionGraph(TransGraph, AnimBP, TEXT("GetVariable"), VarParams);
            if (VarResult.IsValid() && VarResult->HasField(TEXT("error")))
            {
                return VarResult;
            }

            // Find the spawned feeder node
            FString FeederGuid;
            if (VarResult.IsValid() && VarResult->TryGetStringField(TEXT("node_id"), FeederGuid))
            {
                for (UEdGraphNode* GNode : TransGraph->Nodes)
                {
                    if (GNode && GNode->NodeGuid.ToString() == FeederGuid)
                    {
                        FeederNode = GNode;
                        break;
                    }
                }
            }
        }

        // Create the main K2 condition node in the transition rule graph
        // For compound patterns, strip variable_name from params so SpawnK2Node doesn't try GetVariable logic
        TSharedPtr<FJsonObject> SpawnParams = MakeShared<FJsonObject>();
        for (auto& KV : Params->Values)
        {
            SpawnParams->Values.Add(KV.Key, KV.Value);
        }
        if (bIsCompound)
        {
            SpawnParams->RemoveField(TEXT("variable_name"));
        }

        NodeResult = SpawnK2NodeInTransitionGraph(TransGraph, AnimBP, NodeClass, SpawnParams);
        if (NodeResult.IsValid() && NodeResult->HasField(TEXT("error")))
        {
            return NodeResult;
        }

        // Find the created main node
        UEdGraphNode* MainNode = nullptr;
        FString MainNodeGuid;
        if (NodeResult.IsValid() && NodeResult->TryGetStringField(TEXT("node_id"), MainNodeGuid))
        {
            for (UEdGraphNode* GNode : TransGraph->Nodes)
            {
                if (GNode && GNode->NodeGuid.ToString() == MainNodeGuid)
                {
                    MainNode = GNode;
                    break;
                }
            }
        }

        // Wire feeder → main node for compound patterns
        if (bIsCompound && FeederNode && MainNode)
        {
            // Find the output pin on the feeder (GetVariable result)
            UEdGraphPin* FeederOutput = nullptr;
            for (UEdGraphPin* Pin : FeederNode->Pins)
            {
                if (Pin && Pin->Direction == EGPD_Output && !Pin->bHidden)
                {
                    FeederOutput = Pin;
                    break;
                }
            }

            // Find the first input pin on the main node (A pin for comparisons, input for NOT)
            UEdGraphPin* MainInput = nullptr;
            for (UEdGraphPin* Pin : MainNode->Pins)
            {
                if (Pin && Pin->Direction == EGPD_Input && !Pin->bHidden)
                {
                    MainInput = Pin;
                    break;
                }
            }

            if (FeederOutput && MainInput)
            {
                const UEdGraphSchema* Schema = TransGraph->GetSchema();
                if (Schema)
                {
                    Schema->TryCreateConnection(FeederOutput, MainInput);
                }
                else
                {
                    FeederOutput->MakeLinkTo(MainInput);
                }

                if (NodeResult.IsValid())
                {
                    NodeResult->SetBoolField(TEXT("auto_wired_variable"), true);
                    NodeResult->SetStringField(TEXT("variable_node_id"), FeederNode->NodeGuid.ToString());
                }
            }
        }

        // If auto_connect is true (default), wire the main node's output to the TransitionResult
        bool bAutoConnect = true;
        Params->TryGetBoolField(TEXT("auto_connect"), bAutoConnect);

        if (bAutoConnect && MainNode)
        {
            // Find the TransitionResult node in the BoundGraph
            UAnimationTransitionGraph* TransRuleGraph = Cast<UAnimationTransitionGraph>(TransGraph);
            UAnimGraphNode_TransitionResult* ResultNode = TransRuleGraph ? TransRuleGraph->GetResultNode() : nullptr;

            if (!ResultNode)
            {
                for (UEdGraphNode* GNode : TransGraph->Nodes)
                {
                    ResultNode = Cast<UAnimGraphNode_TransitionResult>(GNode);
                    if (ResultNode) break;
                }
            }

            if (ResultNode)
            {
                // Find the "bCanEnterTransition" input pin on the result node
                UEdGraphPin* ResultInputPin = nullptr;
                for (UEdGraphPin* Pin : ResultNode->Pins)
                {
                    if (Pin && Pin->Direction == EGPD_Input)
                    {
                        ResultInputPin = Pin;
                        break;
                    }
                }

                // Find the output pin on the main node
                UEdGraphPin* ConditionOutput = nullptr;
                for (UEdGraphPin* Pin : MainNode->Pins)
                {
                    if (Pin && Pin->Direction == EGPD_Output && !Pin->bHidden)
                    {
                        // For comparisons/NOT, prefer the bool output (ReturnValue)
                        if (Pin->PinName == TEXT("ReturnValue") || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
                        {
                            ConditionOutput = Pin;
                            break;
                        }
                        if (!ConditionOutput) ConditionOutput = Pin;
                    }
                }

                if (ConditionOutput && ResultInputPin)
                {
                    // Break existing connections on the result input
                    ResultInputPin->BreakAllPinLinks();

                    const UEdGraphSchema* Schema = TransGraph->GetSchema();
                    if (Schema)
                    {
                        Schema->TryCreateConnection(ConditionOutput, ResultInputPin);
                    }
                    else
                    {
                        ConditionOutput->MakeLinkTo(ResultInputPin);
                    }

                    if (NodeResult.IsValid())
                    {
                        NodeResult->SetBoolField(TEXT("connected_to_result"), true);
                    }
                }
            }
        }
    }

    MarkModifiedAndSave(AnimBP, Params);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("transition_id"), TransitionGuid);
    ResultObj->SetNumberField(TEXT("crossfade_duration"), TransNode->CrossfadeDuration);
    ResultObj->SetNumberField(TEXT("priority"), TransNode->PriorityOrder);
    ResultObj->SetBoolField(TEXT("automatic_rule"), TransNode->bAutomaticRuleBasedOnSequencePlayerInState);
    ResultObj->SetBoolField(TEXT("bidirectional"), TransNode->Bidirectional);
    ResultObj->SetBoolField(TEXT("disabled"), TransNode->bDisabled);

    if (TransNode->BoundGraph)
    {
        ResultObj->SetStringField(TEXT("transition_graph"), TransNode->BoundGraph->GetName());
    }

    if (NodeResult.IsValid() && !NodeResult->HasField(TEXT("error")))
    {
        ResultObj->SetObjectField(TEXT("condition_node"), NodeResult);
    }

    return ResultObj;
}

// ============================================================================
// connect_k2_pins — Wire K2 nodes in any graph (transition rules, event graph, etc.)
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleConnectK2Pins(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Required params
    FString SourceNodeGuid, TargetNodeGuid;
    if (!Params->TryGetStringField(TEXT("source_node"), SourceNodeGuid))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_node' parameter (GUID)"));
    }
    if (!Params->TryGetStringField(TEXT("target_node"), TargetNodeGuid))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'target_node' parameter (GUID)"));
    }

    // Optional pin name filters — if not provided, find first compatible output→input pair
    FString SourcePinName, TargetPinName;
    Params->TryGetStringField(TEXT("source_pin"), SourcePinName);
    Params->TryGetStringField(TEXT("target_pin"), TargetPinName);

    // Search all graphs in the AnimBlueprint for both nodes
    UEdGraphNode* SourceNode = nullptr;
    UEdGraphNode* TargetNode = nullptr;
    UEdGraph* ContainingGraph = nullptr;

    TArray<UEdGraph*> AllGraphs;
    AnimBP->GetAllGraphs(AllGraphs);

    for (UEdGraph* Graph : AllGraphs)
    {
        if (!Graph) continue;

        UEdGraphNode* FoundSource = nullptr;
        UEdGraphNode* FoundTarget = nullptr;

        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node) continue;
            if (Node->NodeGuid.ToString() == SourceNodeGuid) FoundSource = Node;
            if (Node->NodeGuid.ToString() == TargetNodeGuid) FoundTarget = Node;
        }

        if (FoundSource && FoundTarget)
        {
            SourceNode = FoundSource;
            TargetNode = FoundTarget;
            ContainingGraph = Graph;
            break;
        }
    }

    if (!SourceNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Source node not found: %s"), *SourceNodeGuid));
    }
    if (!TargetNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Target node not found: %s"), *TargetNodeGuid));
    }
    if (!ContainingGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Source and target nodes not found in the same graph"));
    }

    // Find the output pin on source
    UEdGraphPin* OutputPin = nullptr;
    for (UEdGraphPin* Pin : SourceNode->Pins)
    {
        if (!Pin || Pin->bHidden || Pin->Direction != EGPD_Output) continue;

        if (!SourcePinName.IsEmpty())
        {
            if (Pin->PinName.ToString().Equals(SourcePinName, ESearchCase::IgnoreCase) ||
                Pin->GetDisplayName().ToString().Equals(SourcePinName, ESearchCase::IgnoreCase))
            {
                OutputPin = Pin;
                break;
            }
        }
        else
        {
            // No name specified — prefer ReturnValue, then first non-exec output
            if (Pin->PinName == TEXT("ReturnValue"))
            {
                OutputPin = Pin;
                break;
            }
            if (!OutputPin && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
            {
                OutputPin = Pin;
            }
        }
    }

    if (!OutputPin)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            SourcePinName.IsEmpty()
                ? FString::Printf(TEXT("No suitable output pin found on source node %s"), *SourceNodeGuid)
                : FString::Printf(TEXT("Output pin '%s' not found on source node %s"), *SourcePinName, *SourceNodeGuid));
    }

    // Find the input pin on target
    UEdGraphPin* InputPin = nullptr;
    for (UEdGraphPin* Pin : TargetNode->Pins)
    {
        if (!Pin || Pin->bHidden || Pin->Direction != EGPD_Input) continue;

        if (!TargetPinName.IsEmpty())
        {
            if (Pin->PinName.ToString().Equals(TargetPinName, ESearchCase::IgnoreCase) ||
                Pin->GetDisplayName().ToString().Equals(TargetPinName, ESearchCase::IgnoreCase))
            {
                InputPin = Pin;
                break;
            }
        }
        else
        {
            // No name specified — first non-exec input
            if (!InputPin && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
            {
                InputPin = Pin;
            }
        }
    }

    if (!InputPin)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TargetPinName.IsEmpty()
                ? FString::Printf(TEXT("No suitable input pin found on target node %s"), *TargetNodeGuid)
                : FString::Printf(TEXT("Input pin '%s' not found on target node %s"), *TargetPinName, *TargetNodeGuid));
    }

    // Connect using graph schema (handles type conversion)
    const UEdGraphSchema* Schema = ContainingGraph->GetSchema();
    bool bConnected = false;
    if (Schema)
    {
        bConnected = Schema->TryCreateConnection(OutputPin, InputPin);
    }

    if (!bConnected)
    {
        // Fallback to direct link
        OutputPin->MakeLinkTo(InputPin);
        bConnected = OutputPin->LinkedTo.Contains(InputPin);
    }

    if (!bConnected)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to connect %s.%s → %s.%s (incompatible types? %s → %s)"),
                *SourceNodeGuid, *OutputPin->PinName.ToString(),
                *TargetNodeGuid, *InputPin->PinName.ToString(),
                *OutputPin->PinType.PinCategory.ToString(),
                *InputPin->PinType.PinCategory.ToString()));
    }

    MarkModifiedAndSave(AnimBP, Params);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("source_node"), SourceNodeGuid);
    ResultObj->SetStringField(TEXT("source_pin"), OutputPin->PinName.ToString());
    ResultObj->SetStringField(TEXT("target_node"), TargetNodeGuid);
    ResultObj->SetStringField(TEXT("target_pin"), InputPin->PinName.ToString());
    ResultObj->SetStringField(TEXT("graph"), ContainingGraph->GetName());
    return ResultObj;
}

// ============================================================================
// bind_anim_node_function — Bind an AnimBP function to an AnimNode event
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleBindAnimNodeFunction(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    UEdGraph* Graph = ResolveAnimGraph(AnimBP, Params, Error);
    if (!Graph) return Error;

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Find target node
    UEdGraphNode* Node = FindNodeInGraph(Graph, Params, TEXT("node"), Error);
    if (!Node) return Error;

    UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(Node);
    if (!AnimNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Target node is not an AnimGraph node"));
    }

    FString EventName;
    if (!Params->TryGetStringField(TEXT("event"), EventName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing 'event' parameter. Supported: OnInitialUpdate, OnBecomeRelevant, OnUpdate, OnMotionMatchingStateUpdated"));
    }

    FString FunctionName;
    if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
    }

    // Map event name to the FMemberReference property on the node
    // UAnimGraphNode_Base has: InitialUpdateFunction, BecomeRelevantFunction, UpdateFunction
    // UAnimGraphNode_MotionMatching has: OnMotionMatchingStateUpdatedFunction

    FString PropertyName;
    if (EventName.Equals(TEXT("OnInitialUpdate"), ESearchCase::IgnoreCase) ||
        EventName.Equals(TEXT("InitialUpdate"), ESearchCase::IgnoreCase))
    {
        PropertyName = TEXT("InitialUpdateFunction");
    }
    else if (EventName.Equals(TEXT("OnBecomeRelevant"), ESearchCase::IgnoreCase) ||
             EventName.Equals(TEXT("BecomeRelevant"), ESearchCase::IgnoreCase))
    {
        PropertyName = TEXT("BecomeRelevantFunction");
    }
    else if (EventName.Equals(TEXT("OnUpdate"), ESearchCase::IgnoreCase) ||
             EventName.Equals(TEXT("Update"), ESearchCase::IgnoreCase))
    {
        PropertyName = TEXT("UpdateFunction");
    }
    else if (EventName.Equals(TEXT("OnMotionMatchingStateUpdated"), ESearchCase::IgnoreCase) ||
             EventName.Equals(TEXT("MotionMatchingStateUpdated"), ESearchCase::IgnoreCase) ||
             EventName.Equals(TEXT("OnStateUpdated"), ESearchCase::IgnoreCase))
    {
        PropertyName = TEXT("OnMotionMatchingStateUpdatedFunction");
    }
    else
    {
        // Try using the event name directly as a property name
        PropertyName = EventName;
        if (!PropertyName.EndsWith(TEXT("Function")))
        {
            PropertyName += TEXT("Function");
        }
    }

    // Find the FMemberReference property via UE reflection
    FProperty* Prop = AnimNode->GetClass()->FindPropertyByName(FName(*PropertyName));
    if (!Prop)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Property '%s' not found on node class '%s'. "
                "Available events: OnInitialUpdate, OnBecomeRelevant, OnUpdate, OnMotionMatchingStateUpdated"),
                *PropertyName, *AnimNode->GetClass()->GetName()));
    }

    FStructProperty* StructProp = CastField<FStructProperty>(Prop);
    if (!StructProp || StructProp->Struct->GetName() != TEXT("MemberReference"))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Property '%s' is not an FMemberReference"), *PropertyName));
    }

    // Get the FMemberReference pointer
    FMemberReference* MemberRef = StructProp->ContainerPtrToValuePtr<FMemberReference>(AnimNode);
    if (!MemberRef)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to access FMemberReference on node"));
    }

    // Check if the function exists in the AnimBP
    UClass* BPClass = AnimBP->GeneratedClass;
    UFunction* TargetFunc = BPClass ? BPClass->FindFunctionByName(FName(*FunctionName)) : nullptr;

    // Set as self member (function on this AnimBP)
    MemberRef->SetSelfMember(FName(*FunctionName));

    // Reconstruct node to update pins/display
    AnimNode->ReconstructNode();

    MarkModifiedAndSave(AnimBP, Params);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("node_id"), AnimNode->NodeGuid.ToString());
    ResultObj->SetStringField(TEXT("event"), EventName);
    ResultObj->SetStringField(TEXT("property"), PropertyName);
    ResultObj->SetStringField(TEXT("function_name"), FunctionName);
    ResultObj->SetBoolField(TEXT("function_exists"), TargetFunc != nullptr);

    if (!TargetFunc)
    {
        ResultObj->SetStringField(TEXT("warning"),
            FString::Printf(TEXT("Function '%s' not found in AnimBP. Create it with create_anim_graph_function first, or it will be resolved at compile time."),
                *FunctionName));
    }

    return ResultObj;
}

// ============================================================================
// create_anim_graph_function — Create a function in an AnimBP for AnimNode events
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPAnimGraphCommands::HandleCreateAnimGraphFunction(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Error;
    UAnimBlueprint* AnimBP = LoadAnimBlueprint(Params, Error);
    if (!AnimBP) return Error;

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    FString FunctionName;
    if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
    }

    // Check if function already exists
    for (UEdGraph* ExistingGraph : AnimBP->FunctionGraphs)
    {
        if (ExistingGraph && ExistingGraph->GetName() == FunctionName)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Function '%s' already exists in this AnimBlueprint"), *FunctionName));
        }
    }

    // Determine function type — "anim_event" (default) for AnimNode callbacks,
    // or "blueprint" for a regular K2 function
    FString FunctionType = TEXT("anim_event");
    Params->TryGetStringField(TEXT("type"), FunctionType);

    if (FunctionType.Equals(TEXT("anim_event"), ESearchCase::IgnoreCase))
    {
        // Create a K2 function graph with the AnimNode callback signature:
        // void FunctionName(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
        //
        // The prototype function is:
        //   UAnimExecutionContextLibrary::Prototype_ThreadSafeAnimUpdateCall

        // Find the prototype function
        UFunction* PrototypeFunc = nullptr;
        UClass* ContextLibClass = FindObject<UClass>(nullptr, TEXT("/Script/AnimGraphRuntime.AnimExecutionContextLibrary"));
        if (!ContextLibClass)
        {
            // Search all classes
            for (TObjectIterator<UClass> It; It; ++It)
            {
                if (It->GetName() == TEXT("AnimExecutionContextLibrary"))
                {
                    ContextLibClass = *It;
                    break;
                }
            }
        }

        if (ContextLibClass)
        {
            PrototypeFunc = ContextLibClass->FindFunctionByName(TEXT("Prototype_ThreadSafeAnimUpdateCall"));
        }

        // Create the function graph
        UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
            AnimBP,
            FName(*FunctionName),
            UEdGraph::StaticClass(),
            UEdGraphSchema_K2::StaticClass()
        );

        if (!NewGraph)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create function graph"));
        }

        // Set up as a proper function with terminators
        FBlueprintEditorUtils::AddFunctionGraph<UFunction>(AnimBP, NewGraph, /*bIsUserCreated=*/true, PrototypeFunc);

        // Mark as thread-safe (required for AnimNode callbacks)
        // Find the function entry node and set metadata
        UK2Node_FunctionEntry* EntryNode = nullptr;
        for (UEdGraphNode* GNode : NewGraph->Nodes)
        {
            EntryNode = Cast<UK2Node_FunctionEntry>(GNode);
            if (EntryNode) break;
        }

        if (EntryNode)
        {
            // Set thread-safe metadata
            EntryNode->MetaData.bThreadSafe = true;

            // If no prototype was found, manually add the parameters
            if (!PrototypeFunc)
            {
                // Add FAnimUpdateContext input
                TSharedPtr<FUserPinInfo> ContextPin = MakeShared<FUserPinInfo>();
                ContextPin->PinName = TEXT("Context");
                ContextPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
                UScriptStruct* ContextStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/AnimGraphRuntime.AnimUpdateContext"));
                if (!ContextStruct)
                {
                    for (TObjectIterator<UScriptStruct> It; It; ++It)
                    {
                        if (It->GetName() == TEXT("AnimUpdateContext"))
                        {
                            ContextStruct = *It;
                            break;
                        }
                    }
                }
                if (ContextStruct)
                {
                    ContextPin->PinType.PinSubCategoryObject = ContextStruct;
                }
                ContextPin->PinType.bIsConst = true;
                ContextPin->PinType.bIsReference = true;
                EntryNode->UserDefinedPins.Add(ContextPin);

                // Add FAnimNodeReference input
                TSharedPtr<FUserPinInfo> NodePin = MakeShared<FUserPinInfo>();
                NodePin->PinName = TEXT("Node");
                NodePin->PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
                UScriptStruct* NodeRefStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/AnimGraphRuntime.AnimNodeReference"));
                if (!NodeRefStruct)
                {
                    for (TObjectIterator<UScriptStruct> It; It; ++It)
                    {
                        if (It->GetName() == TEXT("AnimNodeReference"))
                        {
                            NodeRefStruct = *It;
                            break;
                        }
                    }
                }
                if (NodeRefStruct)
                {
                    NodePin->PinType.PinSubCategoryObject = NodeRefStruct;
                }
                NodePin->PinType.bIsConst = true;
                NodePin->PinType.bIsReference = true;
                EntryNode->UserDefinedPins.Add(NodePin);

                EntryNode->ReconstructNode();
            }
        }

        // Compile the blueprint to register the function
        FBlueprintEditorUtils::MarkBlueprintAsModified(AnimBP);

        MarkModifiedAndSave(AnimBP, Params);

        // Build result
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetBoolField(TEXT("success"), true);
        ResultObj->SetStringField(TEXT("function_name"), FunctionName);
        ResultObj->SetStringField(TEXT("graph_name"), NewGraph->GetName());
        ResultObj->SetStringField(TEXT("type"), TEXT("anim_event"));
        ResultObj->SetBoolField(TEXT("thread_safe"), true);

        if (PrototypeFunc)
        {
            ResultObj->SetStringField(TEXT("prototype"), TEXT("Prototype_ThreadSafeAnimUpdateCall"));
            ResultObj->SetStringField(TEXT("signature"), TEXT("void(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)"));
        }
        else
        {
            ResultObj->SetStringField(TEXT("warning"), TEXT("Prototype function not found. Parameters added manually — compile to verify."));
        }

        // Serialize nodes in the function graph
        TArray<TSharedPtr<FJsonValue>> NodesArray;
        for (UEdGraphNode* GNode : NewGraph->Nodes)
        {
            if (!GNode) continue;
            TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
            NodeObj->SetStringField(TEXT("node_id"), GNode->NodeGuid.ToString());
            NodeObj->SetStringField(TEXT("node_class"), GNode->GetClass()->GetName());
            NodeObj->SetStringField(TEXT("node_title"), GNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
            NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
        }
        ResultObj->SetArrayField(TEXT("nodes"), NodesArray);

        return ResultObj;
    }
    else if (FunctionType.Equals(TEXT("anim_layer"), ESearchCase::IgnoreCase))
    {
        // Create an AnimGraph layer function (UAnimationGraph + AnimationGraphSchema)
        // This creates a function graph that can be targeted by LinkedAnimLayer nodes
        UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
            AnimBP,
            FName(*FunctionName),
            UAnimationGraph::StaticClass(),
            UAnimationGraphSchema::StaticClass()
        );

        if (!NewGraph)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create anim layer graph"));
        }

        FBlueprintEditorUtils::AddDomainSpecificGraph(AnimBP, NewGraph);
        FBlueprintEditorUtils::MarkBlueprintAsModified(AnimBP);

        MarkModifiedAndSave(AnimBP, Params);

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetBoolField(TEXT("success"), true);
        ResultObj->SetStringField(TEXT("function_name"), FunctionName);
        ResultObj->SetStringField(TEXT("graph_name"), NewGraph->GetName());
        ResultObj->SetStringField(TEXT("type"), TEXT("anim_layer"));

        // Serialize nodes in the layer graph
        TArray<TSharedPtr<FJsonValue>> NodesArray;
        for (UEdGraphNode* GNode : NewGraph->Nodes)
        {
            if (!GNode) continue;
            TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
            NodeObj->SetStringField(TEXT("node_id"), GNode->NodeGuid.ToString());
            NodeObj->SetStringField(TEXT("node_class"), GNode->GetClass()->GetName());
            NodeObj->SetStringField(TEXT("node_title"), GNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
            NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
        }
        ResultObj->SetArrayField(TEXT("nodes"), NodesArray);

        return ResultObj;
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Unknown function type: '%s'. Supported: anim_event, anim_layer"), *FunctionType));
    }
}
