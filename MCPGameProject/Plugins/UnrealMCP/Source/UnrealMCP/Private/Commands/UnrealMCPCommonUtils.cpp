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
#include "Engine/Selection.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabase.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "Editor.h"
#include "Animation/AnimBlueprint.h"
#include "AnimationGraph.h"
#include "AnimGraphNode_Base.h"
#include "AnimationStateMachineGraph.h"

// JSON Utilities
TSharedPtr<FJsonObject> FUnrealMCPCommonUtils::CreateErrorResponse(const FString& Message, const FString& Context)
{
    TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
    ResponseObject->SetBoolField(TEXT("success"), false);
    ResponseObject->SetStringField(TEXT("error"), Message);
    if (!Context.IsEmpty())
    {
        ResponseObject->SetStringField(TEXT("context"), Context);
    }
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

TSharedPtr<FJsonObject> FUnrealMCPCommonUtils::EnsurePIEStopped()
{
	if (!GEditor || !GEditor->PlayWorld)
	{
		return nullptr; // Not playing, safe to proceed
	}

	UE_LOG(LogTemp, Log, TEXT("UnrealMCP: Auto-stopping PIE before asset operation"));
	GEditor->RequestEndPlayMap();
	return nullptr;
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

// Blueprint Utilities
UBlueprint* FUnrealMCPCommonUtils::FindBlueprint(const FString& BlueprintName)
{
    return FindBlueprintByName(BlueprintName);
}

UBlueprint* FUnrealMCPCommonUtils::FindBlueprintByName(const FString& BlueprintName)
{
    // 1. If caller passed a full content path, try loading directly
    if (BlueprintName.StartsWith(TEXT("/Game/")) || BlueprintName.StartsWith(TEXT("/Script/")))
    {
        UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintName);
        if (BP) return BP;
    }

    // 2. Asset Registry search by name (finds blueprints anywhere in /Game/)
    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

    FARFilter Filter;
    Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;
    Filter.PackagePaths.Add(FName(TEXT("/Game")));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> Results;
    AssetRegistry.GetAssets(Filter, Results);

    for (const FAssetData& Asset : Results)
    {
        if (Asset.AssetName.ToString() == BlueprintName)
        {
            return Cast<UBlueprint>(Asset.GetAsset());
        }
    }

    // 3. Legacy fallback — only for bare names, not full paths
    if (!BlueprintName.StartsWith(TEXT("/")))
    {
        FString LegacyPath = TEXT("/Game/Blueprints/") + BlueprintName;
        return LoadObject<UBlueprint>(nullptr, *LegacyPath);
    }

    return nullptr;
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

UEdGraph* FUnrealMCPCommonUtils::FindGraphByName(UBlueprint* Blueprint, const FString& GraphName)
{
    if (!Blueprint)
    {
        return nullptr;
    }

    // Empty or "EventGraph" → delegate to existing behavior
    if (GraphName.IsEmpty() || GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
    {
        return FindOrCreateEventGraph(Blueprint);
    }

    // Search function graphs
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (Graph && Graph->GetName() == GraphName)
        {
            return Graph;
        }
    }

    // Search ubergraph pages (for non-EventGraph uber graphs)
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph && Graph->GetName() == GraphName)
        {
            return Graph;
        }
    }

    // Search macro graphs
    for (UEdGraph* Graph : Blueprint->MacroGraphs)
    {
        if (Graph && Graph->GetName() == GraphName)
        {
            return Graph;
        }
    }

    // Search AnimGraph sub-graphs (state machines, state inner graphs) for AnimBlueprints
    UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Blueprint);
    if (AnimBP)
    {
        // Recursively search all sub-graphs (state machines have nested graphs)
        TArray<UEdGraph*> AllGraphs;
        for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        {
            if (Graph)
            {
                AllGraphs.Add(Graph);
                Graph->GetAllChildrenGraphs(AllGraphs);
            }
        }
        for (UEdGraph* Graph : AllGraphs)
        {
            if (Graph && Graph->GetName() == GraphName)
            {
                return Graph;
            }
        }
    }

    return nullptr;
}

