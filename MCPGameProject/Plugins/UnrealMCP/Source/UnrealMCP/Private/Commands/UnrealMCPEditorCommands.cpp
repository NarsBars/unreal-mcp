#include "Commands/UnrealMCPEditorCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "ImageUtils.h"
#include "HighResScreenshot.h"
#include "Engine/GameViewportClient.h"
#include "FileHelpers.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "PlayInEditorDataTypes.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "UObject/UnrealType.h"
#include "IPythonScriptPlugin.h"
#include "PythonScriptTypes.h"
// PIE input driving
#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "EnhancedPlayerInput.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "GameplayTagContainer.h"
#if UNREALMCP_WITH_GMC
#include "Components/GMCOrganicMovementComponent.h"
#include "Components/GMCAbilityComponent.h"
#endif

FUnrealMCPEditorCommands::FUnrealMCPEditorCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    // Actor manipulation commands
    if (CommandType == TEXT("get_actors_in_level"))
    {
        return HandleGetActorsInLevel(Params);
    }
    else if (CommandType == TEXT("find_actors_by_name"))
    {
        return HandleFindActorsByName(Params);
    }
    else if (CommandType == TEXT("spawn_actor") || CommandType == TEXT("create_actor"))
    {
        if (CommandType == TEXT("create_actor"))
        {
            UE_LOG(LogTemp, Warning, TEXT("'create_actor' command is deprecated and will be removed in a future version. Please use 'spawn_actor' instead."));
        }
        return HandleSpawnActor(Params);
    }
    else if (CommandType == TEXT("delete_actor"))
    {
        return HandleDeleteActor(Params);
    }
    else if (CommandType == TEXT("set_actor_transform"))
    {
        return HandleSetActorTransform(Params);
    }
    else if (CommandType == TEXT("get_actor_properties"))
    {
        return HandleGetActorProperties(Params);
    }
    else if (CommandType == TEXT("set_actor_property"))
    {
        return HandleSetActorProperty(Params);
    }
    // Blueprint actor spawning
    else if (CommandType == TEXT("spawn_blueprint_actor"))
    {
        return HandleSpawnBlueprintActor(Params);
    }
    // Editor viewport commands
    else if (CommandType == TEXT("focus_viewport"))
    {
        return HandleFocusViewport(Params);
    }
    else if (CommandType == TEXT("take_screenshot"))
    {
        return HandleTakeScreenshot(Params);
    }
    // PIE control commands
    else if (CommandType == TEXT("start_pie"))
    {
        return HandleStartPIE(Params);
    }
    else if (CommandType == TEXT("stop_pie"))
    {
        return HandleStopPIE(Params);
    }
    else if (CommandType == TEXT("get_pie_state"))
    {
        return HandleGetPIEState(Params);
    }
    // PIE input driving (start-and-poll model)
    else if (CommandType == TEXT("pie_drive_input_start"))
    {
        return HandlePIEDriveInputStart(Params);
    }
    else if (CommandType == TEXT("pie_simulate_key_start"))
    {
        return HandlePIESimulateKeyStart(Params);
    }
    else if (CommandType == TEXT("pie_get_job_result"))
    {
        return HandlePIEGetJobResult(Params);
    }
    else if (CommandType == TEXT("pie_set_control_rotation"))
    {
        return HandlePIESetControlRotation(Params);
    }
    else if (CommandType == TEXT("pie_cancel_job"))
    {
        return HandlePIECancelJob(Params);
    }
    // Console command execution
    else if (CommandType == TEXT("execute_console_command"))
    {
        return HandleExecuteConsoleCommand(Params);
    }
    // Log query
    else if (CommandType == TEXT("get_editor_log"))
    {
        return HandleGetEditorLog(Params);
    }
    // Python scripting
    else if (CommandType == TEXT("execute_python"))
    {
        return HandleExecutePython(Params);
    }
    // Close editor
    else if (CommandType == TEXT("close_editor"))
    {
        return HandleCloseEditor(Params);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown editor command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params)
{
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> ActorArray;
    for (AActor* Actor : AllActors)
    {
        if (Actor)
        {
            ActorArray.Add(FUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), ActorArray);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params)
{
    FString Pattern;
    if (!Params->TryGetStringField(TEXT("pattern"), Pattern))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pattern' parameter"));
    }
    
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> MatchingActors;
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName().Contains(Pattern))
        {
            MatchingActors.Add(FUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), MatchingActors);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSpawnActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString ActorType;
    if (!Params->TryGetStringField(TEXT("type"), ActorType))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    // Get actor name (required parameter)
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Get optional transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // Create the actor based on type
    AActor* NewActor = nullptr;
    UWorld* World = GEditor->GetEditorWorldContext().World();

    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Check if an actor with this name already exists
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor with name '%s' already exists"), *ActorName));
        }
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;

    // Try known shortcuts first (backward compat)
    UClass* ActorClass = nullptr;
    if (ActorType == TEXT("StaticMeshActor")) ActorClass = AStaticMeshActor::StaticClass();
    else if (ActorType == TEXT("PointLight")) ActorClass = APointLight::StaticClass();
    else if (ActorType == TEXT("SpotLight")) ActorClass = ASpotLight::StaticClass();
    else if (ActorType == TEXT("DirectionalLight")) ActorClass = ADirectionalLight::StaticClass();
    else if (ActorType == TEXT("CameraActor")) ActorClass = ACameraActor::StaticClass();

    // Dynamic class lookup fallback
    if (!ActorClass)
    {
        ActorClass = FindObject<UClass>(nullptr, *ActorType);

        // Try with "A" prefix
        if (!ActorClass && !ActorType.StartsWith(TEXT("A")))
            ActorClass = FindObject<UClass>(nullptr, *(TEXT("A") + ActorType));

        // Try common script paths
        if (!ActorClass)
        {
            TArray<FString> SearchPaths = {
                FString::Printf(TEXT("/Script/Engine.%s"), *ActorType),
                FString::Printf(TEXT("/Script/%s.%s"), FApp::GetProjectName(), *ActorType),
            };
            for (const FString& Path : SearchPaths)
            {
                ActorClass = FindObject<UClass>(nullptr, *Path);
                if (ActorClass) break;
            }
        }
    }

    if (!ActorClass || !ActorClass->IsChildOf(AActor::StaticClass()))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown or invalid actor type: %s"), *ActorType));
    }

    NewActor = World->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParams);

    if (NewActor)
    {
        // Set scale (since SpawnActor only takes location and rotation)
        FTransform Transform = NewActor->GetTransform();
        Transform.SetScale3D(Scale);
        NewActor->SetActorTransform(Transform);

        // Return the created actor's details
        return FUnrealMCPCommonUtils::ActorToJsonObject(NewActor);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create actor"));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleDeleteActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            // Store actor info before deletion for the response
            TSharedPtr<FJsonObject> ActorInfo = FUnrealMCPCommonUtils::ActorToJsonObject(Actor);
            
            // Delete the actor
            Actor->Destroy();
            
            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetObjectField(TEXT("deleted_actor"), ActorInfo);
            return ResultObj;
        }
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get transform parameters
    FTransform NewTransform = TargetActor->GetTransform();

    if (Params->HasField(TEXT("location")))
    {
        NewTransform.SetLocation(FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        NewTransform.SetRotation(FQuat(FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"))));
    }
    if (Params->HasField(TEXT("scale")))
    {
        NewTransform.SetScale3D(FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
    }

    // Set the new transform
    TargetActor->SetActorTransform(NewTransform);

    // Return updated actor info
    return FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor);
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorProperties(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);

    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Optional filters
    FString CategoryFilter;
    Params->TryGetStringField(TEXT("category_filter"), CategoryFilter);
    bool bIncludeInherited = true;
    if (Params->HasField(TEXT("include_inherited")))
        bIncludeInherited = Params->GetBoolField(TEXT("include_inherited"));

    // Base info (name, class, transform)
    TSharedPtr<FJsonObject> ResultObj = FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor);

    // Reflect actual properties
    EFieldIterationFlags IterFlags = bIncludeInherited
        ? EFieldIterationFlags::IncludeSuper : EFieldIterationFlags::None;

    TSharedPtr<FJsonObject> PropsObj = MakeShared<FJsonObject>();
    for (TFieldIterator<FProperty> PropIt(TargetActor->GetClass(), IterFlags); PropIt; ++PropIt)
    {
        FProperty* Prop = *PropIt;
        if (Prop->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)) continue;

        if (!CategoryFilter.IsEmpty())
        {
            FString Category = Prop->GetMetaData(TEXT("Category"));
            if (!Category.Contains(CategoryFilter)) continue;
        }

        const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(TargetActor);
        TSharedPtr<FJsonValue> Value = FUnrealMCPCommonUtils::GetPropertyAsJson(Prop, ValuePtr);
        if (Value.IsValid())
        {
            PropsObj->SetField(Prop->GetName(), Value);
        }
    }
    ResultObj->SetObjectField(TEXT("properties"), PropsObj);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetActorProperty(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get property name
    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
    }

    // Get property value
    if (!Params->HasField(TEXT("property_value")))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_value' parameter"));
    }
    
    TSharedPtr<FJsonValue> PropertyValue = Params->Values.FindRef(TEXT("property_value"));
    
    // Set the property using our utility function
    FString ErrorMessage;
    if (FUnrealMCPCommonUtils::SetObjectProperty(TargetActor, PropertyName, PropertyValue, ErrorMessage))
    {
        // Property set successfully
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("actor"), ActorName);
        ResultObj->SetStringField(TEXT("property"), PropertyName);
        ResultObj->SetBoolField(TEXT("success"), true);
        
        // Also include the full actor details
        ResultObj->SetObjectField(TEXT("actor_details"), FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor));
        return ResultObj;
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
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
    if (BlueprintName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint name is empty"));
    }

    FString Root      = TEXT("/Game/Blueprints/");
    FString AssetPath = Root + BlueprintName;

    if (!FPackageName::DoesPackageExist(AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint '%s' not found – it must reside under /Game/Blueprints"), *BlueprintName));
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
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
    SpawnTransform.SetScale3D(Scale);

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;

    AActor* NewActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, SpawnTransform, SpawnParams);
    if (NewActor)
    {
        // For Blueprint actors, return basic info immediately without full serialization
        // Full serialization can hang on complex blueprints with construction scripts/events
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetBoolField(TEXT("success"), true);
        ResultObj->SetStringField(TEXT("name"), NewActor->GetName());
        ResultObj->SetStringField(TEXT("class"), NewActor->GetClass()->GetName());
        ResultObj->SetStringField(TEXT("blueprint"), BlueprintName);

        // Safe to get basic transform info
        FVector ActorLocation = NewActor->GetActorLocation();
        TArray<TSharedPtr<FJsonValue>> LocationArray;
        LocationArray.Add(MakeShared<FJsonValueNumber>(ActorLocation.X));
        LocationArray.Add(MakeShared<FJsonValueNumber>(ActorLocation.Y));
        LocationArray.Add(MakeShared<FJsonValueNumber>(ActorLocation.Z));
        ResultObj->SetArrayField(TEXT("location"), LocationArray);

        FRotator ActorRotation = NewActor->GetActorRotation();
        TArray<TSharedPtr<FJsonValue>> RotationArray;
        RotationArray.Add(MakeShared<FJsonValueNumber>(ActorRotation.Pitch));
        RotationArray.Add(MakeShared<FJsonValueNumber>(ActorRotation.Yaw));
        RotationArray.Add(MakeShared<FJsonValueNumber>(ActorRotation.Roll));
        ResultObj->SetArrayField(TEXT("rotation"), RotationArray);

        FVector ActorScale = NewActor->GetActorScale3D();
        TArray<TSharedPtr<FJsonValue>> ScaleArray;
        ScaleArray.Add(MakeShared<FJsonValueNumber>(ActorScale.X));
        ScaleArray.Add(MakeShared<FJsonValueNumber>(ActorScale.Y));
        ScaleArray.Add(MakeShared<FJsonValueNumber>(ActorScale.Z));
        ResultObj->SetArrayField(TEXT("scale"), ScaleArray);

        return ResultObj;
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn blueprint actor"));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleFocusViewport(const TSharedPtr<FJsonObject>& Params)
{
    // Get target actor name if provided
    FString TargetActorName;
    bool HasTargetActor = Params->TryGetStringField(TEXT("target"), TargetActorName);

    // Get location if provided
    FVector Location(0.0f, 0.0f, 0.0f);
    bool HasLocation = false;
    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
        HasLocation = true;
    }

    // Get distance
    float Distance = 1000.0f;
    if (Params->HasField(TEXT("distance")))
    {
        Distance = Params->GetNumberField(TEXT("distance"));
    }

    // Get orientation if provided
    FRotator Orientation(0.0f, 0.0f, 0.0f);
    bool HasOrientation = false;
    if (Params->HasField(TEXT("orientation")))
    {
        Orientation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("orientation"));
        HasOrientation = true;
    }

    // Get the active viewport
    FLevelEditorViewportClient* ViewportClient = (FLevelEditorViewportClient*)GEditor->GetActiveViewport()->GetClient();
    if (!ViewportClient)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get active viewport"));
    }

    // If we have a target actor, focus on it
    if (HasTargetActor)
    {
        // Find the actor
        AActor* TargetActor = nullptr;
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
        
        for (AActor* Actor : AllActors)
        {
            if (Actor && Actor->GetName() == TargetActorName)
            {
                TargetActor = Actor;
                break;
            }
        }

        if (!TargetActor)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *TargetActorName));
        }

        // Focus on the actor
        ViewportClient->SetViewLocation(TargetActor->GetActorLocation() - FVector(Distance, 0.0f, 0.0f));
    }
    // Otherwise use the provided location
    else if (HasLocation)
    {
        ViewportClient->SetViewLocation(Location - FVector(Distance, 0.0f, 0.0f));
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Either 'target' or 'location' must be provided"));
    }

    // Set orientation if provided
    if (HasOrientation)
    {
        ViewportClient->SetViewRotation(Orientation);
    }

    // Force viewport to redraw
    ViewportClient->Invalidate();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params)
{
    // Get file path parameter
    FString FilePath;
    if (!Params->TryGetStringField(TEXT("filepath"), FilePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'filepath' parameter"));
    }
    
    // Ensure the file path has a proper extension
    if (!FilePath.EndsWith(TEXT(".png")))
    {
        FilePath += TEXT(".png");
    }

    // Determine source: "auto" (default), "editor", or "pie"
    FString Source = TEXT("auto");
    Params->TryGetStringField(TEXT("source"), Source);

    FViewport* Viewport = nullptr;
    FString ActualSource;

    bool bPIEActive = GEditor && GEditor->PlayWorld && GEngine && GEngine->GameViewport;

    if (Source == TEXT("pie"))
    {
        if (!bPIEActive)
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("PIE is not active"));
        Viewport = GEngine->GameViewport->Viewport;
        ActualSource = TEXT("pie");
    }
    else if (Source == TEXT("editor"))
    {
        if (GEditor) Viewport = GEditor->GetActiveViewport();
        ActualSource = TEXT("editor");
    }
    else // "auto"
    {
        if (bPIEActive)
        {
            Viewport = GEngine->GameViewport->Viewport;
            ActualSource = TEXT("pie");
        }
        else if (GEditor)
        {
            Viewport = GEditor->GetActiveViewport();
            ActualSource = TEXT("editor");
        }
    }

    if (!Viewport)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No viewport available"));

    TArray<FColor> Bitmap;
    int32 Width = Viewport->GetSizeXY().X;
    int32 Height = Viewport->GetSizeXY().Y;
    FIntRect ViewportRect(0, 0, Width, Height);

    if (!Viewport->ReadPixels(Bitmap, FReadSurfaceDataFlags(), ViewportRect))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to read viewport pixels"));

    FImageView Image(Bitmap.GetData(), Width, Height);
    if (!FImageUtils::SaveImageByExtension(*FilePath, Image))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to save screenshot file"));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("filepath"), FilePath);
    ResultObj->SetStringField(TEXT("source"), ActualSource);
    ResultObj->SetNumberField(TEXT("width"), Width);
    ResultObj->SetNumberField(TEXT("height"), Height);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleStartPIE(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available"));

    if (GEditor->PlayWorld)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("PIE session already active"));

    FRequestPlaySessionParams SessionParams;
    GEditor->RequestPlaySession(SessionParams);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleStopPIE(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available"));

    if (!GEditor->PlayWorld)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active PIE session"));

    GEditor->RequestEndPlayMap();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetPIEState(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();

    bool bIsPlaying = GEditor && GEditor->PlayWorld != nullptr;
    bool bIsPaused = bIsPlaying && GEditor->PlayWorld->IsPaused();

    ResultObj->SetBoolField(TEXT("is_playing"), bIsPlaying);
    ResultObj->SetBoolField(TEXT("is_paused"), bIsPaused);
    return ResultObj;
}

// Output device that captures console command output into a string
class FMCPStringOutputDevice : public FOutputDevice
{
public:
    FString Output;

    virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
    {
        if (!Output.IsEmpty())
        {
            Output += TEXT("\n");
        }
        Output += V;
    }
};

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleExecuteConsoleCommand(const TSharedPtr<FJsonObject>& Params)
{
    FString Command;
    if (!Params->TryGetStringField(TEXT("command"), Command))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'command' parameter"));

    UWorld* TargetWorld = nullptr;
    if (GEditor && GEditor->PlayWorld)
    {
        TargetWorld = GEditor->PlayWorld;
    }
    else if (GEditor)
    {
        TargetWorld = GEditor->GetEditorWorldContext().World();
    }

    if (!TargetWorld)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No world available"));

    // Capture output from the console command
    FMCPStringOutputDevice CaptureDevice;
    bool bHandled = GEngine->Exec(TargetWorld, *Command, CaptureDevice);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("handled"), bHandled);
    ResultObj->SetStringField(TEXT("command"), Command);
    ResultObj->SetStringField(TEXT("output"), CaptureDevice.Output);
    ResultObj->SetStringField(TEXT("world"), GEditor && GEditor->PlayWorld ? TEXT("pie") : TEXT("editor"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetEditorLog(const TSharedPtr<FJsonObject>& Params)
{
    // Parameters
    int32 LineCount = 100;
    if (Params->HasField(TEXT("lines")))
    {
        LineCount = FMath::Clamp(static_cast<int32>(Params->GetNumberField(TEXT("lines"))), 1, 5000);
    }

    FString SeverityFilter;
    if (Params->HasField(TEXT("severity")))
    {
        SeverityFilter = Params->GetStringField(TEXT("severity"));
    }

    FString CategoryFilter;
    if (Params->HasField(TEXT("category")))
    {
        CategoryFilter = Params->GetStringField(TEXT("category"));
    }

    FString SearchFilter;
    if (Params->HasField(TEXT("search")))
    {
        SearchFilter = Params->GetStringField(TEXT("search"));
    }

    // Find the log file (named <ProjectName>.log by Unreal convention)
    FString LogFilePath = FPaths::ProjectLogDir() / FApp::GetProjectName() + TEXT(".log");
    if (!FPaths::FileExists(LogFilePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Log file not found at: %s"), *LogFilePath));
    }

    // Read the file with shared read access (editor holds a write lock on the active log)
    FString FileContent;
    {
        IFileHandle* FileHandle = FPlatformFileManager::Get().GetPlatformFile().OpenRead(*LogFilePath, /* bAllowWrite */ true);
        if (!FileHandle)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Failed to open log file: %s"), *LogFilePath));
        }

        int64 FileSize = FileHandle->Size();
        TArray<uint8> Buffer;
        Buffer.SetNumUninitialized(FileSize);
        bool bReadOk = FileHandle->Read(Buffer.GetData(), FileSize);
        delete FileHandle;

        if (!bReadOk)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to read log file"));
        }

        // Convert UTF-8 to FString
        FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Buffer.GetData()), Buffer.Num());
        FileContent = FString(Converter.Length(), Converter.Get());
    }

    // Split into lines
    TArray<FString> AllLines;
    FileContent.ParseIntoArrayLines(AllLines);

    // Apply filters
    TArray<FString> FilteredLines;
    for (const FString& Line : AllLines)
    {
        // Severity filter
        if (!SeverityFilter.IsEmpty())
        {
            if (SeverityFilter.Equals(TEXT("Error"), ESearchCase::IgnoreCase))
            {
                if (!Line.Contains(TEXT("Error:")) && !Line.Contains(TEXT("Error]")))
                {
                    continue;
                }
            }
            else if (SeverityFilter.Equals(TEXT("Warning"), ESearchCase::IgnoreCase))
            {
                if (!Line.Contains(TEXT("Warning:")) && !Line.Contains(TEXT("Warning]")))
                {
                    continue;
                }
            }
            else if (SeverityFilter.Equals(TEXT("WarningOrError"), ESearchCase::IgnoreCase))
            {
                bool bIsWarning = Line.Contains(TEXT("Warning:")) || Line.Contains(TEXT("Warning]"));
                bool bIsError = Line.Contains(TEXT("Error:")) || Line.Contains(TEXT("Error]"));
                if (!bIsWarning && !bIsError)
                {
                    continue;
                }
            }
        }

        // Category filter (e.g., "LogBlueprint", "LogCompile", "LogAngelscript")
        if (!CategoryFilter.IsEmpty())
        {
            if (!Line.Contains(CategoryFilter))
            {
                continue;
            }
        }

        // Search text filter
        if (!SearchFilter.IsEmpty())
        {
            if (!Line.Contains(SearchFilter))
            {
                continue;
            }
        }

        FilteredLines.Add(Line);
    }

    // Take last N lines
    int32 StartIndex = FMath::Max(0, FilteredLines.Num() - LineCount);
    TArray<TSharedPtr<FJsonValue>> LinesArray;
    for (int32 i = StartIndex; i < FilteredLines.Num(); ++i)
    {
        LinesArray.Add(MakeShared<FJsonValueString>(FilteredLines[i]));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("log_file"), LogFilePath);
    ResultObj->SetNumberField(TEXT("total_lines"), AllLines.Num());
    ResultObj->SetNumberField(TEXT("matched_lines"), FilteredLines.Num());
    ResultObj->SetNumberField(TEXT("returned_lines"), LinesArray.Num());
    ResultObj->SetArrayField(TEXT("lines"), LinesArray);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleExecutePython(const TSharedPtr<FJsonObject>& Params)
{
    FString Code;
    if (!Params->TryGetStringField(TEXT("code"), Code))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'code' parameter"));
    }

    // Check Python availability
    IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
    if (!PythonPlugin)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Python Script Plugin not found"));
    }
    if (!PythonPlugin->IsPythonAvailable())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Python is not available. Enable the Python Editor Script Plugin."));
    }

    // Determine execution mode
    FString Mode;
    Params->TryGetStringField(TEXT("mode"), Mode);

    FPythonCommandEx PythonCommand;
    PythonCommand.Command = Code;
    PythonCommand.FileExecutionScope = EPythonFileExecutionScope::Public; // Shared environment across calls

    if (Mode.Equals(TEXT("evaluate"), ESearchCase::IgnoreCase))
    {
        PythonCommand.ExecutionMode = EPythonCommandExecutionMode::EvaluateStatement;
    }
    else
    {
        // Default: ExecuteStatement prints result, ExecuteFile handles multi-line
        PythonCommand.ExecutionMode = Code.Contains(TEXT("\n"))
            ? EPythonCommandExecutionMode::ExecuteFile
            : EPythonCommandExecutionMode::ExecuteStatement;
    }

    bool bSuccess = PythonPlugin->ExecPythonCommandEx(PythonCommand);

    // Build log output array
    TArray<TSharedPtr<FJsonValue>> LogArray;
    for (const FPythonLogOutputEntry& Entry : PythonCommand.LogOutput)
    {
        TSharedPtr<FJsonObject> LogEntry = MakeShared<FJsonObject>();
        switch (Entry.Type)
        {
        case EPythonLogOutputType::Warning:
            LogEntry->SetStringField(TEXT("type"), TEXT("warning"));
            break;
        case EPythonLogOutputType::Error:
            LogEntry->SetStringField(TEXT("type"), TEXT("error"));
            break;
        default:
            LogEntry->SetStringField(TEXT("type"), TEXT("info"));
            break;
        }
        LogEntry->SetStringField(TEXT("message"), Entry.Output);
        LogArray.Add(MakeShared<FJsonValueObject>(LogEntry));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), bSuccess);
    ResultObj->SetStringField(TEXT("result"), PythonCommand.CommandResult);
    ResultObj->SetArrayField(TEXT("log"), LogArray);
    return ResultObj;
}

