#include "Commands/UnrealMCPBlueprintCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
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
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

FUnrealMCPBlueprintCommands::FUnrealMCPBlueprintCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_blueprint"))
    {
        return HandleCreateBlueprint(Params);
    }
    else if (CommandType == TEXT("add_component_to_blueprint"))
    {
        return HandleAddComponentToBlueprint(Params);
    }
    else if (CommandType == TEXT("set_component_property"))
    {
        return HandleSetComponentProperty(Params);
    }
    else if (CommandType == TEXT("set_physics_properties"))
    {
        return HandleSetPhysicsProperties(Params);
    }
    else if (CommandType == TEXT("compile_blueprint"))
    {
        return HandleCompileBlueprint(Params);
    }
    else if (CommandType == TEXT("spawn_blueprint_actor"))
    {
        return HandleSpawnBlueprintActor(Params);
    }
    else if (CommandType == TEXT("set_blueprint_property"))
    {
        return HandleSetBlueprintProperty(Params);
    }
    else if (CommandType == TEXT("set_static_mesh_properties"))
    {
        return HandleSetStaticMeshProperties(Params);
    }
    else if (CommandType == TEXT("set_pawn_properties"))
    {
        return HandleSetPawnProperties(Params);
    }
    else if (CommandType == TEXT("list_blueprint_components"))
    {
        return HandleListBlueprintComponents(Params);
    }
    else if (CommandType == TEXT("get_blueprint_component_properties"))
    {
        return HandleGetBlueprintComponentProperties(Params);
    }
    else if (CommandType == TEXT("remove_blueprint_component"))
    {
        return HandleRemoveBlueprintComponent(Params);
    }
    else if (CommandType == TEXT("reparent_blueprint_component"))
    {
        return HandleReparentBlueprintComponent(Params);
    }
    else if (CommandType == TEXT("get_blueprint_class_settings"))
    {
        return HandleGetBlueprintClassSettings(Params);
    }
    else if (CommandType == TEXT("add_blueprint_interface"))
    {
        return HandleAddBlueprintInterface(Params);
    }
    else if (CommandType == TEXT("remove_blueprint_interface"))
    {
        return HandleRemoveBlueprintInterface(Params);
    }
    else if (CommandType == TEXT("get_blueprint_defaults"))
    {
        return HandleGetBlueprintDefaults(Params);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown blueprint command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleCreateBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Check if blueprint already exists
    FString PackagePath = TEXT("/Game/Blueprints/");
    FString AssetName = BlueprintName;
    if (UEditorAssetLibrary::DoesAssetExist(PackagePath + AssetName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint already exists: %s"), *BlueprintName));
    }

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Create the blueprint factory
    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    
    // Handle parent class
    FString ParentClass;
    Params->TryGetStringField(TEXT("parent_class"), ParentClass);
    
    // Default to Actor if no parent class specified
    UClass* SelectedParentClass = AActor::StaticClass();
    
    // Try to find the specified parent class
    if (!ParentClass.IsEmpty())
    {
        FString ClassName = ParentClass;
        if (!ClassName.StartsWith(TEXT("A")))
        {
            ClassName = TEXT("A") + ClassName;
        }
        
        // First try direct StaticClass lookup for common classes
        UClass* FoundClass = nullptr;
        if (ClassName == TEXT("APawn"))
        {
            FoundClass = APawn::StaticClass();
        }
        else if (ClassName == TEXT("AActor"))
        {
            FoundClass = AActor::StaticClass();
        }
        else
        {
            // Try loading the class using LoadClass which is more reliable than FindObject
            const FString ClassPath = FString::Printf(TEXT("/Script/Engine.%s"), *ClassName);
            FoundClass = LoadClass<AActor>(nullptr, *ClassPath);
            
            if (!FoundClass)
            {
                // Try alternate paths if not found
                const FString GameClassPath = FString::Printf(TEXT("/Script/Game.%s"), *ClassName);
                FoundClass = LoadClass<AActor>(nullptr, *GameClassPath);
            }
        }

        if (FoundClass)
        {
            SelectedParentClass = FoundClass;
            UE_LOG(LogTemp, Log, TEXT("Successfully set parent class to '%s'"), *ClassName);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Could not find specified parent class '%s' at paths: /Script/Engine.%s or /Script/Game.%s, defaulting to AActor"), 
                *ClassName, *ClassName, *ClassName);
        }
    }
    
    Factory->ParentClass = SelectedParentClass;

    // Create the blueprint
    UPackage* Package = CreatePackage(*(PackagePath + AssetName));
    UBlueprint* NewBlueprint = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *AssetName, RF_Standalone | RF_Public, nullptr, GWarn));

    if (NewBlueprint)
    {
        // Notify the asset registry
        FAssetRegistryModule::AssetCreated(NewBlueprint);

        // Mark the package dirty
        Package->MarkPackageDirty();

        bool bSave = true;
        if (Params->HasField(TEXT("save")))
        {
            bSave = Params->GetBoolField(TEXT("save"));
        }
        if (bSave && NewBlueprint)
        {
            FString SavePath = NewBlueprint->GetOutermost()->GetName();
            UEditorAssetLibrary::SaveAsset(SavePath, false);
        }

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("name"), AssetName);
        ResultObj->SetStringField(TEXT("path"), PackagePath + AssetName);
        return ResultObj;
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create blueprint"));
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleAddComponentToBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentType;
    if (!Params->TryGetStringField(TEXT("component_type"), ComponentType))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Create the component - dynamically find the component class by name
    UClass* ComponentClass = nullptr;

    // Try to find the class with exact name first
    ComponentClass = FindObject<UClass>(nullptr, *ComponentType);
    
    // If not found, try with "Component" suffix
    if (!ComponentClass && !ComponentType.EndsWith(TEXT("Component")))
    {
        FString ComponentTypeWithSuffix = ComponentType + TEXT("Component");
        ComponentClass = FindObject<UClass>(nullptr, *ComponentTypeWithSuffix);
    }
    
    // If still not found, try with "U" prefix
    if (!ComponentClass && !ComponentType.StartsWith(TEXT("U")))
    {
        FString ComponentTypeWithPrefix = TEXT("U") + ComponentType;
        ComponentClass = FindObject<UClass>(nullptr, *ComponentTypeWithPrefix);
        
        // Try with both prefix and suffix
        if (!ComponentClass && !ComponentType.EndsWith(TEXT("Component")))
        {
            FString ComponentTypeWithBoth = TEXT("U") + ComponentType + TEXT("Component");
            ComponentClass = FindObject<UClass>(nullptr, *ComponentTypeWithBoth);
        }
    }
    
    // Verify that the class is a valid component type
    if (!ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown component type: %s"), *ComponentType));
    }

    // Add the component to the blueprint
    USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(ComponentClass, *ComponentName);
    if (NewNode)
    {
        // Set transform if provided
        USceneComponent* SceneComponent = Cast<USceneComponent>(NewNode->ComponentTemplate);
        if (SceneComponent)
        {
            if (Params->HasField(TEXT("location")))
            {
                SceneComponent->SetRelativeLocation(FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")));
            }
            if (Params->HasField(TEXT("rotation")))
            {
                SceneComponent->SetRelativeRotation(FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation")));
            }
            if (Params->HasField(TEXT("scale")))
            {
                SceneComponent->SetRelativeScale3D(FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
            }
        }

        // Add to parent component or root
        FString ParentComponentName;
        if (Params->TryGetStringField(TEXT("parent_component"), ParentComponentName) && !ParentComponentName.IsEmpty())
        {
            USCS_Node* ParentNode = nullptr;
            for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
            {
                if (Node && Node->GetVariableName().ToString() == ParentComponentName)
                {
                    ParentNode = Node;
                    break;
                }
            }
            if (!ParentNode)
            {
                return FUnrealMCPCommonUtils::CreateErrorResponse(
                    FString::Printf(TEXT("Parent component not found: %s"), *ParentComponentName));
            }
            ParentNode->AddChildNode(NewNode, /*bAddToAllNodes=*/true);
        }
        else
        {
            Blueprint->SimpleConstructionScript->AddNode(NewNode);
        }

        // Compile the blueprint
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

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
        ResultObj->SetStringField(TEXT("component_name"), ComponentName);
        ResultObj->SetStringField(TEXT("component_type"), ComponentType);
        return ResultObj;
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to add component to blueprint"));
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleSetComponentProperty(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName, ComponentName, PropertyName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
    if (!Params->HasField(TEXT("property_value")))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_value' parameter"));

    // Find blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));

    if (!Blueprint->SimpleConstructionScript)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Invalid blueprint construction script"));

    // Find component node
    USCS_Node* ComponentNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }
    if (!ComponentNode || !ComponentNode->ComponentTemplate)
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));

    // Set property using generic handler
    TSharedPtr<FJsonValue> JsonValue = Params->Values.FindRef(TEXT("property_value"));
    FString ErrorMessage;
    if (!FUnrealMCPCommonUtils::SetObjectProperty(ComponentNode->ComponentTemplate, PropertyName, JsonValue, ErrorMessage))
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);

    // Mark modified and save
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    bool bSave = true;
    if (Params->HasField(TEXT("save")))
        bSave = Params->GetBoolField(TEXT("save"));
    if (bSave)
    {
        FString SavePath = Blueprint->GetOutermost()->GetName();
        UEditorAssetLibrary::SaveAsset(SavePath, false);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("component"), ComponentName);
    ResultObj->SetStringField(TEXT("property"), PropertyName);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleSetPhysicsProperties(const TSharedPtr<FJsonObject>& Params)
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

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Find the component
    USCS_Node* ComponentNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }

    if (!ComponentNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(ComponentNode->ComponentTemplate);
    if (!PrimComponent)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a primitive component"));
    }

    // Set physics properties
    if (Params->HasField(TEXT("simulate_physics")))
    {
        PrimComponent->SetSimulatePhysics(Params->GetBoolField(TEXT("simulate_physics")));
    }

    if (Params->HasField(TEXT("mass")))
    {
        float Mass = Params->GetNumberField(TEXT("mass"));
        // In UE5.5, use proper overrideMass instead of just scaling
        PrimComponent->SetMassOverrideInKg(NAME_None, Mass);
        UE_LOG(LogTemp, Display, TEXT("Set mass for component %s to %f kg"), *ComponentName, Mass);
    }

    if (Params->HasField(TEXT("linear_damping")))
    {
        PrimComponent->SetLinearDamping(Params->GetNumberField(TEXT("linear_damping")));
    }

    if (Params->HasField(TEXT("angular_damping")))
    {
        PrimComponent->SetAngularDamping(Params->GetNumberField(TEXT("angular_damping")));
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
    ResultObj->SetStringField(TEXT("component"), ComponentName);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Compile the blueprint
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

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
    ResultObj->SetStringField(TEXT("name"), BlueprintName);
    ResultObj->SetBoolField(TEXT("compiled"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }

    // Spawn the actor
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    FTransform SpawnTransform;
    SpawnTransform.SetLocation(Location);
    SpawnTransform.SetRotation(FQuat(Rotation));

    AActor* NewActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, SpawnTransform);
    if (NewActor)
    {
        NewActor->SetActorLabel(*ActorName);
        return FUnrealMCPCommonUtils::ActorToJsonObject(NewActor);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn blueprint actor"));
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleSetBlueprintProperty(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Get the default object
    UObject* DefaultObject = Blueprint->GeneratedClass->GetDefaultObject();
    if (!DefaultObject)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get default object"));
    }

    // Set the property value
    if (Params->HasField(TEXT("property_value")))
    {
        TSharedPtr<FJsonValue> JsonValue = Params->Values.FindRef(TEXT("property_value"));
        
        FString ErrorMessage;
        if (FUnrealMCPCommonUtils::SetObjectProperty(DefaultObject, PropertyName, JsonValue, ErrorMessage))
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
            ResultObj->SetStringField(TEXT("property"), PropertyName);
            ResultObj->SetBoolField(TEXT("success"), true);
            return ResultObj;
        }
        else
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
        }
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_value' parameter"));
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleSetStaticMeshProperties(const TSharedPtr<FJsonObject>& Params)
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

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Find the component
    USCS_Node* ComponentNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }

    if (!ComponentNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(ComponentNode->ComponentTemplate);
    if (!MeshComponent)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a static mesh component"));
    }

    // Set static mesh properties
    if (Params->HasField(TEXT("static_mesh")))
    {
        FString MeshPath = Params->GetStringField(TEXT("static_mesh"));
        UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
        if (Mesh)
        {
            MeshComponent->SetStaticMesh(Mesh);
        }
    }

    if (Params->HasField(TEXT("material")))
    {
        FString MaterialPath = Params->GetStringField(TEXT("material"));
        UMaterialInterface* Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
        if (Material)
        {
            MeshComponent->SetMaterial(0, Material);
        }
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("component"), ComponentName);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleSetPawnProperties(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the default object
    UObject* DefaultObject = Blueprint->GeneratedClass->GetDefaultObject();
    if (!DefaultObject)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get default object"));
    }

    // Track if any properties were set successfully
    bool bAnyPropertiesSet = false;
    TSharedPtr<FJsonObject> ResultsObj = MakeShared<FJsonObject>();
    
    // Set auto possess player if specified
    if (Params->HasField(TEXT("auto_possess_player")))
    {
        TSharedPtr<FJsonValue> AutoPossessValue = Params->Values.FindRef(TEXT("auto_possess_player"));
        
        FString ErrorMessage;
        if (FUnrealMCPCommonUtils::SetObjectProperty(DefaultObject, TEXT("AutoPossessPlayer"), AutoPossessValue, ErrorMessage))
        {
            bAnyPropertiesSet = true;
            TSharedPtr<FJsonObject> PropResultObj = MakeShared<FJsonObject>();
            PropResultObj->SetBoolField(TEXT("success"), true);
            ResultsObj->SetObjectField(TEXT("AutoPossessPlayer"), PropResultObj);
        }
        else
        {
            TSharedPtr<FJsonObject> PropResultObj = MakeShared<FJsonObject>();
            PropResultObj->SetBoolField(TEXT("success"), false);
            PropResultObj->SetStringField(TEXT("error"), ErrorMessage);
            ResultsObj->SetObjectField(TEXT("AutoPossessPlayer"), PropResultObj);
        }
    }
    
    // Set controller rotation properties
    const TCHAR* RotationProps[] = {
        TEXT("bUseControllerRotationYaw"),
        TEXT("bUseControllerRotationPitch"),
        TEXT("bUseControllerRotationRoll")
    };
    
    const TCHAR* ParamNames[] = {
        TEXT("use_controller_rotation_yaw"),
        TEXT("use_controller_rotation_pitch"),
        TEXT("use_controller_rotation_roll")
    };
    
    for (int32 i = 0; i < 3; i++)
    {
        if (Params->HasField(ParamNames[i]))
        {
            TSharedPtr<FJsonValue> Value = Params->Values.FindRef(ParamNames[i]);
            
            FString ErrorMessage;
            if (FUnrealMCPCommonUtils::SetObjectProperty(DefaultObject, RotationProps[i], Value, ErrorMessage))
            {
                bAnyPropertiesSet = true;
                TSharedPtr<FJsonObject> PropResultObj = MakeShared<FJsonObject>();
                PropResultObj->SetBoolField(TEXT("success"), true);
                ResultsObj->SetObjectField(RotationProps[i], PropResultObj);
            }
            else
            {
                TSharedPtr<FJsonObject> PropResultObj = MakeShared<FJsonObject>();
                PropResultObj->SetBoolField(TEXT("success"), false);
                PropResultObj->SetStringField(TEXT("error"), ErrorMessage);
                ResultsObj->SetObjectField(RotationProps[i], PropResultObj);
            }
        }
    }
    
    // Set can be damaged property
    if (Params->HasField(TEXT("can_be_damaged")))
    {
        TSharedPtr<FJsonValue> Value = Params->Values.FindRef(TEXT("can_be_damaged"));
        
        FString ErrorMessage;
        if (FUnrealMCPCommonUtils::SetObjectProperty(DefaultObject, TEXT("bCanBeDamaged"), Value, ErrorMessage))
        {
            bAnyPropertiesSet = true;
            TSharedPtr<FJsonObject> PropResultObj = MakeShared<FJsonObject>();
            PropResultObj->SetBoolField(TEXT("success"), true);
            ResultsObj->SetObjectField(TEXT("bCanBeDamaged"), PropResultObj);
        }
        else
        {
            TSharedPtr<FJsonObject> PropResultObj = MakeShared<FJsonObject>();
            PropResultObj->SetBoolField(TEXT("success"), false);
            PropResultObj->SetStringField(TEXT("error"), ErrorMessage);
            ResultsObj->SetObjectField(TEXT("bCanBeDamaged"), PropResultObj);
        }
    }

    // Mark the blueprint as modified if any properties were set
    if (bAnyPropertiesSet)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    }
    else if (ResultsObj->Values.Num() == 0)
    {
        // No properties were specified
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No properties specified to set"));
    }

    TSharedPtr<FJsonObject> ResponseObj = MakeShared<FJsonObject>();
    ResponseObj->SetStringField(TEXT("blueprint"), BlueprintName);
    ResponseObj->SetBoolField(TEXT("success"), bAnyPropertiesSet);
    ResponseObj->SetObjectField(TEXT("results"), ResultsObj);
    return ResponseObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleListBlueprintComponents(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    TArray<TSharedPtr<FJsonValue>> ComponentsArray;

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (SCS)
    {
        const TArray<USCS_Node*>& RootNodes = SCS->GetRootNodes();
        const TArray<USCS_Node*>& AllNodes = SCS->GetAllNodes();

        for (USCS_Node* Node : AllNodes)
        {
            if (!Node)
            {
                continue;
            }

            TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
            CompObj->SetStringField(TEXT("name"), Node->GetVariableName().ToString());

            if (Node->ComponentTemplate)
            {
                CompObj->SetStringField(TEXT("class"), Node->ComponentTemplate->GetClass()->GetName());
            }
            else if (Node->ComponentClass)
            {
                CompObj->SetStringField(TEXT("class"), Node->ComponentClass->GetName());
            }
            else
            {
                CompObj->SetStringField(TEXT("class"), TEXT("Unknown"));
            }

            bool bIsRoot = RootNodes.Contains(Node);
            CompObj->SetBoolField(TEXT("is_root"), bIsRoot);
            CompObj->SetStringField(TEXT("parent"), Node->ParentComponentOrVariableName.ToString());

            // Add transform info for scene components
            USceneComponent* SceneComp = Cast<USceneComponent>(Node->ComponentTemplate);
            if (SceneComp)
            {
                FVector Loc = SceneComp->GetRelativeLocation();
                TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
                LocObj->SetNumberField(TEXT("X"), Loc.X);
                LocObj->SetNumberField(TEXT("Y"), Loc.Y);
                LocObj->SetNumberField(TEXT("Z"), Loc.Z);
                CompObj->SetObjectField(TEXT("relative_location"), LocObj);

                FRotator Rot = SceneComp->GetRelativeRotation();
                TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
                RotObj->SetNumberField(TEXT("Pitch"), Rot.Pitch);
                RotObj->SetNumberField(TEXT("Yaw"), Rot.Yaw);
                RotObj->SetNumberField(TEXT("Roll"), Rot.Roll);
                CompObj->SetObjectField(TEXT("relative_rotation"), RotObj);

                FVector Scale = SceneComp->GetRelativeScale3D();
                TSharedPtr<FJsonObject> ScaleObj = MakeShared<FJsonObject>();
                ScaleObj->SetNumberField(TEXT("X"), Scale.X);
                ScaleObj->SetNumberField(TEXT("Y"), Scale.Y);
                ScaleObj->SetNumberField(TEXT("Z"), Scale.Z);
                CompObj->SetObjectField(TEXT("relative_scale"), ScaleObj);
            }

            ComponentsArray.Add(MakeShared<FJsonValueObject>(CompObj));
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
    ResultObj->SetArrayField(TEXT("components"), ComponentsArray);
    ResultObj->SetNumberField(TEXT("count"), ComponentsArray.Num());
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleGetBlueprintComponentProperties(const TSharedPtr<FJsonObject>& Params)
{
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

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    if (!Blueprint->SimpleConstructionScript)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint has no SimpleConstructionScript"));
    }

    // Find the component node
    USCS_Node* ComponentNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }

    if (!ComponentNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    UObject* Template = ComponentNode->ComponentTemplate;
    if (!Template)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component template is null"));
    }

    // Optional category filter
    FString CategoryFilter;
    Params->TryGetStringField(TEXT("category_filter"), CategoryFilter);

    TSharedPtr<FJsonObject> PropertiesObj = MakeShared<FJsonObject>();

    for (TFieldIterator<FProperty> PropIt(Template->GetClass()); PropIt; ++PropIt)
    {
        FProperty* Prop = *PropIt;

        // Skip transient and deprecated properties
        if (Prop->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
        {
            continue;
        }

        // Apply category filter if specified
        if (!CategoryFilter.IsEmpty())
        {
            FString Category = Prop->GetMetaData(TEXT("Category"));
            if (!Category.Contains(CategoryFilter))
            {
                continue;
            }
        }

        TSharedPtr<FJsonValue> Value = FUnrealMCPCommonUtils::GetPropertyAsJson(Prop, Template);
        if (Value.IsValid())
        {
            PropertiesObj->SetField(Prop->GetName(), Value);
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("component_name"), ComponentName);
    ResultObj->SetStringField(TEXT("component_class"), Template->GetClass()->GetName());
    ResultObj->SetObjectField(TEXT("properties"), PropertiesObj);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleRemoveBlueprintComponent(const TSharedPtr<FJsonObject>& Params)
{
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

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    if (!Blueprint->SimpleConstructionScript)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint has no SimpleConstructionScript"));
    }

    // Find the component node
    USCS_Node* ComponentNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }

    if (!ComponentNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    // Check promote_children option
    bool bPromoteChildren = false;
    if (Params->HasField(TEXT("promote_children")))
    {
        bPromoteChildren = Params->GetBoolField(TEXT("promote_children"));
    }

    if (bPromoteChildren)
    {
        Blueprint->SimpleConstructionScript->RemoveNodeAndPromoteChildren(ComponentNode);
    }
    else
    {
        Blueprint->SimpleConstructionScript->RemoveNode(ComponentNode);
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

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
    ResultObj->SetStringField(TEXT("removed_component"), ComponentName);
    ResultObj->SetBoolField(TEXT("children_promoted"), bPromoteChildren);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleReparentBlueprintComponent(const TSharedPtr<FJsonObject>& Params)
{
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

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint has no SimpleConstructionScript"));
    }

    // Find the node to move
    USCS_Node* NodeToMove = nullptr;
    for (USCS_Node* Node : SCS->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            NodeToMove = Node;
            break;
        }
    }

    if (!NodeToMove)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    // Determine new parent
    FString NewParentName;
    bool bHasNewParent = Params->TryGetStringField(TEXT("new_parent_name"), NewParentName) && !NewParentName.IsEmpty();

    USCS_Node* NewParentNode = nullptr;
    if (bHasNewParent)
    {
        for (USCS_Node* Node : SCS->GetAllNodes())
        {
            if (Node && Node->GetVariableName().ToString() == NewParentName)
            {
                NewParentNode = Node;
                break;
            }
        }

        if (!NewParentNode)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("New parent component not found: %s"), *NewParentName));
        }

        // Prevent parenting to self
        if (NewParentNode == NodeToMove)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Cannot reparent a component to itself"));
        }

        // Prevent parenting to own descendant (would create cycle)
        if (NewParentNode->IsChildOf(NodeToMove))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Cannot reparent a component to one of its own descendants"));
        }
    }

    // Detach from current parent — always remove from AllNodes for clean re-add
    USCS_Node* CurrentParent = SCS->FindParentNode(NodeToMove);

    if (CurrentParent)
    {
        CurrentParent->RemoveChildNode(NodeToMove, /*bRemoveFromAllNodes=*/true);
    }
    else
    {
        SCS->RemoveNode(NodeToMove, /*bValidateSceneRootNodes=*/false);
    }

    // Attach to new parent or make root — always re-add to AllNodes
    if (NewParentNode)
    {
        NewParentNode->AddChildNode(NodeToMove, /*bAddToAllNodes=*/true);
    }
    else
    {
        SCS->AddNode(NodeToMove);
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

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
    ResultObj->SetStringField(TEXT("component"), ComponentName);
    ResultObj->SetStringField(TEXT("new_parent"), bHasNewParent ? NewParentName : TEXT("(root)"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleGetBlueprintClassSettings(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
    ResultObj->SetBoolField(TEXT("success"), true);

    // Parent class info
    if (Blueprint->ParentClass)
    {
        TSharedPtr<FJsonObject> ParentObj = MakeShared<FJsonObject>();
        ParentObj->SetStringField(TEXT("name"), Blueprint->ParentClass->GetName());
        ParentObj->SetStringField(TEXT("path"), Blueprint->ParentClass->GetPathName());
        ResultObj->SetObjectField(TEXT("parent_class"), ParentObj);
    }

    // Interfaces
    TArray<TSharedPtr<FJsonValue>> InterfacesArray;
    for (const FBPInterfaceDescription& Iface : Blueprint->ImplementedInterfaces)
    {
        if (Iface.Interface)
        {
            TSharedPtr<FJsonObject> IfaceObj = MakeShared<FJsonObject>();
            IfaceObj->SetStringField(TEXT("name"), Iface.Interface->GetName());
            IfaceObj->SetStringField(TEXT("path"), Iface.Interface->GetPathName());
            InterfacesArray.Add(MakeShared<FJsonValueObject>(IfaceObj));
        }
    }
    ResultObj->SetArrayField(TEXT("interfaces"), InterfacesArray);

    // Flags
    TSharedPtr<FJsonObject> FlagsObj = MakeShared<FJsonObject>();
    FlagsObj->SetBoolField(TEXT("is_abstract"), Blueprint->bGenerateAbstractClass != 0);
    FlagsObj->SetBoolField(TEXT("is_deprecated"), Blueprint->bDeprecate != 0);
    ResultObj->SetObjectField(TEXT("flags"), FlagsObj);

    // Metadata
    TSharedPtr<FJsonObject> MetaObj = MakeShared<FJsonObject>();
    MetaObj->SetStringField(TEXT("category"), Blueprint->BlueprintCategory);
    MetaObj->SetStringField(TEXT("description"), Blueprint->BlueprintDescription);

    FString BlueprintTypeStr;
    switch (Blueprint->BlueprintType)
    {
        case BPTYPE_Normal:          BlueprintTypeStr = TEXT("Normal"); break;
        case BPTYPE_Const:           BlueprintTypeStr = TEXT("Const"); break;
        case BPTYPE_MacroLibrary:    BlueprintTypeStr = TEXT("MacroLibrary"); break;
        case BPTYPE_Interface:       BlueprintTypeStr = TEXT("Interface"); break;
        case BPTYPE_LevelScript:     BlueprintTypeStr = TEXT("LevelScript"); break;
        case BPTYPE_FunctionLibrary: BlueprintTypeStr = TEXT("FunctionLibrary"); break;
        default:                     BlueprintTypeStr = TEXT("Unknown"); break;
    }
    MetaObj->SetStringField(TEXT("blueprint_type"), BlueprintTypeStr);
    ResultObj->SetObjectField(TEXT("metadata"), MetaObj);

    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleAddBlueprintInterface(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString InterfaceName;
    if (!Params->TryGetStringField(TEXT("interface_name"), InterfaceName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'interface_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Find the interface class using multi-try pattern
    UClass* InterfaceClass = FindObject<UClass>(nullptr, *InterfaceName);

    // Try with "U" prefix
    if (!InterfaceClass && !InterfaceName.StartsWith(TEXT("U")))
    {
        InterfaceClass = FindObject<UClass>(nullptr, *(TEXT("U") + InterfaceName));
    }

    // Try common script paths
    if (!InterfaceClass)
    {
        TArray<FString> PathPrefixes = {
            TEXT("/Script/Engine."),
            TEXT("/Script/CoreUObject."),
            FString::Printf(TEXT("/Script/%s."), FApp::GetProjectName()),
            TEXT("/Script/UMG."),
            TEXT("/Script/GameplayAbilities.")
        };
        for (const FString& Prefix : PathPrefixes)
        {
            InterfaceClass = FindObject<UClass>(nullptr, *(Prefix + InterfaceName));
            if (InterfaceClass) break;

            // Also try with "U" prefix
            if (!InterfaceName.StartsWith(TEXT("U")))
            {
                InterfaceClass = FindObject<UClass>(nullptr, *(Prefix + TEXT("U") + InterfaceName));
                if (InterfaceClass) break;
            }
        }
    }

    if (!InterfaceClass)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Interface class not found: %s"), *InterfaceName));
    }

    if (!InterfaceClass->HasAnyClassFlags(CLASS_Interface))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Class '%s' is not an interface"), *InterfaceClass->GetName()));
    }

    // Check if already implemented
    for (const FBPInterfaceDescription& Iface : Blueprint->ImplementedInterfaces)
    {
        if (Iface.Interface == InterfaceClass)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Interface '%s' is already implemented"), *InterfaceClass->GetName()));
        }
    }

    // Add the interface
    FBPInterfaceDescription NewInterface;
    NewInterface.Interface = InterfaceClass;
    Blueprint->ImplementedInterfaces.Add(NewInterface);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

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
    ResultObj->SetStringField(TEXT("interface"), InterfaceClass->GetName());
    ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleRemoveBlueprintInterface(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString InterfaceName;
    if (!Params->TryGetStringField(TEXT("interface_name"), InterfaceName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'interface_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

    // Find the interface class using multi-try pattern
    UClass* InterfaceClass = FindObject<UClass>(nullptr, *InterfaceName);

    if (!InterfaceClass && !InterfaceName.StartsWith(TEXT("U")))
    {
        InterfaceClass = FindObject<UClass>(nullptr, *(TEXT("U") + InterfaceName));
    }

    if (!InterfaceClass)
    {
        TArray<FString> PathPrefixes = {
            TEXT("/Script/Engine."),
            TEXT("/Script/CoreUObject."),
            FString::Printf(TEXT("/Script/%s."), FApp::GetProjectName()),
            TEXT("/Script/UMG."),
            TEXT("/Script/GameplayAbilities.")
        };
        for (const FString& Prefix : PathPrefixes)
        {
            InterfaceClass = FindObject<UClass>(nullptr, *(Prefix + InterfaceName));
            if (InterfaceClass) break;

            if (!InterfaceName.StartsWith(TEXT("U")))
            {
                InterfaceClass = FindObject<UClass>(nullptr, *(Prefix + TEXT("U") + InterfaceName));
                if (InterfaceClass) break;
            }
        }
    }

    if (!InterfaceClass)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Interface class not found: %s"), *InterfaceName));
    }

    // Check that it IS implemented
    bool bFound = false;
    for (const FBPInterfaceDescription& Iface : Blueprint->ImplementedInterfaces)
    {
        if (Iface.Interface == InterfaceClass)
        {
            bFound = true;
            break;
        }
    }

    if (!bFound)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Interface '%s' is not implemented by this blueprint"), *InterfaceClass->GetName()));
    }

    // Remove the interface
    Blueprint->ImplementedInterfaces.RemoveAll([InterfaceClass](const FBPInterfaceDescription& Iface)
    {
        return Iface.Interface == InterfaceClass;
    });

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

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
    ResultObj->SetStringField(TEXT("removed_interface"), InterfaceClass->GetName());
    ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleGetBlueprintDefaults(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    if (!Blueprint->GeneratedClass)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint has no GeneratedClass"));
    }

    UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
    if (!CDO)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get default object"));
    }

    // Optional parameters
    FString CategoryFilter;
    Params->TryGetStringField(TEXT("category_filter"), CategoryFilter);

    bool bIncludeInherited = false;
    if (Params->HasField(TEXT("include_inherited")))
    {
        bIncludeInherited = Params->GetBoolField(TEXT("include_inherited"));
    }

    // Choose iteration flags
    EFieldIterationFlags IterFlags = EFieldIterationFlags::IncludeSuper;
    if (!bIncludeInherited)
    {
        IterFlags = EFieldIterationFlags::None;
    }

    TSharedPtr<FJsonObject> PropertiesObj = MakeShared<FJsonObject>();

    for (TFieldIterator<FProperty> PropIt(Blueprint->GeneratedClass, IterFlags); PropIt; ++PropIt)
    {
        FProperty* Prop = *PropIt;

        // Skip transient and deprecated properties
        if (Prop->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
        {
            continue;
        }

        // Apply category filter if specified
        FString Category = Prop->GetMetaData(TEXT("Category"));
        if (!CategoryFilter.IsEmpty() && !Category.Contains(CategoryFilter))
        {
            continue;
        }

        TSharedPtr<FJsonObject> PropInfo = MakeShared<FJsonObject>();

        // Value
        TSharedPtr<FJsonValue> Value = FUnrealMCPCommonUtils::GetPropertyAsJson(Prop, CDO);
        if (Value.IsValid())
        {
            PropInfo->SetField(TEXT("value"), Value);
        }

        // Type
        PropInfo->SetStringField(TEXT("type"), Prop->GetCPPType());

        // Category
        PropInfo->SetStringField(TEXT("category"), Category);

        // Flags
        TSharedPtr<FJsonObject> PropFlags = MakeShared<FJsonObject>();
        PropFlags->SetBoolField(TEXT("is_editable"), Prop->HasAnyPropertyFlags(CPF_Edit));
        PropFlags->SetBoolField(TEXT("is_blueprint_visible"), Prop->HasAnyPropertyFlags(CPF_BlueprintVisible));
        PropFlags->SetBoolField(TEXT("is_replicated"), Prop->HasAnyPropertyFlags(CPF_Net));
        PropInfo->SetObjectField(TEXT("flags"), PropFlags);

        PropertiesObj->SetObjectField(Prop->GetName(), PropInfo);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
    ResultObj->SetStringField(TEXT("class_name"), Blueprint->GeneratedClass->GetName());
    ResultObj->SetObjectField(TEXT("properties"), PropertiesObj);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}