UEdGraph* FUnrealMCPCommonUtils::FindAnimGraph(UBlueprint* Blueprint, const FString& GraphName)
{
    UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Blueprint);
    if (!AnimBP)
    {
        return nullptr;
    }

    // If a specific graph name is provided, search for it
    if (!GraphName.IsEmpty() && !GraphName.Equals(TEXT("AnimGraph"), ESearchCase::IgnoreCase))
    {
        // Search all sub-graphs including state machine inner graphs
        TArray<UEdGraph*> AllGraphs;
        for (UEdGraph* Graph : AnimBP->FunctionGraphs)
        {
            if (Graph)
            {
                AllGraphs.Add(Graph);
                Graph->GetAllChildrenGraphs(AllGraphs);
            }
        }
        for (UEdGraph* Graph : AllGraphs)
        {
            if (Graph && Graph->GetName() == GraphName)
            {
                return Graph;
            }
        }
        return nullptr;
    }

    // Default: find the root AnimGraph (uses UAnimationGraphSchema)
    for (UEdGraph* Graph : AnimBP->FunctionGraphs)
    {
        if (Graph && Graph->GetName() == TEXT("AnimGraph"))
        {
            return Graph;
        }
    }

    return nullptr;
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

    // Find the function — walk up the class hierarchy (handles AS-defined
    // BlueprintEvents on parent classes that aren't on the child yet)
    UClass* BlueprintClass = Blueprint->GeneratedClass;
    UFunction* EventFunction = nullptr;
    UClass* OwnerClass = nullptr;

    for (UClass* SearchClass = BlueprintClass; SearchClass; SearchClass = SearchClass->GetSuperClass())
    {
        EventFunction = SearchClass->FindFunctionByName(FName(*EventName));
        if (EventFunction)
        {
            OwnerClass = SearchClass;
            break;
        }
    }

    // Also check parent Blueprint's skeleton class (for AS classes that register
    // functions on the skeleton rather than the generated class)
    if (!EventFunction && Blueprint->ParentClass)
    {
        for (UClass* SearchClass = Blueprint->ParentClass; SearchClass; SearchClass = SearchClass->GetSuperClass())
        {
            EventFunction = SearchClass->FindFunctionByName(FName(*EventName));
            if (EventFunction)
            {
                OwnerClass = SearchClass;
                break;
            }
        }
    }

    if (EventFunction && OwnerClass)
    {
        EventNode = NewObject<UK2Node_Event>(Graph);
        EventNode->EventReference.SetExternalMember(FName(*EventName), OwnerClass);
        EventNode->bOverrideFunction = true;
        EventNode->NodePosX = Position.X;
        EventNode->NodePosY = Position.Y;
        Graph->AddNode(EventNode, true);
        EventNode->CreateNewGuid();
        EventNode->PostPlacedNewNode();
        EventNode->AllocateDefaultPins();
        UE_LOG(LogTemp, Display, TEXT("Created event node '%s' bound to %s (ID: %s)"),
            *EventName, *OwnerClass->GetName(), *EventNode->NodeGuid.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to find function '%s' on class hierarchy for %s"),
            *EventName, *Blueprint->GetName());
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

TSharedPtr<FJsonObject> FUnrealMCPCommonUtils::ActorToJsonObject(AActor* Actor)
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

// Helper: parse "PropName[2]" into ("PropName", 2). Returns true if array index found.
static bool ParseArrayIndex(const FString& Segment, FString& OutPropName, int32& OutIndex)
{
    int32 OpenBracket;
    if (!Segment.FindChar('[', OpenBracket))
    {
        OutPropName = Segment;
        OutIndex = -1;
        return false;
    }

    int32 CloseBracket;
    if (!Segment.FindChar(']', CloseBracket) || CloseBracket <= OpenBracket + 1)
    {
        OutPropName = Segment;
        OutIndex = -1;
        return false;
    }

    FString IndexStr = Segment.Mid(OpenBracket + 1, CloseBracket - OpenBracket - 1);
    if (!IndexStr.IsNumeric())
    {
        OutPropName = Segment;
        OutIndex = -1;
        return false;
    }

    OutPropName = Segment.Left(OpenBracket);
    OutIndex = FCString::Atoi(*IndexStr);
    return true;
}

// Helper: resolve a single path segment (struct or array[N]) and advance CurrentContainer/CurrentStruct.
// Returns false and sets OutErrorMessage on failure.
static bool ResolvePathSegment(const FString& Segment, const FString& FullPath,
    UStruct*& CurrentStruct, void*& CurrentContainer, FString& OutErrorMessage)
{
    FString PropName;
    int32 ArrayIndex;
    bool bHasArrayIndex = ParseArrayIndex(Segment, PropName, ArrayIndex);

    FProperty* SegmentProp = CurrentStruct->FindPropertyByName(*PropName);
    if (!SegmentProp)
    {
        OutErrorMessage = FString::Printf(TEXT("Property not found at segment '%s' in path: %s"), *PropName, *FullPath);
        return false;
    }

    if (bHasArrayIndex)
    {
        // Array element access
        FArrayProperty* ArrayProp = CastField<FArrayProperty>(SegmentProp);
        if (!ArrayProp)
        {
            OutErrorMessage = FString::Printf(TEXT("'%s' is not an array property in path: %s"), *PropName, *FullPath);
            return false;
        }

        void* ArrayAddr = ArrayProp->ContainerPtrToValuePtr<void>(CurrentContainer);
        FScriptArrayHelper ArrayHelper(ArrayProp, ArrayAddr);

        if (ArrayIndex < 0 || ArrayIndex >= ArrayHelper.Num())
        {
            OutErrorMessage = FString::Printf(TEXT("Index %d out of bounds for '%s' (length: %d) in path: %s"),
                ArrayIndex, *PropName, ArrayHelper.Num(), *FullPath);
            return false;
        }

        CurrentContainer = ArrayHelper.GetRawPtr(ArrayIndex);

        // Determine inner type for continued traversal
        FStructProperty* InnerStructProp = CastField<FStructProperty>(ArrayProp->Inner);
        if (InnerStructProp)
        {
            CurrentStruct = InnerStructProp->Struct;
        }
        else
        {
            // Non-struct inner — can't walk further. Caller must handle this as a leaf.
            CurrentStruct = nullptr;
        }
    }
    else
    {
        // Struct traversal (original behavior)
        FStructProperty* StructProp = CastField<FStructProperty>(SegmentProp);
        if (!StructProp)
        {
            OutErrorMessage = FString::Printf(TEXT("Segment '%s' is not a struct in path: %s"), *PropName, *FullPath);
            return false;
        }

        CurrentContainer = StructProp->ContainerPtrToValuePtr<void>(CurrentContainer);
        CurrentStruct = StructProp->Struct;
    }

    return true;
}

bool FUnrealMCPCommonUtils::SetObjectProperty(UObject* Object, const FString& PropertyName,
                                     const TSharedPtr<FJsonValue>& Value, FString& OutErrorMessage)
{
    if (!Object)
    {
        OutErrorMessage = TEXT("Invalid object");
        return false;
    }

    // Dot-path and/or array-index traversal: "Node.SpineBones", "Attacks[0].Name", "A[0].B[1].C"
    if (PropertyName.Contains(TEXT(".")) || PropertyName.Contains(TEXT("[")))
    {
        TArray<FString> PathSegments;
        PropertyName.ParseIntoArray(PathSegments, TEXT("."));

        if (PathSegments.Num() < 1)
        {
            OutErrorMessage = FString::Printf(TEXT("Invalid path: %s"), *PropertyName);
            return false;
        }

        // Single segment with array index (no dot): e.g. "Attacks[0]"
        if (PathSegments.Num() == 1)
        {
            FString PropName;
            int32 ArrayIndex;
            if (ParseArrayIndex(PathSegments[0], PropName, ArrayIndex))
            {
                FProperty* Prop = Object->GetClass()->FindPropertyByName(*PropName);
                if (!Prop)
                {
                    OutErrorMessage = FString::Printf(TEXT("Property not found: %s"), *PropName);
                    return false;
                }

                FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop);
                if (!ArrayProp)
                {
                    OutErrorMessage = FString::Printf(TEXT("'%s' is not an array property"), *PropName);
                    return false;
                }

                void* ArrayAddr = ArrayProp->ContainerPtrToValuePtr<void>(Object);
                FScriptArrayHelper ArrayHelper(ArrayProp, ArrayAddr);

                if (ArrayIndex < 0 || ArrayIndex >= ArrayHelper.Num())
                {
                    OutErrorMessage = FString::Printf(TEXT("Index %d out of bounds for '%s' (length: %d)"),
                        ArrayIndex, *PropName, ArrayHelper.Num());
                    return false;
                }

                void* ElemAddr = ArrayHelper.GetRawPtr(ArrayIndex);
                return SetPropertyValue(ArrayProp->Inner, ElemAddr, Value, OutErrorMessage);
            }
            // Fall through to simple property lookup below
        }
        else
        {
            // Multi-segment path: walk intermediate segments
            UStruct* CurrentStruct = Object->GetClass();
            void* CurrentContainer = Object;

            for (int32 i = 0; i < PathSegments.Num() - 1; ++i)
            {
                if (!ResolvePathSegment(PathSegments[i], PropertyName, CurrentStruct, CurrentContainer, OutErrorMessage))
                {
                    return false;
                }

                // If ResolvePathSegment set CurrentStruct to nullptr, the inner type is non-struct
                if (!CurrentStruct && i < PathSegments.Num() - 2)
                {
                    OutErrorMessage = FString::Printf(TEXT("Cannot traverse further after non-struct array element '%s' in path: %s"),
                        *PathSegments[i], *PropertyName);
                    return false;
                }
            }

            // Resolve the leaf segment
            const FString& LeafSegment = PathSegments.Last();
            FString LeafName;
            int32 LeafArrayIndex;
            bool bLeafHasArrayIndex = ParseArrayIndex(LeafSegment, LeafName, LeafArrayIndex);

            // If CurrentStruct is nullptr, the previous segment was a non-struct array element —
            // leaf property lookup is invalid (we're already at the value)
            if (!CurrentStruct)
            {
                OutErrorMessage = FString::Printf(TEXT("Cannot access property '%s' on non-struct array element in path: %s"),
                    *LeafName, *PropertyName);
                return false;
            }

            FProperty* LeafProp = CurrentStruct->FindPropertyByName(*LeafName);
            if (!LeafProp)
            {
                OutErrorMessage = FString::Printf(TEXT("Leaf property '%s' not found in path: %s"), *LeafName, *PropertyName);
                return false;
            }

            if (bLeafHasArrayIndex)
            {
                // Leaf is an array element: "Config.Attacks[0]"
                FArrayProperty* LeafArrayProp = CastField<FArrayProperty>(LeafProp);
                if (!LeafArrayProp)
                {
                    OutErrorMessage = FString::Printf(TEXT("Leaf '%s' is not an array property in path: %s"), *LeafName, *PropertyName);
                    return false;
                }

                void* ArrayAddr = LeafArrayProp->ContainerPtrToValuePtr<void>(CurrentContainer);
                FScriptArrayHelper ArrayHelper(LeafArrayProp, ArrayAddr);

                if (LeafArrayIndex < 0 || LeafArrayIndex >= ArrayHelper.Num())
                {
                    OutErrorMessage = FString::Printf(TEXT("Index %d out of bounds for leaf '%s' (length: %d) in path: %s"),
                        LeafArrayIndex, *LeafName, ArrayHelper.Num(), *PropertyName);
                    return false;
                }

                void* ElemAddr = ArrayHelper.GetRawPtr(LeafArrayIndex);
                return SetPropertyValue(LeafArrayProp->Inner, ElemAddr, Value, OutErrorMessage);
            }
            else
            {
                void* LeafAddr = LeafProp->ContainerPtrToValuePtr<void>(CurrentContainer);
                return SetPropertyValue(LeafProp, LeafAddr, Value, OutErrorMessage);
            }
        }
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
    else if (Property->IsA<FNameProperty>())
    {
        ((FNameProperty*)Property)->SetPropertyValue(PropertyAddr, FName(*Value->AsString()));
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
    else if (Property->IsA<FTextProperty>())
    {
        FTextProperty* TextProp = CastField<FTextProperty>(Property);
        TextProp->SetPropertyValue(PropertyAddr, FText::FromString(Value->AsString()));
        return true;
    }
    else if (Property->IsA<FStructProperty>())
    {
        FStructProperty* StructProp = CastField<FStructProperty>(Property);

        // FLinearColor
        if (StructProp->Struct == TBaseStructure<FLinearColor>::Get())
        {
            FLinearColor Color;
            const TArray<TSharedPtr<FJsonValue>>* ArrValue;
            const TSharedPtr<FJsonObject>* ObjValue;
            if (Value->TryGetArray(ArrValue) && ArrValue->Num() >= 3)
            {
                Color.R = (*ArrValue)[0]->AsNumber();
                Color.G = (*ArrValue)[1]->AsNumber();
                Color.B = (*ArrValue)[2]->AsNumber();
                Color.A = ArrValue->Num() >= 4 ? (*ArrValue)[3]->AsNumber() : 1.0f;
            }
            else if (Value->TryGetObject(ObjValue))
            {
                Color.R = (*ObjValue)->GetNumberField(TEXT("R"));
                Color.G = (*ObjValue)->GetNumberField(TEXT("G"));
                Color.B = (*ObjValue)->GetNumberField(TEXT("B"));
                Color.A = (*ObjValue)->HasField(TEXT("A")) ? (*ObjValue)->GetNumberField(TEXT("A")) : 1.0f;
            }
            else
            {
                OutErrorMessage = FString::Printf(TEXT("FLinearColor property %s requires [R,G,B,A] array or {\"R\",\"G\",\"B\",\"A\"} object"), *PropertyName);
                return false;
            }
            *static_cast<FLinearColor*>(PropertyAddr) = Color;
            return true;
        }
        // FVector
        else if (StructProp->Struct == TBaseStructure<FVector>::Get())
        {
            const TSharedPtr<FJsonObject>* ObjValue;
            if (Value->TryGetObject(ObjValue))
            {
                FVector Vec;
                Vec.X = (*ObjValue)->GetNumberField(TEXT("X"));
                Vec.Y = (*ObjValue)->GetNumberField(TEXT("Y"));
                Vec.Z = (*ObjValue)->GetNumberField(TEXT("Z"));
                *static_cast<FVector*>(PropertyAddr) = Vec;
                return true;
            }
            const TArray<TSharedPtr<FJsonValue>>* ArrValue;
            if (Value->TryGetArray(ArrValue) && ArrValue->Num() >= 3)
            {
                FVector Vec;
                Vec.X = (*ArrValue)[0]->AsNumber();
                Vec.Y = (*ArrValue)[1]->AsNumber();
                Vec.Z = (*ArrValue)[2]->AsNumber();
                *static_cast<FVector*>(PropertyAddr) = Vec;
                return true;
            }
            OutErrorMessage = FString::Printf(TEXT("FVector property %s requires {\"X\",\"Y\",\"Z\"} object or [X,Y,Z] array"), *PropertyName);
            return false;
        }
        // FRotator
        else if (StructProp->Struct == TBaseStructure<FRotator>::Get())
        {
            const TSharedPtr<FJsonObject>* ObjValue;
            if (Value->TryGetObject(ObjValue))
            {
                FRotator Rot;
                Rot.Pitch = (*ObjValue)->GetNumberField(TEXT("Pitch"));
                Rot.Yaw = (*ObjValue)->GetNumberField(TEXT("Yaw"));
                Rot.Roll = (*ObjValue)->GetNumberField(TEXT("Roll"));
                *static_cast<FRotator*>(PropertyAddr) = Rot;
                return true;
            }
            const TArray<TSharedPtr<FJsonValue>>* ArrValue;
            if (Value->TryGetArray(ArrValue) && ArrValue->Num() >= 3)
            {
                FRotator Rot;
                Rot.Pitch = (*ArrValue)[0]->AsNumber();
                Rot.Yaw = (*ArrValue)[1]->AsNumber();
                Rot.Roll = (*ArrValue)[2]->AsNumber();
                *static_cast<FRotator*>(PropertyAddr) = Rot;
                return true;
            }
            OutErrorMessage = FString::Printf(TEXT("FRotator property %s requires {\"Pitch\",\"Yaw\",\"Roll\"} object or [P,Y,R] array"), *PropertyName);
            return false;
        }
        // FGameplayTag — accepts string like "Perk.Bow.Hunter.SteadyAim"
        // Use ImportText for robust handling across native and AngelScript contexts
        else if (StructProp->Struct == FGameplayTag::StaticStruct()
                 || StructProp->Struct->GetFName() == FName("GameplayTag"))
        {
            FString TagString = Value->AsString();
            if (TagString.IsEmpty())
            {
                *static_cast<FGameplayTag*>(PropertyAddr) = FGameplayTag();
                return true;
            }
            // RequestGameplayTag — tag must be registered in DefaultGameplayTags.ini or via AddNativeGameplayTag
            FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagString), false);
            if (Tag.IsValid())
            {
                *static_cast<FGameplayTag*>(PropertyAddr) = Tag;
                return true;
            }
            // Tag not registered — report error
            OutErrorMessage = FString::Printf(TEXT("Gameplay tag '%s' is not registered. Add it to DefaultGameplayTags.ini and restart the editor."), *TagString);
            return false;
        }
        // FGameplayTagContainer — accepts array of tag strings
        else if (StructProp->Struct == FGameplayTagContainer::StaticStruct()
                 || StructProp->Struct->GetFName() == FName("GameplayTagContainer"))
        {
            const TArray<TSharedPtr<FJsonValue>>* ArrValue;
            if (Value->TryGetArray(ArrValue))
            {
                FGameplayTagContainer Container;
                for (const TSharedPtr<FJsonValue>& Elem : *ArrValue)
                {
                    FString TagString = Elem->AsString();
                    FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagString), false);
                    if (Tag.IsValid())
                    {
                        Container.AddTag(Tag);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Gameplay tag '%s' not registered, skipping"), *TagString);
                    }
                }
                *static_cast<FGameplayTagContainer*>(PropertyAddr) = Container;
            }
            return true;
        }
        // Generic struct — recurse into fields from a JSON object
        else
        {
            const TSharedPtr<FJsonObject>* ObjValue;
            if (Value->TryGetObject(ObjValue))
            {
                UScriptStruct* Struct = StructProp->Struct;
                for (const auto& Pair : (*ObjValue)->Values)
                {
                    FProperty* InnerProp = Struct->FindPropertyByName(*Pair.Key);
                    if (InnerProp)
                    {
                        void* InnerAddr = InnerProp->ContainerPtrToValuePtr<void>(PropertyAddr);
                        FString InnerError;
                        // Use the inner setter directly
                        if (!SetPropertyValue(InnerProp, InnerAddr, Pair.Value, InnerError))
                        {
                            UE_LOG(LogTemp, Warning, TEXT("Failed to set struct field %s.%s: %s"),
                                *PropertyName, *Pair.Key, *InnerError);
                        }
                    }
                }
                return true;
            }
            OutErrorMessage = FString::Printf(TEXT("Struct property %s requires a JSON object"), *PropertyName);
            return false;
        }
    }
    else if (Property->IsA<FArrayProperty>())
    {
        FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property);
        const TArray<TSharedPtr<FJsonValue>>* ArrValue;
        if (!Value->TryGetArray(ArrValue))
        {
            OutErrorMessage = FString::Printf(TEXT("Array property %s requires a JSON array"), *PropertyName);
            return false;
        }

        FScriptArrayHelper ArrayHelper(ArrayProp, PropertyAddr);
        ArrayHelper.EmptyValues();

        FProperty* InnerProp = ArrayProp->Inner;
        for (int32 i = 0; i < ArrValue->Num(); ++i)
        {
            int32 NewIndex = ArrayHelper.AddValue();
            void* ElemAddr = ArrayHelper.GetRawPtr(NewIndex);
            FString InnerError;
            if (!SetPropertyValue(InnerProp, ElemAddr, (*ArrValue)[i], InnerError))
            {
                UE_LOG(LogTemp, Warning, TEXT("Failed to set array element %d of %s: %s"),
                    i, *PropertyName, *InnerError);
            }
        }
        return true;
    }
    else if (Property->IsA<FClassProperty>())
    {
        // TSubclassOf<T> — accepts class path string like "/Script/Module.ClassName"
        FClassProperty* ClassProp = CastField<FClassProperty>(Property);
        FString ClassPath = Value->AsString();
        if (ClassPath.IsEmpty())
        {
            ClassProp->SetPropertyValue(PropertyAddr, nullptr);
            return true;
        }
        UClass* LoadedClass = LoadObject<UClass>(nullptr, *ClassPath);
        if (!LoadedClass)
        {
            // Try finding by short name
            LoadedClass = FindFirstObject<UClass>(*ClassPath, EFindFirstObjectOptions::NativeFirst);
        }
        if (LoadedClass)
        {
            ClassProp->SetPropertyValue(PropertyAddr, LoadedClass);
            return true;
        }
        OutErrorMessage = FString::Printf(TEXT("Could not load class: %s"), *ClassPath);
        return false;
    }
    else if (Property->IsA<FSoftClassProperty>())
    {
        FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Property);
        FString ClassPath = Value->AsString();
        FSoftObjectPtr& SoftPtr = *static_cast<FSoftObjectPtr*>(PropertyAddr);
        SoftPtr = FSoftObjectPath(ClassPath);
        return true;
    }
    else if (Property->IsA<FObjectProperty>())
    {
        // UObject* reference — accepts asset path like "/Game/Path/Asset.Asset"
        FObjectProperty* ObjProp = CastField<FObjectProperty>(Property);
        FString AssetPath = Value->AsString();
        if (AssetPath.IsEmpty())
        {
            ObjProp->SetPropertyValue(PropertyAddr, nullptr);
            return true;
        }
        UObject* LoadedObj = LoadObject<UObject>(nullptr, *AssetPath);
        if (LoadedObj)
        {
            ObjProp->SetPropertyValue(PropertyAddr, LoadedObj);
            return true;
        }
        OutErrorMessage = FString::Printf(TEXT("Could not load object: %s"), *AssetPath);
        return false;
    }

    OutErrorMessage = FString::Printf(TEXT("Unsupported property type: %s for property %s"),
                                    *Property->GetClass()->GetName(), *PropertyName);
    return false;
}