// ---------------------------------------------------------------------------
// close_editor — Save dirty packages and request editor exit
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleCloseEditor(const TSharedPtr<FJsonObject>& Params)
{
    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);

    if (bSave)
    {
        // Save all dirty packages
        FEditorFileUtils::SaveDirtyPackages(
            /*bPromptUserToSave=*/ false,
            /*bSaveMapPackages=*/ true,
            /*bSaveContentPackages=*/ true
        );
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("saved"), bSave);

    // Schedule the exit on the next tick so the response can be sent first
    AsyncTask(ENamedThreads::GameThread, []()
    {
        // Small delay to let TCP response flush
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) -> bool
        {
            FPlatformMisc::RequestExit(false);
            return false;
        }), 0.5f);
    });

    return ResultObj;
}

// =====================================================================================
// PIE input driving (start-and-poll model)
//
// MCPServerRunnable reads on a worker thread; UnrealMCPBridge::ExecuteCommand pumps each
// command body through AsyncTask(GameThread, ...) and blocks the worker on Future.Get().
// A long-running command body therefore freezes the editor (the game thread cannot tick
// while the task body is in flight). We instead register a per-job FTSTicker that runs
// each frame, accumulates samples, and unregisters itself when complete. The "start"
// handler returns a job_id immediately; a separate "result" handler reads the buffer.
// =====================================================================================

