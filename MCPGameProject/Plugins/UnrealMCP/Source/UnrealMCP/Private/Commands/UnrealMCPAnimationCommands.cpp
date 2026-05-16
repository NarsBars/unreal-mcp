#include "Commands/UnrealMCPAnimationCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"

// UE core
#include "EditorAssetLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "EngineUtils.h"

// Animation
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimTypes.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/SkeletalMesh.h"

// Animation Factories
#include "Factories/BlendSpaceFactoryNew.h"
#include "Factories/AnimBlueprintFactory.h"

// Level Sequence
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "MovieSceneSequencePlayer.h"

FUnrealMCPAnimationCommands::FUnrealMCPAnimationCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPAnimationCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("create_blend_space"))
	{
		return HandleCreateBlendSpace(Params);
	}
	else if (CommandType == TEXT("add_anim_notify"))
	{
		return HandleAddAnimNotify(Params);
	}
	else if (CommandType == TEXT("play_animation"))
	{
		return HandlePlayAnimation(Params);
	}
	else if (CommandType == TEXT("create_anim_blueprint"))
	{
		return HandleCreateAnimBlueprint(Params);
	}
	else if (CommandType == TEXT("create_sequence"))
	{
		return HandleCreateSequence(Params);
	}
	else if (CommandType == TEXT("add_actor_to_sequence"))
	{
		return HandleAddActorToSequence(Params);
	}
	else if (CommandType == TEXT("play_sequence"))
	{
		return HandlePlaySequence(Params);
	}
	else if (CommandType == TEXT("add_anim_notify_state"))
	{
		return HandleAddAnimNotifyState(Params);
	}
	else if (CommandType == TEXT("add_skeletal_mesh_socket"))
	{
		return HandleAddSkeletalMeshSocket(Params);
	}

	return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown animation command: %s"), *CommandType));
}

AActor* FUnrealMCPAnimationCommands::FindActorByName(const FString& Name)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return nullptr;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && (Actor->GetName() == Name || Actor->GetActorLabel() == Name))
		{
			return Actor;
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// create_blend_space
// Params: { "name", "skeleton_path", "path", "save" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPAnimationCommands::HandleCreateBlendSpace(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	FString SkeletonPath;
	if (!Params->TryGetStringField(TEXT("skeleton_path"), SkeletonPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'skeleton_path' parameter"));
	}

	FString Path = TEXT("/Game/Animations");
	Params->TryGetStringField(TEXT("path"), Path);

	// Load skeleton
	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skeleton)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	// Create via factory
	UBlendSpaceFactoryNew* Factory = NewObject<UBlendSpaceFactoryNew>();
	Factory->TargetSkeleton = Skeleton;

	IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(Name, Path, UBlendSpace::StaticClass(), Factory);

	if (!NewAsset)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to create BlendSpace: %s/%s"), *Path, *Name));
	}

	FString FullPath = Path / Name;
	FAssetRegistryModule::AssetCreated(NewAsset);
	NewAsset->GetOutermost()->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(FullPath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("name"), Name);
	Result->SetStringField(TEXT("path"), FullPath);
	Result->SetStringField(TEXT("type"), TEXT("BlendSpace"));
	Result->SetStringField(TEXT("skeleton"), SkeletonPath);
	return Result;
}

