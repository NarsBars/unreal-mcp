#pragma once

#include "CoreMinimal.h"
#include "Json.h"

// Forward declarations
class UAnimBlueprint;
class UEdGraph;
class UEdGraphNode;

/**
 * Handler class for AnimGraph-related MCP commands.
 * Provides tools for reading, creating, connecting, and configuring
 * AnimGraph nodes in Animation Blueprints.
 */
class UNREALMCP_API FUnrealMCPAnimGraphCommands
{
public:
    FUnrealMCPAnimGraphCommands();

    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Command handlers
    TSharedPtr<FJsonObject> HandleReadAnimGraph(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddAnimGraphNode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleConnectAnimPins(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetAnimNodeProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteAnimGraphNode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDisconnectAnimPin(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleFindAnimGraphNodes(const TSharedPtr<FJsonObject>& Params);

    // Tier 2 — State Machine commands
    TSharedPtr<FJsonObject> HandleAddStateMachine(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddState(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddStateTransition(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetStateAnimation(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleReadStateMachine(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRenameState(const TSharedPtr<FJsonObject>& Params);

    // Tier 3 — Advanced commands
    TSharedPtr<FJsonObject> HandleAddAnimLayer(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlendNode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlendPosePin(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetAnimBlueprintParent(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCompileAnimBlueprint(const TSharedPtr<FJsonObject>& Params);

    // Tier 2.5 — State Machine utilities
    TSharedPtr<FJsonObject> HandleSetStateEntry(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleBindTransitionCondition(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleConnectK2Pins(const TSharedPtr<FJsonObject>& Params);

    // Tier 3.5 — Property Access binding & AnimNode function binding
    TSharedPtr<FJsonObject> HandleBindAnimPinToProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleBindAnimNodeFunction(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateAnimGraphFunction(const TSharedPtr<FJsonObject>& Params);

    // Tier 4 — PoseSearch commands
    TSharedPtr<FJsonObject> HandleConfigureMotionMatching(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleConfigureHistoryCollector(const TSharedPtr<FJsonObject>& Params);

    // Helpers

    /** Load an AnimBlueprint from a path or name. Returns nullptr + sets OutError on failure. */
    UAnimBlueprint* LoadAnimBlueprint(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutError);

    /** Resolve an AnimGraph from an AnimBlueprint by optional graph_name param. */
    UEdGraph* ResolveAnimGraph(UAnimBlueprint* AnimBP, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutError);

    /** Find a node by GUID string, or by class+index. */
    UEdGraphNode* FindNodeInGraph(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params,
                                   const FString& NodeIdField, TSharedPtr<FJsonObject>& OutError);

    /** Serialize a single AnimGraph node to JSON. */
    TSharedPtr<FJsonObject> SerializeAnimNode(UEdGraphNode* Node, int32 NodeIndex, const TMap<FGuid, int32>& NodeIndexMap);

    /** Mark blueprint modified and optionally save. */
    void MarkModifiedAndSave(UAnimBlueprint* AnimBP, const TSharedPtr<FJsonObject>& Params);

    /** Find a state machine's inner graph from a state machine node GUID in the root AnimGraph. */
    UEdGraph* FindStateMachineGraph(UAnimBlueprint* AnimBP, const FString& StateMachineGuid,
                                     TSharedPtr<FJsonObject>& OutError);

    /** Create an AnimGraph node in a target graph. Reusable core for add_anim_graph_node and set_state_animation. */
    TSharedPtr<FJsonObject> CreateAnimNodeInGraph(UEdGraph* Graph, UAnimBlueprint* AnimBP,
                                                   const FString& NodeClassName,
                                                   const TSharedPtr<FJsonObject>& Params);

    /** Spawn a K2 (Blueprint) node in a transition rule graph. Used by bind_transition_condition. */
    TSharedPtr<FJsonObject> SpawnK2NodeInTransitionGraph(UEdGraph* TransGraph, UAnimBlueprint* AnimBP,
                                                          const FString& NodeClass,
                                                          const TSharedPtr<FJsonObject>& Params);
};
