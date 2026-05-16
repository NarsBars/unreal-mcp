#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Audio-related MCP commands.
 * Provides tools for creating SoundClass and SoundMix assets,
 * setting up SoundClass hierarchies, and querying audio asset info.
 */
class UNREALMCP_API FUnrealMCPAudioCommands
{
public:
	FUnrealMCPAudioCommands();

	// Route command to appropriate handler
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// Asset creation
	TSharedPtr<FJsonObject> HandleCreateSoundClass(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCreateSoundMix(const TSharedPtr<FJsonObject>& Params);

	// Asset modification
	TSharedPtr<FJsonObject> HandleSetSoundClassParent(const TSharedPtr<FJsonObject>& Params);

	// Asset queries
	TSharedPtr<FJsonObject> HandleGetAudioInfo(const TSharedPtr<FJsonObject>& Params);
};