// ---------------------------------------------------------------------------
// add_anim_notify
// Params: { "animation_path", "notify_name", "time", "notify_class", "save" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPAnimationCommands::HandleAddAnimNotify(const TSharedPtr<FJsonObject>& Params)
{
	FString AnimationPath;
	if (!Params->TryGetStringField(TEXT("animation_path"), AnimationPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'animation_path' parameter"));
	}

	FString NotifyName;
	if (!Params->TryGetStringField(TEXT("notify_name"), NotifyName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'notify_name' parameter"));
	}

	double Time = 0.0;
	if (!Params->TryGetNumberField(TEXT("time"), Time))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'time' parameter"));
	}

	// Load animation
	UAnimSequenceBase* AnimSequence = LoadObject<UAnimSequenceBase>(nullptr, *AnimationPath);
	if (!AnimSequence)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Could not load animation: %s"), *AnimationPath));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	// Optionally load a specific notify class
	UClass* NotifyClass = UAnimNotify::StaticClass();
	FString NotifyClassName;
	if (Params->TryGetStringField(TEXT("notify_class"), NotifyClassName) && !NotifyClassName.IsEmpty())
	{
		UClass* FoundClass = FindObject<UClass>(nullptr, *NotifyClassName);
		if (!FoundClass)
		{
			FoundClass = LoadObject<UClass>(nullptr, *NotifyClassName);
		}
		if (FoundClass && FoundClass->IsChildOf(UAnimNotify::StaticClass()))
		{
			NotifyClass = FoundClass;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Could not find notify class '%s', using default UAnimNotify"), *NotifyClassName);
		}
	}

	// Create the notify
	UAnimNotify* NewNotify = NewObject<UAnimNotify>(AnimSequence, NotifyClass, FName(*NotifyName));
	if (!NewNotify)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create AnimNotify instance"));
	}

	// Add to the animation's notify array
	FAnimNotifyEvent& NewEvent = AnimSequence->Notifies.AddDefaulted_GetRef();
	NewEvent.NotifyName = FName(*NotifyName);
	NewEvent.Notify = NewNotify;
	NewEvent.SetTime(static_cast<float>(Time));
	NewEvent.TriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::OffsetBefore);

	AnimSequence->GetOutermost()->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(AnimationPath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("animation_path"), AnimationPath);
	Result->SetStringField(TEXT("notify_name"), NotifyName);
	Result->SetNumberField(TEXT("time"), Time);
	Result->SetStringField(TEXT("notify_class"), NotifyClass->GetName());
	return Result;
}

// ---------------------------------------------------------------------------
// play_animation
// Params: { "actor_name", "animation_path", "loop" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPAnimationCommands::HandlePlayAnimation(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
	}

	FString AnimationPath;
	if (!Params->TryGetStringField(TEXT("animation_path"), AnimationPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'animation_path' parameter"));
	}

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	UAnimationAsset* AnimAsset = LoadObject<UAnimationAsset>(nullptr, *AnimationPath);
	if (!AnimAsset)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Could not load animation: %s"), *AnimationPath));
	}

	bool bLoop = false;
	if (Params->HasField(TEXT("loop")))
	{
		bLoop = Params->GetBoolField(TEXT("loop"));
	}

	// Find skeletal mesh component on the actor
	USkeletalMeshComponent* SkelMesh = Actor->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkelMesh)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' has no SkeletalMeshComponent"), *ActorName));
	}

	SkelMesh->PlayAnimation(AnimAsset, bLoop);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetStringField(TEXT("animation_path"), AnimationPath);
	Result->SetBoolField(TEXT("looping"), bLoop);
	return Result;
}

// ---------------------------------------------------------------------------
// create_anim_blueprint
// Params: { "name", "skeleton_path", "path", "parent_class", "save" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPAnimationCommands::HandleCreateAnimBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	FString SkeletonPath;
	if (!Params->TryGetStringField(TEXT("skeleton_path"), SkeletonPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'skeleton_path' parameter"));
	}

	FString Path = TEXT("/Game/Animations");
	Params->TryGetStringField(TEXT("path"), Path);

	// Load skeleton
	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skeleton)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	// Create via factory
	UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
	Factory->TargetSkeleton = Skeleton;
	Factory->ParentClass = UAnimInstance::StaticClass();

	// Optional parent class override
	FString ParentClassName;
	if (Params->TryGetStringField(TEXT("parent_class"), ParentClassName) && !ParentClassName.IsEmpty())
	{
		UClass* ParentClass = FindObject<UClass>(nullptr, *ParentClassName);
		if (!ParentClass)
		{
			ParentClass = LoadObject<UClass>(nullptr, *ParentClassName);
		}
		if (ParentClass && ParentClass->IsChildOf(UAnimInstance::StaticClass()))
		{
			Factory->ParentClass = ParentClass;
		}
	}

	IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(Name, Path, UAnimBlueprint::StaticClass(), Factory);

	if (!NewAsset)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to create AnimBlueprint: %s/%s"), *Path, *Name));
	}

	FString FullPath = Path / Name;
	FAssetRegistryModule::AssetCreated(NewAsset);
	NewAsset->GetOutermost()->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(FullPath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("name"), Name);
	Result->SetStringField(TEXT("path"), FullPath);
	Result->SetStringField(TEXT("type"), TEXT("AnimBlueprint"));
	Result->SetStringField(TEXT("skeleton"), SkeletonPath);
	return Result;
}