bool FUnrealMCPCommonUtils::SetPropertyValue(FProperty* Property, void* ValueAddr,
                                              const TSharedPtr<FJsonValue>& Value, FString& OutErrorMessage)
{
    if (!Property || !ValueAddr)
    {
        OutErrorMessage = TEXT("Invalid property or address");
        return false;
    }

    // Bool
    if (Property->IsA<FBoolProperty>())
    {
        ((FBoolProperty*)Property)->SetPropertyValue(ValueAddr, Value->AsBool());
        return true;
    }
    // Int
    else if (Property->IsA<FIntProperty>())
    {
        ((FIntProperty*)Property)->SetPropertyValue(ValueAddr, static_cast<int32>(Value->AsNumber()));
        return true;
    }
    // Float
    else if (Property->IsA<FFloatProperty>())
    {
        ((FFloatProperty*)Property)->SetPropertyValue(ValueAddr, Value->AsNumber());
        return true;
    }
    // Double
    else if (Property->IsA<FDoubleProperty>())
    {
        ((FDoubleProperty*)Property)->SetPropertyValue(ValueAddr, Value->AsNumber());
        return true;
    }
    // String
    else if (Property->IsA<FStrProperty>())
    {
        ((FStrProperty*)Property)->SetPropertyValue(ValueAddr, Value->AsString());
        return true;
    }
    // Name
    else if (Property->IsA<FNameProperty>())
    {
        ((FNameProperty*)Property)->SetPropertyValue(ValueAddr, FName(*Value->AsString()));
        return true;
    }
    // Text
    else if (Property->IsA<FTextProperty>())
    {
        ((FTextProperty*)Property)->SetPropertyValue(ValueAddr, FText::FromString(Value->AsString()));
        return true;
    }
    // Byte/Enum (simplified for inner array elements)
    else if (Property->IsA<FByteProperty>())
    {
        ((FByteProperty*)Property)->SetPropertyValue(ValueAddr, static_cast<uint8>(Value->AsNumber()));
        return true;
    }
    // Enum property
    else if (Property->IsA<FEnumProperty>())
    {
        FEnumProperty* EnumProp = CastField<FEnumProperty>(Property);
        UEnum* EnumDef = EnumProp ? EnumProp->GetEnum() : nullptr;
        FNumericProperty* UnderlyingNumericProp = EnumProp ? EnumProp->GetUnderlyingProperty() : nullptr;
        if (EnumDef && UnderlyingNumericProp)
        {
            if (Value->Type == EJson::Number)
            {
                UnderlyingNumericProp->SetIntPropertyValue(ValueAddr, static_cast<int64>(Value->AsNumber()));
                return true;
            }
            else if (Value->Type == EJson::String)
            {
                FString EnumValueName = Value->AsString();
                if (EnumValueName.Contains(TEXT("::")))
                {
                    EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
                }
                int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
                if (EnumValue != INDEX_NONE)
                {
                    UnderlyingNumericProp->SetIntPropertyValue(ValueAddr, EnumValue);
                    return true;
                }
            }
        }
        OutErrorMessage = TEXT("Failed to set enum value");
        return false;
    }
    // Struct
    else if (Property->IsA<FStructProperty>())
    {
        FStructProperty* StructProp = CastField<FStructProperty>(Property);

        // FGameplayTag
        if (StructProp->Struct == FGameplayTag::StaticStruct()
            || StructProp->Struct->GetFName() == FName("GameplayTag"))
        {
            FString TagString = Value->AsString();
            if (TagString.IsEmpty())
            {
                FMemory::Memzero(ValueAddr, StructProp->Struct->GetStructureSize());
                return true;
            }
            FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagString), false);
            *static_cast<FGameplayTag*>(ValueAddr) = Tag;
            return true;
        }
        // FGameplayTagContainer
        else if (StructProp->Struct == FGameplayTagContainer::StaticStruct()
                 || StructProp->Struct->GetFName() == FName("GameplayTagContainer"))
        {
            const TArray<TSharedPtr<FJsonValue>>* ArrValue;
            if (Value->TryGetArray(ArrValue))
            {
                FGameplayTagContainer Container;
                for (const TSharedPtr<FJsonValue>& Elem : *ArrValue)
                {
                    FString TagString = Elem->AsString();
                    FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagString), false);
                    if (Tag.IsValid())
                    {
                        Container.AddTag(Tag);
                    }
                }
                *static_cast<FGameplayTagContainer*>(ValueAddr) = Container;
            }
            return true;
        }
        // FLinearColor
        else if (StructProp->Struct == TBaseStructure<FLinearColor>::Get())
        {
            FLinearColor Color;
            const TArray<TSharedPtr<FJsonValue>>* ArrValue;
            const TSharedPtr<FJsonObject>* ObjValue;
            if (Value->TryGetArray(ArrValue) && ArrValue->Num() >= 3)
            {
                Color.R = (*ArrValue)[0]->AsNumber();
                Color.G = (*ArrValue)[1]->AsNumber();
                Color.B = (*ArrValue)[2]->AsNumber();
                Color.A = ArrValue->Num() >= 4 ? (*ArrValue)[3]->AsNumber() : 1.0f;
            }
            else if (Value->TryGetObject(ObjValue))
            {
                Color.R = (*ObjValue)->GetNumberField(TEXT("R"));
                Color.G = (*ObjValue)->GetNumberField(TEXT("G"));
                Color.B = (*ObjValue)->GetNumberField(TEXT("B"));
                Color.A = (*ObjValue)->HasField(TEXT("A")) ? (*ObjValue)->GetNumberField(TEXT("A")) : 1.0f;
            }
            else
            {
                OutErrorMessage = TEXT("FLinearColor requires [R,G,B,A] array or {\"R\",\"G\",\"B\",\"A\"} object");
                return false;
            }
            *static_cast<FLinearColor*>(ValueAddr) = Color;
            return true;
        }
        // FVector
        else if (StructProp->Struct == TBaseStructure<FVector>::Get())
        {
            const TSharedPtr<FJsonObject>* ObjValue;
            if (Value->TryGetObject(ObjValue))
            {
                FVector Vec;
                Vec.X = (*ObjValue)->GetNumberField(TEXT("X"));
                Vec.Y = (*ObjValue)->GetNumberField(TEXT("Y"));
                Vec.Z = (*ObjValue)->GetNumberField(TEXT("Z"));
                *static_cast<FVector*>(ValueAddr) = Vec;
                return true;
            }
            const TArray<TSharedPtr<FJsonValue>>* ArrValue;
            if (Value->TryGetArray(ArrValue) && ArrValue->Num() >= 3)
            {
                FVector Vec;
                Vec.X = (*ArrValue)[0]->AsNumber();
                Vec.Y = (*ArrValue)[1]->AsNumber();
                Vec.Z = (*ArrValue)[2]->AsNumber();
                *static_cast<FVector*>(ValueAddr) = Vec;
                return true;
            }
            OutErrorMessage = TEXT("FVector requires {\"X\",\"Y\",\"Z\"} object or [X,Y,Z] array");
            return false;
        }
        // FRotator
        else if (StructProp->Struct == TBaseStructure<FRotator>::Get())
        {
            const TSharedPtr<FJsonObject>* ObjValue;
            if (Value->TryGetObject(ObjValue))
            {
                FRotator Rot;
                Rot.Pitch = (*ObjValue)->GetNumberField(TEXT("Pitch"));
                Rot.Yaw = (*ObjValue)->GetNumberField(TEXT("Yaw"));
                Rot.Roll = (*ObjValue)->GetNumberField(TEXT("Roll"));
                *static_cast<FRotator*>(ValueAddr) = Rot;
                return true;
            }
            const TArray<TSharedPtr<FJsonValue>>* ArrValue;
            if (Value->TryGetArray(ArrValue) && ArrValue->Num() >= 3)
            {
                FRotator Rot;
                Rot.Pitch = (*ArrValue)[0]->AsNumber();
                Rot.Yaw = (*ArrValue)[1]->AsNumber();
                Rot.Roll = (*ArrValue)[2]->AsNumber();
                *static_cast<FRotator*>(ValueAddr) = Rot;
                return true;
            }
            OutErrorMessage = TEXT("FRotator requires {\"Pitch\",\"Yaw\",\"Roll\"} object or [P,Y,R] array");
            return false;
        }
        // Generic struct recursion
        else
        {
            const TSharedPtr<FJsonObject>* ObjValue;
            if (Value->TryGetObject(ObjValue))
            {
                UScriptStruct* Struct = StructProp->Struct;
                for (const auto& Pair : (*ObjValue)->Values)
                {
                    FProperty* InnerProp = Struct->FindPropertyByName(*Pair.Key);
                    if (InnerProp)
                    {
                        void* InnerAddr = InnerProp->ContainerPtrToValuePtr<void>(ValueAddr);
                        FString InnerError;
                        if (!SetPropertyValue(InnerProp, InnerAddr, Pair.Value, InnerError))
                        {
                            UE_LOG(LogTemp, Warning, TEXT("Failed to set struct field %s: %s"),
                                *Pair.Key, *InnerError);
                        }
                    }
                }
                return true;
            }
        }
        OutErrorMessage = FString::Printf(TEXT("Unsupported struct type or invalid value for: %s"), *StructProp->Struct->GetName());
        return false;
    }
    // Array
    else if (Property->IsA<FArrayProperty>())
    {
        FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property);
        const TArray<TSharedPtr<FJsonValue>>* ArrValue;
        if (!Value->TryGetArray(ArrValue))
        {
            OutErrorMessage = TEXT("Array property requires a JSON array");
            return false;
        }
        FScriptArrayHelper ArrayHelper(ArrayProp, ValueAddr);
        ArrayHelper.EmptyValues();
        FProperty* InnerProp = ArrayProp->Inner;
        for (int32 i = 0; i < ArrValue->Num(); ++i)
        {
            int32 NewIndex = ArrayHelper.AddValue();
            void* ElemAddr = ArrayHelper.GetRawPtr(NewIndex);
            FString InnerError;
            if (!SetPropertyValue(InnerProp, ElemAddr, (*ArrValue)[i], InnerError))
            {
                UE_LOG(LogTemp, Warning, TEXT("Failed to set array element %d: %s"), i, *InnerError);
            }
        }
        return true;
    }
    // Class reference (TSubclassOf)
    else if (Property->IsA<FClassProperty>())
    {
        FClassProperty* ClassProp = CastField<FClassProperty>(Property);
        FString ClassPath = Value->AsString();
        if (ClassPath.IsEmpty())
        {
            ClassProp->SetPropertyValue(ValueAddr, nullptr);
            return true;
        }
        UClass* LoadedClass = LoadObject<UClass>(nullptr, *ClassPath);
        if (!LoadedClass)
        {
            LoadedClass = FindFirstObject<UClass>(*ClassPath, EFindFirstObjectOptions::NativeFirst);
        }
        if (LoadedClass)
        {
            ClassProp->SetPropertyValue(ValueAddr, LoadedClass);
            return true;
        }
        OutErrorMessage = FString::Printf(TEXT("Could not load class: %s"), *ClassPath);
        return false;
    }
    // Object reference
    else if (Property->IsA<FObjectProperty>())
    {
        FObjectProperty* ObjProp = CastField<FObjectProperty>(Property);
        FString AssetPath = Value->AsString();
        if (AssetPath.IsEmpty())
        {
            ObjProp->SetPropertyValue(ValueAddr, nullptr);
            return true;
        }
        UObject* LoadedObj = LoadObject<UObject>(nullptr, *AssetPath);
        if (LoadedObj)
        {
            ObjProp->SetPropertyValue(ValueAddr, LoadedObj);
            return true;
        }
        OutErrorMessage = FString::Printf(TEXT("Could not load object: %s"), *AssetPath);
        return false;
    }

    OutErrorMessage = FString::Printf(TEXT("Unsupported property type in SetPropertyValue: %s"),
        *Property->GetClass()->GetName());
    return false;
}

