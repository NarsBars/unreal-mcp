#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for DataAsset-related MCP commands.
 * Provides tools for creating UDataAsset instances (including AngelScript subclasses),
 * setting complex property types, and reading back property values.
 */
class UNREALMCP_API FUnrealMCPDataAssetCommands
{
public:
	FUnrealMCPDataAssetCommands();

	// Route command to appropriate handler
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// Asset creation (UDataAsset subclasses only)
	TSharedPtr<FJsonObject> HandleCreateDataAsset(const TSharedPtr<FJsonObject>& Params);

	// Generic asset creation (any UObject class — SubsurfaceProfile, CurveFloat, etc.)
	TSharedPtr<FJsonObject> HandleCreateAsset(const TSharedPtr<FJsonObject>& Params);

	// Property modification
	TSharedPtr<FJsonObject> HandleSetDataAssetProperty(const TSharedPtr<FJsonObject>& Params);

	// Property reading
	TSharedPtr<FJsonObject> HandleGetDataAssetProperties(const TSharedPtr<FJsonObject>& Params);

	// Array element operations
	TSharedPtr<FJsonObject> HandleGetArrayElement(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetArrayElement(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddArrayElement(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveArrayElement(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetArrayLength(const TSharedPtr<FJsonObject>& Params);

	// Raw text import for properties that can't be set via JSON (e.g., InstancedStruct arrays)
	TSharedPtr<FJsonObject> HandleImportPropertyText(const TSharedPtr<FJsonObject>& Params);

	// Shared helper: resolve asset + array property from params
	bool ResolveArrayProperty(const TSharedPtr<FJsonObject>& Params, UObject*& OutObject,
		FArrayProperty*& OutArrayProp, void*& OutArrayAddr, FString& OutError);
};