// ---------------------------------------------------------------------------
// create_sequence
// Params: { "name", "path", "save" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPAnimationCommands::HandleCreateSequence(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	FString Path = TEXT("/Game/Sequences");
	Params->TryGetStringField(TEXT("path"), Path);

	FString FullPath = Path / Name;
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Sequence already exists: %s"), *FullPath));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(Name, Path, ULevelSequence::StaticClass(), nullptr);

	ULevelSequence* Sequence = Cast<ULevelSequence>(NewAsset);
	if (!Sequence)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to create LevelSequence: %s"), *FullPath));
	}

	// Initialize the MovieScene
	Sequence->Initialize();

	FAssetRegistryModule::AssetCreated(Sequence);
	Sequence->GetOutermost()->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(FullPath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("name"), Name);
	Result->SetStringField(TEXT("path"), FullPath);
	Result->SetStringField(TEXT("type"), TEXT("LevelSequence"));
	return Result;
}

// ---------------------------------------------------------------------------
// add_actor_to_sequence
// Params: { "sequence_path", "actor_name" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPAnimationCommands::HandleAddActorToSequence(const TSharedPtr<FJsonObject>& Params)
{
	FString SequencePath;
	if (!Params->TryGetStringField(TEXT("sequence_path"), SequencePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'sequence_path' parameter"));
	}

	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
	}

	// Load sequence
	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
	if (!Sequence)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Could not load sequence: %s"), *SequencePath));
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Sequence has no MovieScene"));
	}

	// Find actor
	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	// Add as possessable
	FGuid PossessableGuid = MovieScene->AddPossessable(Actor->GetActorLabel(), Actor->GetClass());

	// Bind the possessable to the actor
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (World)
	{
		Sequence->BindPossessableObject(PossessableGuid, *Actor, World);
	}

	Sequence->GetOutermost()->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(SequencePath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("sequence_path"), SequencePath);
	Result->SetStringField(TEXT("actor_name"), ActorName);
	Result->SetStringField(TEXT("possessable_guid"), PossessableGuid.ToString());
	return Result;
}

// ---------------------------------------------------------------------------
// play_sequence
// Params: { "sequence_path", "start_time", "loop" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPAnimationCommands::HandlePlaySequence(const TSharedPtr<FJsonObject>& Params)
{
	FString SequencePath;
	if (!Params->TryGetStringField(TEXT("sequence_path"), SequencePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'sequence_path' parameter"));
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
	if (!Sequence)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Could not load sequence: %s"), *SequencePath));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available"));
	}

	// Configure playback settings
	FMovieSceneSequencePlaybackSettings PlaybackSettings;

	bool bLoop = false;
	if (Params->HasField(TEXT("loop")))
	{
		bLoop = Params->GetBoolField(TEXT("loop"));
	}
	if (bLoop)
	{
		PlaybackSettings.LoopCount.Value = -1; // Infinite loop
	}

	// Create player
	ALevelSequenceActor* SequenceActor = nullptr;
	ULevelSequencePlayer* Player = ULevelSequencePlayer::CreateLevelSequencePlayer(
		World, Sequence, PlaybackSettings, SequenceActor);

	if (!Player)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create LevelSequencePlayer"));
	}

	// Optional start time
	double StartTime = 0.0;
	if (Params->TryGetNumberField(TEXT("start_time"), StartTime) && StartTime > 0.0)
	{
		Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(
			static_cast<float>(StartTime), EUpdatePositionMethod::Jump));
	}

	Player->Play();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("sequence_path"), SequencePath);
	Result->SetBoolField(TEXT("playing"), true);
	Result->SetBoolField(TEXT("looping"), bLoop);
	if (SequenceActor)
	{
		Result->SetStringField(TEXT("sequence_actor"), SequenceActor->GetName());
	}
	return Result;
}