TSharedPtr<FJsonValue> FUnrealMCPCommonUtils::GetPropertyAsJson(FProperty* Property, const void* ContainerPtr)
{
    if (!Property || !ContainerPtr)
    {
        return MakeShared<FJsonValueNull>();
    }

    const void* ValueAddr = Property->ContainerPtrToValuePtr<void>(ContainerPtr);
    return GetPropertyValueAsJson(Property, ValueAddr);
}

TSharedPtr<FJsonValue> FUnrealMCPCommonUtils::GetPropertyValueAsJson(FProperty* Property, const void* ValueAddr)
{
    if (!Property || !ValueAddr)
    {
        return MakeShared<FJsonValueNull>();
    }

    // Bool
    if (Property->IsA<FBoolProperty>())
    {
        return MakeShared<FJsonValueBoolean>(CastField<FBoolProperty>(Property)->GetPropertyValue(ValueAddr));
    }
    // Int
    else if (Property->IsA<FIntProperty>())
    {
        return MakeShared<FJsonValueNumber>(CastField<FIntProperty>(Property)->GetPropertyValue(ValueAddr));
    }
    // Float
    else if (Property->IsA<FFloatProperty>())
    {
        return MakeShared<FJsonValueNumber>(CastField<FFloatProperty>(Property)->GetPropertyValue(ValueAddr));
    }
    // Double
    else if (Property->IsA<FDoubleProperty>())
    {
        return MakeShared<FJsonValueNumber>(CastField<FDoubleProperty>(Property)->GetPropertyValue(ValueAddr));
    }
    // String
    else if (Property->IsA<FStrProperty>())
    {
        return MakeShared<FJsonValueString>(CastField<FStrProperty>(Property)->GetPropertyValue(ValueAddr));
    }
    // Name
    else if (Property->IsA<FNameProperty>())
    {
        return MakeShared<FJsonValueString>(CastField<FNameProperty>(Property)->GetPropertyValue(ValueAddr).ToString());
    }
    // Text
    else if (Property->IsA<FTextProperty>())
    {
        return MakeShared<FJsonValueString>(CastField<FTextProperty>(Property)->GetPropertyValue(ValueAddr).ToString());
    }
    // Byte
    else if (Property->IsA<FByteProperty>())
    {
        FByteProperty* ByteProp = CastField<FByteProperty>(Property);
        UEnum* EnumDef = ByteProp->GetIntPropertyEnum();
        if (EnumDef)
        {
            return MakeShared<FJsonValueString>(EnumDef->GetNameStringByValue(ByteProp->GetPropertyValue(ValueAddr)));
        }
        return MakeShared<FJsonValueNumber>(ByteProp->GetPropertyValue(ValueAddr));
    }
    // Enum
    else if (Property->IsA<FEnumProperty>())
    {
        FEnumProperty* EnumProp = CastField<FEnumProperty>(Property);
        UEnum* EnumDef = EnumProp->GetEnum();
        FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
        if (EnumDef && UnderlyingProp)
        {
            int64 Val = UnderlyingProp->GetSignedIntPropertyValue(ValueAddr);
            return MakeShared<FJsonValueString>(EnumDef->GetNameStringByValue(Val));
        }
        return MakeShared<FJsonValueNull>();
    }
    // Struct
    else if (Property->IsA<FStructProperty>())
    {
        FStructProperty* StructProp = CastField<FStructProperty>(Property);

        // FGameplayTag
        if (StructProp->Struct == FGameplayTag::StaticStruct()
            || StructProp->Struct->GetFName() == FName("GameplayTag"))
        {
            FString ExportedValue;
            StructProp->ExportText_Direct(ExportedValue, ValueAddr, ValueAddr, nullptr, PPF_None);
            int32 Start = ExportedValue.Find(TEXT("TagName=\""), ESearchCase::CaseSensitive);
            if (Start != INDEX_NONE)
            {
                Start += 9;
                int32 End = ExportedValue.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
                if (End != INDEX_NONE)
                {
                    FString TagName = ExportedValue.Mid(Start, End - Start);
                    return MakeShared<FJsonValueString>(TagName.IsEmpty() ? TEXT("None") : TagName);
                }
            }
            return MakeShared<FJsonValueString>(TEXT("None"));
        }
        // FGameplayTagContainer
        else if (StructProp->Struct == FGameplayTagContainer::StaticStruct()
                 || StructProp->Struct->GetFName() == FName("GameplayTagContainer"))
        {
            FString ExportedValue;
            StructProp->ExportText_Direct(ExportedValue, ValueAddr, ValueAddr, nullptr, PPF_None);
            TArray<TSharedPtr<FJsonValue>> TagArray;
            FString ParseStr = ExportedValue;
            FString TagName;
            int32 Idx = 0;
            while ((Idx = ParseStr.Find(TEXT("TagName=\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Idx)) != INDEX_NONE)
            {
                Idx += 9;
                int32 EndIdx = ParseStr.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Idx);
                if (EndIdx != INDEX_NONE)
                {
                    TagName = ParseStr.Mid(Idx, EndIdx - Idx);
                    TagArray.Add(MakeShared<FJsonValueString>(TagName));
                    Idx = EndIdx + 1;
                }
                else break;
            }
            return MakeShared<FJsonValueArray>(TagArray);
        }
        // FLinearColor
        else if (StructProp->Struct == TBaseStructure<FLinearColor>::Get())
        {
            const FLinearColor* Color = static_cast<const FLinearColor*>(ValueAddr);
            TArray<TSharedPtr<FJsonValue>> Arr;
            Arr.Add(MakeShared<FJsonValueNumber>(Color->R));
            Arr.Add(MakeShared<FJsonValueNumber>(Color->G));
            Arr.Add(MakeShared<FJsonValueNumber>(Color->B));
            Arr.Add(MakeShared<FJsonValueNumber>(Color->A));
            return MakeShared<FJsonValueArray>(Arr);
        }
        // FVector
        else if (StructProp->Struct == TBaseStructure<FVector>::Get())
        {
            const FVector* Vec = static_cast<const FVector*>(ValueAddr);
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetNumberField(TEXT("X"), Vec->X);
            Obj->SetNumberField(TEXT("Y"), Vec->Y);
            Obj->SetNumberField(TEXT("Z"), Vec->Z);
            return MakeShared<FJsonValueObject>(Obj);
        }
        // FRotator
        else if (StructProp->Struct == TBaseStructure<FRotator>::Get())
        {
            const FRotator* Rot = static_cast<const FRotator*>(ValueAddr);
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetNumberField(TEXT("Pitch"), Rot->Pitch);
            Obj->SetNumberField(TEXT("Yaw"), Rot->Yaw);
            Obj->SetNumberField(TEXT("Roll"), Rot->Roll);
            return MakeShared<FJsonValueObject>(Obj);
        }
        // Generic struct — recurse into fields
        else
        {
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            UScriptStruct* Struct = StructProp->Struct;
            for (TFieldIterator<FProperty> It(Struct); It; ++It)
            {
                FProperty* InnerProp = *It;
                const void* InnerValueAddr = InnerProp->ContainerPtrToValuePtr<void>(ValueAddr);
                TSharedPtr<FJsonValue> InnerVal = GetPropertyValueAsJson(InnerProp, InnerValueAddr);
                Obj->SetField(InnerProp->GetName(), InnerVal);
            }
            return MakeShared<FJsonValueObject>(Obj);
        }
    }
    // Array — recurse into elements
    else if (Property->IsA<FArrayProperty>())
    {
        FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property);
        FScriptArrayHelper ArrayHelper(ArrayProp, ValueAddr);
        TArray<TSharedPtr<FJsonValue>> Arr;
        FProperty* InnerProp = ArrayProp->Inner;
        for (int32 i = 0; i < ArrayHelper.Num(); ++i)
        {
            const void* ElemAddr = ArrayHelper.GetRawPtr(i);
            Arr.Add(GetPropertyValueAsJson(InnerProp, ElemAddr));
        }
        return MakeShared<FJsonValueArray>(Arr);
    }
    // Class reference
    else if (Property->IsA<FClassProperty>())
    {
        UClass* ClassValue = Cast<UClass>(CastField<FClassProperty>(Property)->GetPropertyValue(ValueAddr));
        if (ClassValue)
        {
            return MakeShared<FJsonValueString>(ClassValue->GetPathName());
        }
        return MakeShared<FJsonValueString>(TEXT("None"));
    }
    // Soft object reference
    else if (Property->IsA<FSoftObjectProperty>())
    {
        FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property);
        const FSoftObjectPtr* SoftPtr = static_cast<const FSoftObjectPtr*>(ValueAddr);
        FString Path = SoftPtr->ToSoftObjectPath().ToString();
        return MakeShared<FJsonValueString>(Path.IsEmpty() ? TEXT("None") : Path);
    }
    // Soft class reference
    else if (Property->IsA<FSoftClassProperty>())
    {
        const FSoftObjectPtr* SoftPtr = static_cast<const FSoftObjectPtr*>(ValueAddr);
        FString Path = SoftPtr->ToSoftObjectPath().ToString();
        return MakeShared<FJsonValueString>(Path.IsEmpty() ? TEXT("None") : Path);
    }
    // Object reference
    else if (Property->IsA<FObjectProperty>())
    {
        UObject* ObjValue = CastField<FObjectProperty>(Property)->GetPropertyValue(ValueAddr);
        if (ObjValue)
        {
            return MakeShared<FJsonValueString>(ObjValue->GetPathName());
        }
        return MakeShared<FJsonValueString>(TEXT("None"));
    }

    // Fallback: export as text
    FString ExportedVal;
    Property->ExportTextItem_Direct(ExportedVal, ValueAddr, nullptr, nullptr, PPF_None);
    return MakeShared<FJsonValueString>(ExportedVal);
}