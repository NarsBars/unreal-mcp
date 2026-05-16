#include "Commands/UnrealMCPActorQueryCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "UObject/UnrealType.h"

// UE core
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Editor.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Engine/Selection.h"

FUnrealMCPActorQueryCommands::FUnrealMCPActorQueryCommands()
{
}

AActor* FUnrealMCPActorQueryCommands::FindActorByName(const FString& ActorName)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);

	for (AActor* Actor : AllActors)
	{
		if (Actor && (Actor->GetName() == ActorName || Actor->GetActorLabel() == ActorName))
		{
			return Actor;
		}
	}
	return nullptr;
}

TSharedPtr<FJsonObject> FUnrealMCPActorQueryCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("get_actor_transform"))
	{
		return HandleGetActorTransform(Params);
	}
	else if (CommandType == TEXT("get_actor_components"))
	{
		return HandleGetActorComponents(Params);
	}
	else if (CommandType == TEXT("get_bounding_box"))
	{
		return HandleGetBoundingBox(Params);
	}
	else if (CommandType == TEXT("attach_actor"))
	{
		return HandleAttachActor(Params);
	}
	else if (CommandType == TEXT("detach_actor"))
	{
		return HandleDetachActor(Params);
	}
	else if (CommandType == TEXT("set_actor_visibility"))
	{
		return HandleSetActorVisibility(Params);
	}
	else if (CommandType == TEXT("duplicate_actor"))
	{
		return HandleDuplicateActor(Params);
	}
	else if (CommandType == TEXT("add_actor_tag"))
	{
		return HandleAddActorTag(Params);
	}
	else if (CommandType == TEXT("remove_actor_tag"))
	{
		return HandleRemoveActorTag(Params);
	}
	else if (CommandType == TEXT("find_actors_by_tag"))
	{
		return HandleFindActorsByTag(Params);
	}

	return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown actor query command: %s"), *CommandType));
}

// ---------------------------------------------------------------------------
// get_actor_transform
// Params: { "name" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPActorQueryCommands::HandleGetActorTransform(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("name"), ActorName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	FVector Location = Actor->GetActorLocation();
	FRotator Rotation = Actor->GetActorRotation();
	FVector Scale = Actor->GetActorScale3D();

	TSharedPtr<FJsonObject> LocationObj = MakeShared<FJsonObject>();
	LocationObj->SetNumberField(TEXT("x"), Location.X);
	LocationObj->SetNumberField(TEXT("y"), Location.Y);
	LocationObj->SetNumberField(TEXT("z"), Location.Z);

	TSharedPtr<FJsonObject> RotationObj = MakeShared<FJsonObject>();
	RotationObj->SetNumberField(TEXT("pitch"), Rotation.Pitch);
	RotationObj->SetNumberField(TEXT("yaw"), Rotation.Yaw);
	RotationObj->SetNumberField(TEXT("roll"), Rotation.Roll);

	TSharedPtr<FJsonObject> ScaleObj = MakeShared<FJsonObject>();
	ScaleObj->SetNumberField(TEXT("x"), Scale.X);
	ScaleObj->SetNumberField(TEXT("y"), Scale.Y);
	ScaleObj->SetNumberField(TEXT("z"), Scale.Z);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("name"), ActorName);
	Result->SetObjectField(TEXT("location"), LocationObj);
	Result->SetObjectField(TEXT("rotation"), RotationObj);
	Result->SetObjectField(TEXT("scale"), ScaleObj);
	return Result;
}

// ---------------------------------------------------------------------------
// get_actor_components
// Params: { "name", "class_filter" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPActorQueryCommands::HandleGetActorComponents(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("name"), ActorName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	FString ClassFilter;
	Params->TryGetStringField(TEXT("class_filter"), ClassFilter);

	bool bIncludeProperties = false;
	if (Params->HasField(TEXT("include_properties")))
		bIncludeProperties = Params->GetBoolField(TEXT("include_properties"));

	TArray<UActorComponent*> Components;
	Actor->GetComponents(Components);

	TArray<TSharedPtr<FJsonValue>> ComponentsArray;
	for (UActorComponent* Component : Components)
	{
		if (!Component) continue;

		// Apply class filter if specified
		if (!ClassFilter.IsEmpty() && !Component->GetClass()->GetName().Contains(ClassFilter))
		{
			continue;
		}

		TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
		CompObj->SetStringField(TEXT("name"), Component->GetName());
		CompObj->SetStringField(TEXT("class"), Component->GetClass()->GetName());

		// Add transform info for scene components
		if (USceneComponent* SceneComp = Cast<USceneComponent>(Component))
		{
			TSharedPtr<FJsonObject> TransformObj = MakeShared<FJsonObject>();
			FVector RelLoc = SceneComp->GetRelativeLocation();
			TransformObj->SetNumberField(TEXT("x"), RelLoc.X);
			TransformObj->SetNumberField(TEXT("y"), RelLoc.Y);
			TransformObj->SetNumberField(TEXT("z"), RelLoc.Z);
			CompObj->SetObjectField(TEXT("relative_location"), TransformObj);
			CompObj->SetBoolField(TEXT("is_root"), SceneComp == Actor->GetRootComponent());
		}

		// Add full property introspection if requested
		if (bIncludeProperties)
		{
			TSharedPtr<FJsonObject> PropsObj = MakeShared<FJsonObject>();
			for (TFieldIterator<FProperty> PropIt(Component->GetClass()); PropIt; ++PropIt)
			{
				FProperty* Prop = *PropIt;
				if (Prop->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)) continue;

				const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Component);
				TSharedPtr<FJsonValue> Value = FUnrealMCPCommonUtils::GetPropertyAsJson(Prop, ValuePtr);
				if (Value.IsValid())
				{
					PropsObj->SetField(Prop->GetName(), Value);
				}
			}
			CompObj->SetObjectField(TEXT("properties"), PropsObj);
		}

		ComponentsArray.Add(MakeShared<FJsonValueObject>(CompObj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("name"), ActorName);
	Result->SetArrayField(TEXT("components"), ComponentsArray);
	Result->SetNumberField(TEXT("count"), ComponentsArray.Num());
	return Result;
}