// ---------------------------------------------------------------------------
// add_anim_notify_state
// Params: { "animation_path", "notify_name", "start_time", "end_time"|"duration", "notify_class", "save" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPAnimationCommands::HandleAddAnimNotifyState(const TSharedPtr<FJsonObject>& Params)
{
	FString AnimationPath;
	if (!Params->TryGetStringField(TEXT("animation_path"), AnimationPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'animation_path' parameter"));
	}

	FString NotifyName;
	if (!Params->TryGetStringField(TEXT("notify_name"), NotifyName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'notify_name' parameter"));
	}

	double StartTime = 0.0;
	if (!Params->TryGetNumberField(TEXT("start_time"), StartTime))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'start_time' parameter"));
	}

	// Compute duration from either end_time or duration
	double EndTime = 0.0;
	double DurationVal = 0.0;
	float Duration = 0.0f;

	if (Params->TryGetNumberField(TEXT("end_time"), EndTime))
	{
		Duration = static_cast<float>(EndTime - StartTime);
	}
	else if (Params->TryGetNumberField(TEXT("duration"), DurationVal))
	{
		Duration = static_cast<float>(DurationVal);
		EndTime = StartTime + DurationVal;
	}
	else
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Must provide either 'end_time' or 'duration'"));
	}

	if (Duration <= 0.0f)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Duration must be positive (got %.3f). Check that end_time > start_time."), Duration));
	}

	// Load animation
	UAnimSequenceBase* AnimSequence = LoadObject<UAnimSequenceBase>(nullptr, *AnimationPath);
	if (!AnimSequence)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Could not load animation: %s"), *AnimationPath));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	// Resolve notify state class
	UClass* NotifyStateClass = UAnimNotifyState::StaticClass();
	FString NotifyClassName;
	if (Params->TryGetStringField(TEXT("notify_class"), NotifyClassName) && !NotifyClassName.IsEmpty())
	{
		UClass* FoundClass = FindObject<UClass>(nullptr, *NotifyClassName);
		if (!FoundClass)
		{
			FoundClass = LoadObject<UClass>(nullptr, *NotifyClassName);
		}
		if (FoundClass && FoundClass->IsChildOf(UAnimNotifyState::StaticClass()))
		{
			NotifyStateClass = FoundClass;
		}
		else
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("'%s' is not a valid UAnimNotifyState subclass"), *NotifyClassName));
		}
	}

	// Create the notify state instance
	UAnimNotifyState* NewNotifyState = NewObject<UAnimNotifyState>(AnimSequence, NotifyStateClass, FName(*NotifyName));
	if (!NewNotifyState)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create AnimNotifyState instance"));
	}

	// Add to the animation's notify array
	FAnimNotifyEvent& NewEvent = AnimSequence->Notifies.AddDefaulted_GetRef();
	NewEvent.NotifyName = FName(*NotifyName);
	NewEvent.NotifyStateClass = NewNotifyState;
	NewEvent.SetTime(static_cast<float>(StartTime));
	NewEvent.SetDuration(Duration);
	NewEvent.TriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::OffsetBefore);
	NewEvent.EndTriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::OffsetBefore);

	AnimSequence->GetOutermost()->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(AnimationPath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("animation_path"), AnimationPath);
	Result->SetStringField(TEXT("notify_name"), NotifyName);
	Result->SetStringField(TEXT("notify_class"), NotifyStateClass->GetName());
	Result->SetNumberField(TEXT("start_time"), StartTime);
	Result->SetNumberField(TEXT("end_time"), EndTime);
	Result->SetNumberField(TEXT("duration"), Duration);
	return Result;
}

