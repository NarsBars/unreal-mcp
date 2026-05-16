#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Level management MCP commands.
 * Provides tools for opening, saving, listing, and creating levels.
 */
class UNREALMCP_API FUnrealMCPLevelCommands
{
public:
	FUnrealMCPLevelCommands();

	// Route command to appropriate handler
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	TSharedPtr<FJsonObject> HandleOpenLevel(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSaveLevel(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListLevels(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCreateLevel(const TSharedPtr<FJsonObject>& Params);
};
