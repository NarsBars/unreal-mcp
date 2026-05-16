#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Enhanced Input-related MCP commands.
 * Provides tools for creating InputAction and InputMappingContext assets,
 * adding key mappings to contexts, and querying input asset info.
 */
class UNREALMCP_API FUnrealMCPInputCommands
{
public:
	FUnrealMCPInputCommands();

	// Route command to appropriate handler
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// Asset creation
	TSharedPtr<FJsonObject> HandleCreateInputAction(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCreateInputMappingContext(const TSharedPtr<FJsonObject>& Params);

	// Asset modification
	TSharedPtr<FJsonObject> HandleAddInputMapping(const TSharedPtr<FJsonObject>& Params);

	// Asset queries
	TSharedPtr<FJsonObject> HandleGetInputInfo(const TSharedPtr<FJsonObject>& Params);
};