namespace
{
    struct FPIEJobSample
    {
        float TimeSec = 0.f;
        FVector Location = FVector::ZeroVector;
        FVector Velocity = FVector::ZeroVector;
        FRotator ControlRotation = FRotator::ZeroRotator;
        FName GMCMovementMode = NAME_None;
        TArray<FString> ActiveTags;
    };

    enum class EPIEJobKind : uint8 { DriveInput, SimulateKey };

    struct FPIEJobState
    {
        FGuid JobId;
        EPIEJobKind Kind = EPIEJobKind::DriveInput;
        float Elapsed = 0.f;
        float Duration = 0.f;
        float SampleDt = 0.1f;
        float NextSampleAt = 0.f;
        bool bDone = false;
        FString ErrorMessage;

        // DriveInput-specific
        enum class EDirMode : uint8 { World, Named } DirMode = EDirMode::World;
        FVector WorldDir = FVector::ZeroVector;
        FName NamedDir = NAME_None;
        bool bPinRotation = false;
        FRotator PinnedRotation = FRotator::ZeroRotator;

        // SimulateKey-specific
        TWeakObjectPtr<UInputAction> Action;

        TWeakObjectPtr<APawn> Pawn;
        TWeakObjectPtr<APlayerController> PC;
        TArray<FPIEJobSample> Samples;
        FTSTicker::FDelegateHandle TickHandle;
    };

