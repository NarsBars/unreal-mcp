#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Niagara/VFX MCP commands.
 * Provides tools for creating Niagara systems and emitters.
 */
class UNREALMCP_API FUnrealMCPNiagaraCommands
{
public:
	FUnrealMCPNiagaraCommands();

	// Route command to appropriate handler
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	TSharedPtr<FJsonObject> HandleCreateNiagaraSystem(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCreateNiagaraEmitter(const TSharedPtr<FJsonObject>& Params);
};
