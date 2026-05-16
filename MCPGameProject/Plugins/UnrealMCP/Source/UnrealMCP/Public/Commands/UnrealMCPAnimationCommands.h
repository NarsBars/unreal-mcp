#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Animation and Level Sequence MCP commands.
 * Provides tools for creating blend spaces, anim blueprints,
 * adding anim notifies, playing animations, and managing level sequences.
 */
class UNREALMCP_API FUnrealMCPAnimationCommands
{
public:
	FUnrealMCPAnimationCommands();

	// Route command to appropriate handler
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// Animation
	TSharedPtr<FJsonObject> HandleCreateBlendSpace(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddAnimNotify(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandlePlayAnimation(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCreateAnimBlueprint(const TSharedPtr<FJsonObject>& Params);

	// Level Sequences
	TSharedPtr<FJsonObject> HandleCreateSequence(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddActorToSequence(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandlePlaySequence(const TSharedPtr<FJsonObject>& Params);

	// Skeletal Mesh Sockets
	TSharedPtr<FJsonObject> HandleAddSkeletalMeshSocket(const TSharedPtr<FJsonObject>& Params);

	// Anim Notify States (duration-based)
	TSharedPtr<FJsonObject> HandleAddAnimNotifyState(const TSharedPtr<FJsonObject>& Params);

	// Helpers
	AActor* FindActorByName(const FString& Name);
};
