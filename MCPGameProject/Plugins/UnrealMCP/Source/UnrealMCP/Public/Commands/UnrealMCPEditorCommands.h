#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Editor-related MCP commands
 * Handles viewport control, actor manipulation, and level management
 */
class UNREALMCP_API FUnrealMCPEditorCommands
{
public:
    FUnrealMCPEditorCommands();

    // Handle editor commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Actor manipulation commands
    TSharedPtr<FJsonObject> HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSpawnActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetActorProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetActorProperty(const TSharedPtr<FJsonObject>& Params);

    // Blueprint actor spawning
    TSharedPtr<FJsonObject> HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params);

    // Editor viewport commands
    TSharedPtr<FJsonObject> HandleFocusViewport(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params);

    // PIE control commands
    TSharedPtr<FJsonObject> HandleStartPIE(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleStopPIE(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetPIEState(const TSharedPtr<FJsonObject>& Params);

    // PIE input driving (start-and-poll model — see Plugins/UnrealMCP/Python/README.md)
    TSharedPtr<FJsonObject> HandlePIEDriveInputStart(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandlePIESimulateKeyStart(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandlePIEGetJobResult(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandlePIESetControlRotation(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandlePIECancelJob(const TSharedPtr<FJsonObject>& Params);

    // Console command execution
    TSharedPtr<FJsonObject> HandleExecuteConsoleCommand(const TSharedPtr<FJsonObject>& Params);

    // Log query
    TSharedPtr<FJsonObject> HandleGetEditorLog(const TSharedPtr<FJsonObject>& Params);

    // Python scripting
    TSharedPtr<FJsonObject> HandleExecutePython(const TSharedPtr<FJsonObject>& Params);

    // Editor lifecycle
    TSharedPtr<FJsonObject> HandleCloseEditor(const TSharedPtr<FJsonObject>& Params);
};