// ---------------------------------------------------------------------------
// add_skeletal_mesh_socket
// Params: { "skeleton_path"|"skeletal_mesh_path", "socket_name", "bone_name",
//           "relative_location", "relative_rotation", "relative_scale", "save" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPAnimationCommands::HandleAddSkeletalMeshSocket(const TSharedPtr<FJsonObject>& Params)
{
	// Resolve skeleton — either directly or via skeletal mesh
	FString SkeletonPath;
	FString MeshPath;
	USkeleton* Skeleton = nullptr;

	if (Params->TryGetStringField(TEXT("skeleton_path"), SkeletonPath) && !SkeletonPath.IsEmpty())
	{
		Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
		if (!Skeleton)
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath));
		}
	}
	else if (Params->TryGetStringField(TEXT("skeletal_mesh_path"), MeshPath) && !MeshPath.IsEmpty())
	{
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
		if (!Mesh)
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Could not load skeletal mesh: %s"), *MeshPath));
		}
		Skeleton = Mesh->GetSkeleton();
		if (!Skeleton)
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Skeletal mesh '%s' has no skeleton assigned"), *MeshPath));
		}
		SkeletonPath = Skeleton->GetPathName();
	}
	else
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Must provide either 'skeleton_path' or 'skeletal_mesh_path'"));
	}

	FString SocketName;
	if (!Params->TryGetStringField(TEXT("socket_name"), SocketName) || SocketName.IsEmpty())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'socket_name' parameter"));
	}

	FString BoneName;
	if (!Params->TryGetStringField(TEXT("bone_name"), BoneName) || BoneName.IsEmpty())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'bone_name' parameter"));
	}

	// Validate bone exists
	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	int32 BoneIndex = RefSkeleton.FindBoneIndex(FName(*BoneName));
	if (BoneIndex == INDEX_NONE)
	{
		// Build list of valid bone names for the error message
		FString ValidBones;
		for (int32 i = 0; i < RefSkeleton.GetNum(); ++i)
		{
			if (i > 0) ValidBones += TEXT(", ");
			ValidBones += RefSkeleton.GetBoneName(i).ToString();
		}
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Bone '%s' not found in skeleton '%s'. Valid bones: [%s]"), *BoneName, *SkeletonPath, *ValidBones));
	}

	// Check for duplicate socket
	if (Skeleton->FindSocket(FName(*SocketName)))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Socket '%s' already exists on skeleton '%s'"), *SocketName, *SkeletonPath));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	// Create socket
	USkeletalMeshSocket* Socket = NewObject<USkeletalMeshSocket>(Skeleton);
	Socket->SocketName = FName(*SocketName);
	Socket->BoneName = FName(*BoneName);

	// Optional transforms
	if (Params->HasField(TEXT("relative_location")))
	{
		Socket->RelativeLocation = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("relative_location"));
	}
	if (Params->HasField(TEXT("relative_rotation")))
	{
		Socket->RelativeRotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("relative_rotation"));
	}
	if (Params->HasField(TEXT("relative_scale")))
	{
		Socket->RelativeScale = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("relative_scale"));
	}

	Skeleton->Sockets.Add(Socket);
	Skeleton->GetOutermost()->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		// Save using the skeleton's package path
		FString SavePath = Skeleton->GetOutermost()->GetName();
		UEditorAssetLibrary::SaveAsset(SavePath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("skeleton_path"), SkeletonPath);
	Result->SetStringField(TEXT("socket_name"), SocketName);
	Result->SetStringField(TEXT("bone_name"), BoneName);

	TSharedPtr<FJsonObject> LocationJson = MakeShared<FJsonObject>();
	LocationJson->SetNumberField(TEXT("X"), Socket->RelativeLocation.X);
	LocationJson->SetNumberField(TEXT("Y"), Socket->RelativeLocation.Y);
	LocationJson->SetNumberField(TEXT("Z"), Socket->RelativeLocation.Z);
	Result->SetObjectField(TEXT("relative_location"), LocationJson);

	TSharedPtr<FJsonObject> RotationJson = MakeShared<FJsonObject>();
	RotationJson->SetNumberField(TEXT("Pitch"), Socket->RelativeRotation.Pitch);
	RotationJson->SetNumberField(TEXT("Yaw"), Socket->RelativeRotation.Yaw);
	RotationJson->SetNumberField(TEXT("Roll"), Socket->RelativeRotation.Roll);
	Result->SetObjectField(TEXT("relative_rotation"), RotationJson);

	TSharedPtr<FJsonObject> ScaleJson = MakeShared<FJsonObject>();
	ScaleJson->SetNumberField(TEXT("X"), Socket->RelativeScale.X);
	ScaleJson->SetNumberField(TEXT("Y"), Socket->RelativeScale.Y);
	ScaleJson->SetNumberField(TEXT("Z"), Socket->RelativeScale.Z);
	Result->SetObjectField(TEXT("relative_scale"), ScaleJson);

	return Result;
}
