#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Actor query MCP commands.
 * Provides tools for querying actor transforms, components, bounds,
 * and managing actor attachment, visibility, duplication, and tags.
 */
class UNREALMCP_API FUnrealMCPActorQueryCommands
{
public:
	FUnrealMCPActorQueryCommands();

	// Route command to appropriate handler
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// Helper to find actor by name in the editor world
	AActor* FindActorByName(const FString& ActorName);

	// Actor queries
	TSharedPtr<FJsonObject> HandleGetActorTransform(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetActorComponents(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetBoundingBox(const TSharedPtr<FJsonObject>& Params);

	// Actor operations
	TSharedPtr<FJsonObject> HandleAttachActor(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDetachActor(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetActorVisibility(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDuplicateActor(const TSharedPtr<FJsonObject>& Params);

	// Tag operations
	TSharedPtr<FJsonObject> HandleAddActorTag(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveActorTag(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleFindActorsByTag(const TSharedPtr<FJsonObject>& Params);
};
