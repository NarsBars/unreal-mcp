#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for World/Environment MCP commands.
 * Provides tools for foliage management and landscape queries.
 */
class UNREALMCP_API FUnrealMCPWorldCommands
{
public:
	FUnrealMCPWorldCommands();

	// Route command to appropriate handler
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// Foliage
	TSharedPtr<FJsonObject> HandleAddFoliageType(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandlePaintFoliage(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListFoliageTypes(const TSharedPtr<FJsonObject>& Params);

	// Landscape
	TSharedPtr<FJsonObject> HandleGetLandscapeInfo(const TSharedPtr<FJsonObject>& Params);
};