// ---------------------------------------------------------------------------
// get_bounding_box
// Params: { "name" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPActorQueryCommands::HandleGetBoundingBox(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("name"), ActorName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	FBox BoundingBox = Actor->GetComponentsBoundingBox();
	FVector Center = BoundingBox.GetCenter();
	FVector Extent = BoundingBox.GetExtent();

	TSharedPtr<FJsonObject> MinObj = MakeShared<FJsonObject>();
	MinObj->SetNumberField(TEXT("x"), BoundingBox.Min.X);
	MinObj->SetNumberField(TEXT("y"), BoundingBox.Min.Y);
	MinObj->SetNumberField(TEXT("z"), BoundingBox.Min.Z);

	TSharedPtr<FJsonObject> MaxObj = MakeShared<FJsonObject>();
	MaxObj->SetNumberField(TEXT("x"), BoundingBox.Max.X);
	MaxObj->SetNumberField(TEXT("y"), BoundingBox.Max.Y);
	MaxObj->SetNumberField(TEXT("z"), BoundingBox.Max.Z);

	TSharedPtr<FJsonObject> CenterObj = MakeShared<FJsonObject>();
	CenterObj->SetNumberField(TEXT("x"), Center.X);
	CenterObj->SetNumberField(TEXT("y"), Center.Y);
	CenterObj->SetNumberField(TEXT("z"), Center.Z);

	TSharedPtr<FJsonObject> ExtentObj = MakeShared<FJsonObject>();
	ExtentObj->SetNumberField(TEXT("x"), Extent.X);
	ExtentObj->SetNumberField(TEXT("y"), Extent.Y);
	ExtentObj->SetNumberField(TEXT("z"), Extent.Z);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("name"), ActorName);
	Result->SetObjectField(TEXT("min"), MinObj);
	Result->SetObjectField(TEXT("max"), MaxObj);
	Result->SetObjectField(TEXT("center"), CenterObj);
	Result->SetObjectField(TEXT("extent"), ExtentObj);
	return Result;
}

// ---------------------------------------------------------------------------
// attach_actor
// Params: { "child_name", "parent_name", "socket_name", "rule" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPActorQueryCommands::HandleAttachActor(const TSharedPtr<FJsonObject>& Params)
{
	FString ChildName;
	if (!Params->TryGetStringField(TEXT("child_name"), ChildName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'child_name' parameter"));
	}

	FString ParentName;
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parent_name' parameter"));
	}

	AActor* ChildActor = FindActorByName(ChildName);
	if (!ChildActor)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Child actor not found: %s"), *ChildName));
	}

	AActor* ParentActor = FindActorByName(ParentName);
	if (!ParentActor)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Parent actor not found: %s"), *ParentName));
	}

	FName SocketName = NAME_None;
	FString SocketNameStr;
	if (Params->TryGetStringField(TEXT("socket_name"), SocketNameStr) && !SocketNameStr.IsEmpty())
	{
		SocketName = FName(*SocketNameStr);
	}

	// Determine attachment rule
	FString RuleStr;
	Params->TryGetStringField(TEXT("rule"), RuleStr);
	FAttachmentTransformRules AttachRules = FAttachmentTransformRules::KeepRelativeTransform;
	if (RuleStr == TEXT("KeepWorld"))
	{
		AttachRules = FAttachmentTransformRules::KeepWorldTransform;
	}
	else if (RuleStr == TEXT("SnapToTarget"))
	{
		AttachRules = FAttachmentTransformRules::SnapToTargetNotIncludingScale;
	}

	ChildActor->AttachToActor(ParentActor, AttachRules, SocketName);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("child"), ChildName);
	Result->SetStringField(TEXT("parent"), ParentName);
	Result->SetStringField(TEXT("socket"), SocketName.ToString());
	return Result;
}

