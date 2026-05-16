#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for ChooserTable-related MCP commands.
 * Provides tools for reading ChooserTable structure/data and modifying column cell values.
 */
class UNREALMCP_API FUnrealMCPChooserCommands
{
public:
	FUnrealMCPChooserCommands();

	// Route command to appropriate handler
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// Read the full structure and data of a ChooserTable (columns, rows, nested tables)
	TSharedPtr<FJsonObject> HandleReadChooserTable(const TSharedPtr<FJsonObject>& Params);

	// Set a single cell value in a ChooserTable column
	TSharedPtr<FJsonObject> HandleSetChooserColumnValue(const TSharedPtr<FJsonObject>& Params);
};
