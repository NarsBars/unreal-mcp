#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Blueprint Node-related MCP commands
 */
class UNREALMCP_API FUnrealMCPBlueprintNodeCommands
{
public:
    FUnrealMCPBlueprintNodeCommands();

    // Handle blueprint node commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Specific blueprint node command handlers
    TSharedPtr<FJsonObject> HandleConnectBlueprintNodes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintGetSelfComponentReference(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintEvent(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintFunctionCall(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintVariable(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintInputActionNode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintSelfReference(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleFindBlueprintNodes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSpawnK2Node(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSmartConnectPins(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleReadBlueprintGraph(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateBlueprintFunction(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteBlueprintNode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDisconnectBlueprintPin(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetPinDefaultValue(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRemoveBlueprintVariable(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetBlueprintVariableDefaults(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRemoveBlueprintGraph(const TSharedPtr<FJsonObject>& Params);

    // Graph pseudocode generation
    struct FBPGraphContext
    {
        struct FExecTarget { FString PinName; int32 TargetNodeIndex; };
        struct FDataInput { FString PinName; int32 SourceNodeIndex; FString SourcePinName; FString DefaultValue; bool bConnected; };

        TArray<class UEdGraphNode*> Nodes;
        TMap<int32, TArray<FExecTarget>> ExecOutgoing;   // node_index → exec outputs
        TMap<int32, TArray<FDataInput>> DataInputs;      // node_index → data inputs
        TSet<int32> HasIncomingExec;                      // nodes with incoming exec connections
        TSet<int32> Visited;                              // cycle protection
    };

    FString BuildBlueprintPseudocode(class UBlueprint* Blueprint) const;
    void BuildGraphAdjacency(const TArray<class UEdGraphNode*>& Nodes, FBPGraphContext& Ctx) const;
    FString FormatExecChain(int32 NodeIndex, int32 Depth, FBPGraphContext& Ctx) const;
    static FString FormatNodeLine(int32 NodeIndex, class UEdGraphNode* Node, FBPGraphContext& Ctx);
    static FString ShortenNodeTitle(const FString& Title);
};