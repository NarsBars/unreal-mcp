#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Asset management MCP commands.
 * Provides tools for searching, importing, duplicating, renaming,
 * moving, deleting assets, and querying dependencies.
 */
class UNREALMCP_API FUnrealMCPAssetCommands
{
public:
	FUnrealMCPAssetCommands();

	// Route command to appropriate handler
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// Asset search/query
	TSharedPtr<FJsonObject> HandleSearchAssets(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetAssetDependencies(const TSharedPtr<FJsonObject>& Params);

	// Asset operations
	TSharedPtr<FJsonObject> HandleImportAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDuplicateAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRenameAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleMoveAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSaveAsset(const TSharedPtr<FJsonObject>& Params);
};
