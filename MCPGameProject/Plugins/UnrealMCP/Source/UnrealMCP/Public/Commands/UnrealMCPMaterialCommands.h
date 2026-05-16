#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Material-related MCP commands.
 * Provides tools for creating and editing materials, material instances,
 * material parameter collections, and material expressions programmatically.
 */
class UNREALMCP_API FUnrealMCPMaterialCommands
{
public:
    FUnrealMCPMaterialCommands();

    // Route command to appropriate handler
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Asset creation
    TSharedPtr<FJsonObject> HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateMaterialParameterCollection(const TSharedPtr<FJsonObject>& Params);

    // Material graph editing
    TSharedPtr<FJsonObject> HandleAddMaterialExpression(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleConnectMaterialExpressions(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleConnectMaterialToProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetMaterialProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRecompileMaterial(const TSharedPtr<FJsonObject>& Params);

    // Material graph queries
    TSharedPtr<FJsonObject> HandleGetMaterialInfo(const TSharedPtr<FJsonObject>& Params);

    // Custom HLSL expression code
    TSharedPtr<FJsonObject> HandleGetCustomExpressionCode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetCustomExpressionCode(const TSharedPtr<FJsonObject>& Params);

    // Expression management
    TSharedPtr<FJsonObject> HandleGetExpressionProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetExpressionProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDisconnectExpression(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRemoveExpression(const TSharedPtr<FJsonObject>& Params);

    // Material instance parameter editing
    TSharedPtr<FJsonObject> HandleSetMaterialInstanceScalarParameter(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetMaterialInstanceVectorParameter(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetMaterialInstanceTextureParameter(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetMaterialInstanceStaticSwitchParameter(const TSharedPtr<FJsonObject>& Params);

    // Helpers
    class UMaterial* LoadMaterialFromPath(const FString& MaterialPath, FString& OutError);
    class UMaterialInstanceConstant* LoadMaterialInstanceFromPath(const FString& InstancePath, FString& OutError);

    // Graph pseudocode generation
    FString BuildGraphPseudocode(class UMaterial* Material) const;

    struct FGraphFormatContext
    {
        TConstArrayView<TObjectPtr<class UMaterialExpression>> Expressions;
        TMap<int32, FString> VarNames;
        TMap<int32, int32> RefCounts;
        TSet<int32> Emitted;
        FString VarSection;
    };

    FString FormatExpression(int32 Index, int32 OutputIndex, FGraphFormatContext& Ctx) const;
    static FString FormatOutputSuffix(int32 OutputIndex, class UMaterialExpression* Expr);
    static FString FormatMaskChannels(class UMaterialExpression* Expr);
    static FString FormatConstantValue(class UMaterialExpression* Expr);
    static FString SanitizeVarName(const FString& Name);
};