// ---------------------------------------------------------------------------
// detach_actor
// Params: { "name", "rule" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPActorQueryCommands::HandleDetachActor(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("name"), ActorName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	FString RuleStr;
	Params->TryGetStringField(TEXT("rule"), RuleStr);
	FDetachmentTransformRules DetachRules = FDetachmentTransformRules::KeepWorldTransform;
	if (RuleStr == TEXT("KeepRelative"))
	{
		DetachRules = FDetachmentTransformRules::KeepRelativeTransform;
	}

	Actor->DetachFromActor(DetachRules);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("name"), ActorName);
	Result->SetStringField(TEXT("detached"), TEXT("true"));
	return Result;
}

// ---------------------------------------------------------------------------
// set_actor_visibility
// Params: { "name", "visible", "propagate" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPActorQueryCommands::HandleSetActorVisibility(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("name"), ActorName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	if (!Params->HasField(TEXT("visible")))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'visible' parameter"));
	}
	bool bVisible = Params->GetBoolField(TEXT("visible"));

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	bool bPropagate = true;
	if (Params->HasField(TEXT("propagate")))
	{
		bPropagate = Params->GetBoolField(TEXT("propagate"));
	}

	Actor->SetActorHiddenInGame(!bVisible);
	Actor->SetIsTemporarilyHiddenInEditor(!bVisible);

	if (bPropagate && Actor->GetRootComponent())
	{
		Actor->GetRootComponent()->SetVisibility(bVisible, true);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("name"), ActorName);
	Result->SetBoolField(TEXT("visible"), bVisible);
	return Result;
}

// ---------------------------------------------------------------------------
// duplicate_actor
// Params: { "name", "new_name", "offset" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPActorQueryCommands::HandleDuplicateActor(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("name"), ActorName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
	if (!EditorActorSubsystem)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("EditorActorSubsystem not available"));
	}

	// Select the actor, duplicate via subsystem
	GEditor->SelectNone(true, true, false);
	GEditor->SelectActor(Actor, true, true);

	EditorActorSubsystem->DuplicateSelectedActors(Actor->GetWorld());

	// The duplicated actor should now be selected
	USelection* Selection = GEditor->GetSelectedActors();
	AActor* NewActor = nullptr;
	if (Selection && Selection->Num() > 0)
	{
		NewActor = Cast<AActor>(Selection->GetSelectedObject(0));
	}
	if (!NewActor || NewActor == Actor)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to duplicate actor: %s"), *ActorName));
	}

	// Apply optional name
	FString NewName;
	if (Params->TryGetStringField(TEXT("new_name"), NewName) && !NewName.IsEmpty())
	{
		NewActor->SetActorLabel(*NewName);
	}

	// Apply optional offset
	if (Params->HasField(TEXT("offset")))
	{
		FVector Offset = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("offset"));
		NewActor->SetActorLocation(NewActor->GetActorLocation() + Offset);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("original"), ActorName);
	Result->SetStringField(TEXT("duplicate"), NewActor->GetActorLabel());
	Result->SetStringField(TEXT("duplicate_name"), NewActor->GetName());
	return Result;
}

// ---------------------------------------------------------------------------
// add_actor_tag
// Params: { "name", "tag" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPActorQueryCommands::HandleAddActorTag(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("name"), ActorName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	FString Tag;
	if (!Params->TryGetStringField(TEXT("tag"), Tag))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'tag' parameter"));
	}

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	Actor->Tags.AddUnique(FName(*Tag));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("name"), ActorName);
	Result->SetStringField(TEXT("tag"), Tag);
	Result->SetNumberField(TEXT("total_tags"), Actor->Tags.Num());
	return Result;
}

// ---------------------------------------------------------------------------
// remove_actor_tag
// Params: { "name", "tag" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPActorQueryCommands::HandleRemoveActorTag(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("name"), ActorName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	FString Tag;
	if (!Params->TryGetStringField(TEXT("tag"), Tag))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'tag' parameter"));
	}

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	Actor->Tags.Remove(FName(*Tag));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("name"), ActorName);
	Result->SetStringField(TEXT("removed_tag"), Tag);
	Result->SetNumberField(TEXT("total_tags"), Actor->Tags.Num());
	return Result;
}

// ---------------------------------------------------------------------------
// find_actors_by_tag
// Params: { "tag" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPActorQueryCommands::HandleFindActorsByTag(const TSharedPtr<FJsonObject>& Params)
{
	FString Tag;
	if (!Params->TryGetStringField(TEXT("tag"), Tag))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'tag' parameter"));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available"));
	}

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(World, FName(*Tag), FoundActors);

	TArray<TSharedPtr<FJsonValue>> ActorsArray;
	for (AActor* Actor : FoundActors)
	{
		if (!Actor) continue;
		ActorsArray.Add(FUnrealMCPCommonUtils::ActorToJson(Actor));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("tag"), Tag);
	Result->SetArrayField(TEXT("actors"), ActorsArray);
	Result->SetNumberField(TEXT("count"), ActorsArray.Num());
	return Result;
}