    // Game-thread-only access (handlers run via AsyncTask(GameThread,...); ticker runs on game thread).
    static TMap<FGuid, TSharedPtr<FPIEJobState>> GPIEJobs;

    FVector ResolveNamedDirection(FName Name, const FRotator& ControlRot)
    {
        // Yaw-only basis so pitch doesn't push input into the floor or sky.
        const FRotator YawOnly(0.f, ControlRot.Yaw, 0.f);
        const FVector Fwd = YawOnly.Vector();
        const FVector Right = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y);

        const FString N = Name.ToString();
        if (N.Equals(TEXT("Forward"),      ESearchCase::IgnoreCase)) return Fwd;
        if (N.Equals(TEXT("Back"),         ESearchCase::IgnoreCase)) return -Fwd;
        if (N.Equals(TEXT("Backward"),     ESearchCase::IgnoreCase)) return -Fwd;
        if (N.Equals(TEXT("Right"),        ESearchCase::IgnoreCase)) return Right;
        if (N.Equals(TEXT("Left"),         ESearchCase::IgnoreCase)) return -Right;
        if (N.Equals(TEXT("ForwardRight"), ESearchCase::IgnoreCase)) return (Fwd + Right).GetSafeNormal();
        if (N.Equals(TEXT("ForwardLeft"),  ESearchCase::IgnoreCase)) return (Fwd - Right).GetSafeNormal();
        if (N.Equals(TEXT("BackRight"),    ESearchCase::IgnoreCase)) return (-Fwd + Right).GetSafeNormal();
        if (N.Equals(TEXT("BackLeft"),     ESearchCase::IgnoreCase)) return (-Fwd - Right).GetSafeNormal();
        return FVector::ZeroVector;
    }

    bool IsNamedDirectionValid(const FString& Name)
    {
        static const TCHAR* const Valid[] = {
            TEXT("Forward"), TEXT("Back"), TEXT("Backward"), TEXT("Right"), TEXT("Left"),
            TEXT("ForwardRight"), TEXT("ForwardLeft"), TEXT("BackRight"), TEXT("BackLeft")
        };
        for (const TCHAR* V : Valid)
        {
            if (Name.Equals(V, ESearchCase::IgnoreCase)) return true;
        }
        return false;
    }

    bool ResolvePIEPlayer(APlayerController*& OutPC, APawn*& OutPawn, FString& OutError)
    {
        if (!GEditor || !GEditor->PlayWorld)
        {
            OutError = TEXT("No active PIE session");
            return false;
        }
        OutPC = GEditor->PlayWorld->GetFirstPlayerController();
        if (!OutPC)
        {
            OutError = TEXT("PIE has no PlayerController yet");
            return false;
        }
        OutPawn = OutPC->GetPawn();
        if (!OutPawn)
        {
            OutError = TEXT("PlayerController has no possessed pawn yet");
            return false;
        }
        return true;
    }

    UInputAction* ResolveInputAction(const FString& ActionName, TArray<FString>& OutAttempts)
    {
        // Direct path
        if (ActionName.StartsWith(TEXT("/Game/")))
        {
            OutAttempts.Add(ActionName);
            return LoadObject<UInputAction>(nullptr, *ActionName);
        }
        // Project convention: /Game/Input/Actions/IA_<Name>.IA_<Name>
        const FString IAPath = FString::Printf(TEXT("/Game/Input/Actions/IA_%s.IA_%s"), *ActionName, *ActionName);
        OutAttempts.Add(IAPath);
        if (UInputAction* IA = LoadObject<UInputAction>(nullptr, *IAPath))
        {
            return IA;
        }
        // Raw name fallback: /Game/Input/Actions/<Name>.<Name>
        const FString RawPath = FString::Printf(TEXT("/Game/Input/Actions/%s.%s"), *ActionName, *ActionName);
        OutAttempts.Add(RawPath);
        return LoadObject<UInputAction>(nullptr, *RawPath);
    }

    void SamplePawnState(FPIEJobSample& OutSample, APawn* Pawn, APlayerController* PC, float TimeSec)
    {
        OutSample.TimeSec = TimeSec;
        OutSample.Location = Pawn->GetActorLocation();
        OutSample.Velocity = Pawn->GetVelocity();
        OutSample.ControlRotation = PC->GetControlRotation();
#if UNREALMCP_WITH_GMC
        // Optional GMC telemetry: movement mode + active gameplay tags from the GMC
        // ability system. Compiled in only when the GMC plugin is present in the
        // host project; otherwise these sample fields stay at their default values.
        if (UGMC_OrganicMovementCmp* GMC = Pawn->FindComponentByClass<UGMC_OrganicMovementCmp>())
        {
            // Short form: "Grounded" instead of "EGMC_MovementMode::Grounded".
            if (const UEnum* ModeEnum = StaticEnum<EGMC_MovementMode>())
            {
                OutSample.GMCMovementMode = FName(*ModeEnum->GetNameStringByValue(static_cast<int64>(GMC->GetMovementMode())));
            }
        }
        if (UGMC_AbilitySystemComponent* ASC = Pawn->FindComponentByClass<UGMC_AbilitySystemComponent>())
        {
            const FGameplayTagContainer Tags = ASC->GetActiveTags();
            for (auto It = Tags.CreateConstIterator(); It; ++It)
            {
                OutSample.ActiveTags.Add(It->ToString());
            }
        }
#endif
    }

    bool TickPIEJob(float DeltaTime, TWeakPtr<FPIEJobState> WeakJob)
    {
        TSharedPtr<FPIEJobState> Job = WeakJob.Pin();
        if (!Job.IsValid() || Job->bDone) return false; // unregister

        APawn* Pawn = Job->Pawn.Get();
        APlayerController* PC = Job->PC.Get();
        if (!Pawn || !PC || !GEditor || !GEditor->PlayWorld)
        {
            Job->ErrorMessage = TEXT("Pawn or PC invalidated mid-job");
            Job->bDone = true;
            return false;
        }

        // Apply input for this frame.
        if (Job->Kind == EPIEJobKind::DriveInput)
        {
            if (Job->bPinRotation) PC->SetControlRotation(Job->PinnedRotation);

            const FVector Dir = (Job->DirMode == FPIEJobState::EDirMode::World)
                ? Job->WorldDir
                : ResolveNamedDirection(Job->NamedDir, PC->GetControlRotation());
            Pawn->AddMovementInput(Dir, 1.f);
        }
        else // SimulateKey
        {
            if (UEnhancedPlayerInput* EPI = Cast<UEnhancedPlayerInput>(PC->PlayerInput))
            {
                if (UInputAction* IA = Job->Action.Get())
                {
                    EPI->InjectInputForAction(IA, FInputActionValue(1.f));
                }
            }
        }

        Job->Elapsed += DeltaTime;

        if (Job->Elapsed >= Job->NextSampleAt)
        {
            FPIEJobSample S;
            SamplePawnState(S, Pawn, PC, Job->Elapsed);
            Job->Samples.Add(MoveTemp(S));
            Job->NextSampleAt = Job->Elapsed + Job->SampleDt;
        }

        if (Job->Elapsed >= Job->Duration)
        {
            // Release SimulateKey action.
            if (Job->Kind == EPIEJobKind::SimulateKey)
            {
                if (UEnhancedPlayerInput* EPI = Cast<UEnhancedPlayerInput>(PC->PlayerInput))
                {
                    if (UInputAction* IA = Job->Action.Get())
                    {
                        EPI->InjectInputForAction(IA, FInputActionValue(0.f));
                    }
                }
            }
            Job->bDone = true;
            return false; // unregister ticker
        }

        return true; // keep ticking
    }

    TSharedRef<FJsonObject> JsonFromVector(const FVector& V)
    {
        TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetNumberField(TEXT("x"), V.X);
        O->SetNumberField(TEXT("y"), V.Y);
        O->SetNumberField(TEXT("z"), V.Z);
        return O;
    }

    TSharedRef<FJsonObject> JsonFromRotator(const FRotator& R)
    {
        TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetNumberField(TEXT("pitch"), R.Pitch);
        O->SetNumberField(TEXT("yaw"), R.Yaw);
        O->SetNumberField(TEXT("roll"), R.Roll);
        return O;
    }

    TSharedRef<FJsonObject> SampleToJson(const FPIEJobSample& S)
    {
        TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetNumberField(TEXT("time"), S.TimeSec);
        O->SetObjectField(TEXT("location"), JsonFromVector(S.Location));
        O->SetObjectField(TEXT("velocity"), JsonFromVector(S.Velocity));
        O->SetObjectField(TEXT("control_rotation"), JsonFromRotator(S.ControlRotation));
        O->SetStringField(TEXT("gmc_movement_mode"), S.GMCMovementMode.ToString());
        TArray<TSharedPtr<FJsonValue>> TagsJson;
        TagsJson.Reserve(S.ActiveTags.Num());
        for (const FString& T : S.ActiveTags) TagsJson.Add(MakeShared<FJsonValueString>(T));
        O->SetArrayField(TEXT("active_tags"), TagsJson);
        return O;
    }

    TSharedRef<FJsonObject> JobStateToJson(const FPIEJobState& Job)
    {
        TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("job_id"), Job.JobId.ToString(EGuidFormats::DigitsWithHyphensLower));
        O->SetBoolField(TEXT("done"), Job.bDone);
        O->SetNumberField(TEXT("elapsed"), Job.Elapsed);
        O->SetNumberField(TEXT("duration"), Job.Duration);
        if (!Job.ErrorMessage.IsEmpty())
        {
            O->SetStringField(TEXT("error"), Job.ErrorMessage);
        }
        TArray<TSharedPtr<FJsonValue>> SamplesJson;
        SamplesJson.Reserve(Job.Samples.Num());
        for (const FPIEJobSample& S : Job.Samples)
        {
            SamplesJson.Add(MakeShared<FJsonValueObject>(SampleToJson(S)));
        }
        O->SetArrayField(TEXT("samples"), SamplesJson);
        return O;
    }

    void RegisterJobTicker(TSharedRef<FPIEJobState> Job)
    {
        TWeakPtr<FPIEJobState> WeakJob = Job;
        Job->TickHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda([WeakJob](float Dt) -> bool
            {
                return TickPIEJob(Dt, WeakJob);
            }), 0.f);
    }
} // namespace

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandlePIEDriveInputStart(const TSharedPtr<FJsonObject>& Params)
{
    APlayerController* PC = nullptr;
    APawn* Pawn = nullptr;
    FString Error;
    if (!ResolvePIEPlayer(PC, Pawn, Error))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    double Duration = 0.0;
    if (!Params->TryGetNumberField(TEXT("duration_sec"), Duration) || Duration <= 0.0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or non-positive 'duration_sec'"));
    }

    double SampleDt = 0.1;
    Params->TryGetNumberField(TEXT("sample_dt_sec"), SampleDt);
    if (SampleDt <= 0.0) SampleDt = 0.1;

    const TArray<TSharedPtr<FJsonValue>>* WorldArr = nullptr;
    FString NamedStr;
    const bool bHasWorld = Params->TryGetArrayField(TEXT("direction_world"), WorldArr) && WorldArr && WorldArr->Num() == 3;
    const bool bHasNamed = Params->TryGetStringField(TEXT("direction_named"), NamedStr) && !NamedStr.IsEmpty();
    if (bHasWorld == bHasNamed)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Pass exactly one of 'direction_world' (3 floats) or 'direction_named'"));
    }

    TSharedRef<FPIEJobState> Job = MakeShared<FPIEJobState>();
    Job->JobId = FGuid::NewGuid();
    Job->Kind = EPIEJobKind::DriveInput;
    Job->Duration = static_cast<float>(Duration);
    Job->SampleDt = static_cast<float>(SampleDt);
    Job->Pawn = Pawn;
    Job->PC = PC;

    if (bHasWorld)
    {
        Job->DirMode = FPIEJobState::EDirMode::World;
        Job->WorldDir = FVector(
            (*WorldArr)[0]->AsNumber(),
            (*WorldArr)[1]->AsNumber(),
            (*WorldArr)[2]->AsNumber()
        );
        if (Job->WorldDir.IsNearlyZero())
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'direction_world' must be a non-zero vector"));
        }
        Job->WorldDir = Job->WorldDir.GetSafeNormal();
    }
    else
    {
        if (!IsNamedDirectionValid(NamedStr))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
                TEXT("Unknown 'direction_named': '%s'. Valid: Forward/Back/Left/Right/ForwardLeft/ForwardRight/BackLeft/BackRight"),
                *NamedStr));
        }
        Job->DirMode = FPIEJobState::EDirMode::Named;
        Job->NamedDir = FName(*NamedStr);
    }

    double PinYaw = 0.0, PinPitch = 0.0;
    const bool bHasYaw = Params->TryGetNumberField(TEXT("pin_yaw"), PinYaw);
    const bool bHasPitch = Params->TryGetNumberField(TEXT("pin_pitch"), PinPitch);
    if (bHasYaw || bHasPitch)
    {
        Job->bPinRotation = true;
        Job->PinnedRotation = FRotator(
            bHasPitch ? static_cast<float>(PinPitch) : PC->GetControlRotation().Pitch,
            bHasYaw   ? static_cast<float>(PinYaw)   : PC->GetControlRotation().Yaw,
            0.f
        );
        // Apply the pin once before the first tick so callers see immediate effect.
        PC->SetControlRotation(Job->PinnedRotation);
    }

    GPIEJobs.Add(Job->JobId, Job);
    RegisterJobTicker(Job);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("job_id"), Job->JobId.ToString(EGuidFormats::DigitsWithHyphensLower));
    Result->SetStringField(TEXT("status"), TEXT("running"));
    Result->SetNumberField(TEXT("duration_sec"), Job->Duration);
    Result->SetNumberField(TEXT("sample_dt_sec"), Job->SampleDt);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandlePIESimulateKeyStart(const TSharedPtr<FJsonObject>& Params)
{
    APlayerController* PC = nullptr;
    APawn* Pawn = nullptr;
    FString Error;
    if (!ResolvePIEPlayer(PC, Pawn, Error))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    FString ActionName;
    if (!Params->TryGetStringField(TEXT("action_name"), ActionName) || ActionName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'action_name'"));
    }

    double Duration = 0.0;
    if (!Params->TryGetNumberField(TEXT("pressed_for_seconds"), Duration) || Duration <= 0.0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or non-positive 'pressed_for_seconds'"));
    }

    double SampleDt = 0.05;
    Params->TryGetNumberField(TEXT("sample_dt_sec"), SampleDt);
    if (SampleDt <= 0.0) SampleDt = 0.05;

    TArray<FString> Attempts;
    UInputAction* IA = ResolveInputAction(ActionName, Attempts);
    if (!IA)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
            TEXT("Could not resolve InputAction '%s' (tried: %s)"),
            *ActionName, *FString::Join(Attempts, TEXT(", "))));
    }

    if (!Cast<UEnhancedPlayerInput>(PC->PlayerInput))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("PlayerController does not have UEnhancedPlayerInput"));
    }

    TSharedRef<FPIEJobState> Job = MakeShared<FPIEJobState>();
    Job->JobId = FGuid::NewGuid();
    Job->Kind = EPIEJobKind::SimulateKey;
    Job->Duration = static_cast<float>(Duration);
    Job->SampleDt = static_cast<float>(SampleDt);
    Job->Pawn = Pawn;
    Job->PC = PC;
    Job->Action = IA;

    GPIEJobs.Add(Job->JobId, Job);
    RegisterJobTicker(Job);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("job_id"), Job->JobId.ToString(EGuidFormats::DigitsWithHyphensLower));
    Result->SetStringField(TEXT("status"), TEXT("running"));
    Result->SetStringField(TEXT("action_resolved"), IA->GetPathName());
    Result->SetNumberField(TEXT("duration_sec"), Job->Duration);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandlePIEGetJobResult(const TSharedPtr<FJsonObject>& Params)
{
    FString JobIdStr;
    if (!Params->TryGetStringField(TEXT("job_id"), JobIdStr) || JobIdStr.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'job_id'"));
    }
    FGuid JobId;
    if (!FGuid::Parse(JobIdStr, JobId))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid job_id GUID: '%s'"), *JobIdStr));
    }
    TSharedPtr<FPIEJobState>* Found = GPIEJobs.Find(JobId);
    if (!Found || !Found->IsValid())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown or already-collected job_id: '%s'"), *JobIdStr));
    }

    TSharedRef<FJsonObject> Result = JobStateToJson(**Found);
    if ((*Found)->bDone)
    {
        // Final read — drop the buffer so the map doesn't leak orphan jobs.
        GPIEJobs.Remove(JobId);
    }
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandlePIESetControlRotation(const TSharedPtr<FJsonObject>& Params)
{
    APlayerController* PC = nullptr;
    APawn* Pawn = nullptr;
    FString Error;
    if (!ResolvePIEPlayer(PC, Pawn, Error))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    double Yaw = 0.0, Pitch = 0.0;
    const bool bHasYaw = Params->TryGetNumberField(TEXT("yaw"), Yaw);
    const bool bHasPitch = Params->TryGetNumberField(TEXT("pitch"), Pitch);
    if (!bHasYaw && !bHasPitch)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Provide at least one of 'yaw' or 'pitch'"));
    }

    const FRotator Current = PC->GetControlRotation();
    const FRotator Target(
        bHasPitch ? static_cast<float>(Pitch) : Current.Pitch,
        bHasYaw   ? static_cast<float>(Yaw)   : Current.Yaw,
        0.f
    );
    PC->SetControlRotation(Target);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetObjectField(TEXT("control_rotation"), JsonFromRotator(Target));
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandlePIECancelJob(const TSharedPtr<FJsonObject>& Params)
{
    FString JobIdStr;
    if (!Params->TryGetStringField(TEXT("job_id"), JobIdStr) || JobIdStr.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'job_id'"));
    }
    FGuid JobId;
    if (!FGuid::Parse(JobIdStr, JobId))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid job_id GUID: '%s'"), *JobIdStr));
    }
    TSharedPtr<FPIEJobState>* Found = GPIEJobs.Find(JobId);
    if (!Found || !Found->IsValid())
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("reason"), TEXT("Unknown or already-collected job_id"));
        return Result;
    }
    // Mark done; ticker will see this next frame and unregister itself.
    (*Found)->bDone = true;
    if ((*Found)->ErrorMessage.IsEmpty())
    {
        (*Found)->ErrorMessage = TEXT("Cancelled by client");
    }
    TSharedRef<FJsonObject> Result = JobStateToJson(**Found);
    GPIEJobs.Remove(JobId);
    return Result;
}
