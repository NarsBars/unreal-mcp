#include "Commands/UnrealMCPMaterialCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"

// UE core
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/UObjectGlobals.h"

// Material system
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialParameterCollection.h"
#include "MaterialEditingLibrary.h"

// Material expression types (for parameter name extraction & graph pseudocode)
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"

// Textures
#include "Engine/Texture.h"

// Static parameters (for static switch overrides on material instances)
#include "StaticParameterSet.h"

// Factories
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Factories/MaterialParameterCollectionFactoryNew.h"

FUnrealMCPMaterialCommands::FUnrealMCPMaterialCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("create_material"))
	{
		return HandleCreateMaterial(Params);
	}
	else if (CommandType == TEXT("create_material_instance"))
	{
		return HandleCreateMaterialInstance(Params);
	}
	else if (CommandType == TEXT("create_material_parameter_collection"))
	{
		return HandleCreateMaterialParameterCollection(Params);
	}
	else if (CommandType == TEXT("add_material_expression"))
	{
		return HandleAddMaterialExpression(Params);
	}
	else if (CommandType == TEXT("connect_material_expressions"))
	{
		return HandleConnectMaterialExpressions(Params);
	}
	else if (CommandType == TEXT("connect_material_to_property"))
	{
		return HandleConnectMaterialToProperty(Params);
	}
	else if (CommandType == TEXT("set_material_property"))
	{
		return HandleSetMaterialProperty(Params);
	}
	else if (CommandType == TEXT("recompile_material"))
	{
		return HandleRecompileMaterial(Params);
	}
	else if (CommandType == TEXT("set_material_instance_scalar_parameter"))
	{
		return HandleSetMaterialInstanceScalarParameter(Params);
	}
	else if (CommandType == TEXT("set_material_instance_vector_parameter"))
	{
		return HandleSetMaterialInstanceVectorParameter(Params);
	}
	else if (CommandType == TEXT("set_material_instance_texture_parameter"))
	{
		return HandleSetMaterialInstanceTextureParameter(Params);
	}
	else if (CommandType == TEXT("set_material_instance_static_switch_parameter"))
	{
		return HandleSetMaterialInstanceStaticSwitchParameter(Params);
	}
	else if (CommandType == TEXT("get_material_info"))
	{
		return HandleGetMaterialInfo(Params);
	}
	else if (CommandType == TEXT("get_custom_expression_code"))
	{
		return HandleGetCustomExpressionCode(Params);
	}
	else if (CommandType == TEXT("set_custom_expression_code"))
	{
		return HandleSetCustomExpressionCode(Params);
	}
	else if (CommandType == TEXT("get_expression_properties"))
	{
		return HandleGetExpressionProperties(Params);
	}
	else if (CommandType == TEXT("set_expression_property"))
	{
		return HandleSetExpressionProperty(Params);
	}
	else if (CommandType == TEXT("disconnect_expression"))
	{
		return HandleDisconnectExpression(Params);
	}
	else if (CommandType == TEXT("remove_expression"))
	{
		return HandleRemoveExpression(Params);
	}

	return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown material command: %s"), *CommandType));
}

// ---------------------------------------------------------------------------
// Helper: Load a UMaterial from a content path
// ---------------------------------------------------------------------------
UMaterial* FUnrealMCPMaterialCommands::LoadMaterialFromPath(const FString& MaterialPath, FString& OutError)
{
	// Fast path: try FindObject first (instant for already-loaded assets, no I/O)
	UObject* LoadedObj = FindObject<UMaterial>(nullptr, *MaterialPath);
	if (!LoadedObj)
	{
		// Slow path: full synchronous load with dependency resolution
		UE_LOG(LogTemp, Display, TEXT("MCP: Material not in memory, using StaticLoadObject for: %s"), *MaterialPath);
		LoadedObj = StaticLoadObject(UMaterial::StaticClass(), nullptr, *MaterialPath);
	}

	if (!LoadedObj)
	{
		OutError = FString::Printf(TEXT("Could not load material at path: %s"), *MaterialPath);
		return nullptr;
	}

	UMaterial* Material = Cast<UMaterial>(LoadedObj);
	if (!Material)
	{
		OutError = FString::Printf(TEXT("Object at path is not a UMaterial: %s"), *MaterialPath);
		return nullptr;
	}

	return Material;
}

// ---------------------------------------------------------------------------
// create_material
// Params: { "name": "PP_Outline", "path": "/Game/Materials/PostProcess" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialName;
	if (!Params->TryGetStringField(TEXT("name"), MaterialName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	FString PackagePath = TEXT("/Game/Materials/");
	Params->TryGetStringField(TEXT("path"), PackagePath);
	if (!PackagePath.EndsWith(TEXT("/")))
	{
		PackagePath += TEXT("/");
	}

	FString FullPath = PackagePath + MaterialName;
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material already exists: %s"), *FullPath));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
	UPackage* Package = CreatePackage(*FullPath);
	UObject* NewObj = Factory->FactoryCreateNew(
		UMaterial::StaticClass(), Package, *MaterialName,
		RF_Standalone | RF_Public, nullptr, GWarn);

	UMaterial* NewMaterial = Cast<UMaterial>(NewObj);
	if (!NewMaterial)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material"));
	}

	FAssetRegistryModule::AssetCreated(NewMaterial);
	Package->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(FullPath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("name"), MaterialName);
	Result->SetStringField(TEXT("path"), FullPath);
	return Result;
}

// ---------------------------------------------------------------------------
// create_material_instance
// Params: { "name", "path", "parent" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params)
{
	FString InstanceName;
	if (!Params->TryGetStringField(TEXT("name"), InstanceName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	FString ParentPath;
	if (!Params->TryGetStringField(TEXT("parent"), ParentPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parent' parameter"));
	}

	UMaterialInterface* ParentMaterial = Cast<UMaterialInterface>(
		StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *ParentPath));
	if (!ParentMaterial)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Could not load parent material: %s"), *ParentPath));
	}

	FString PackagePath = TEXT("/Game/Materials/");
	Params->TryGetStringField(TEXT("path"), PackagePath);
	if (!PackagePath.EndsWith(TEXT("/")))
	{
		PackagePath += TEXT("/");
	}

	FString FullPath = PackagePath + InstanceName;
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material instance already exists: %s"), *FullPath));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
	Factory->InitialParent = ParentMaterial;

	UPackage* Package = CreatePackage(*FullPath);
	UObject* NewObj = Factory->FactoryCreateNew(
		UMaterialInstanceConstant::StaticClass(), Package, *InstanceName,
		RF_Standalone | RF_Public, nullptr, GWarn);

	UMaterialInstanceConstant* NewMIC = Cast<UMaterialInstanceConstant>(NewObj);
	if (!NewMIC)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material instance"));
	}

	FAssetRegistryModule::AssetCreated(NewMIC);
	Package->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(FullPath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("name"), InstanceName);
	Result->SetStringField(TEXT("path"), FullPath);
	Result->SetStringField(TEXT("parent"), ParentPath);
	return Result;
}

// ---------------------------------------------------------------------------
// create_material_parameter_collection
// Params: { "name", "path", "scalar_params": [...], "vector_params": [...] }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleCreateMaterialParameterCollection(const TSharedPtr<FJsonObject>& Params)
{
	FString MPCName;
	if (!Params->TryGetStringField(TEXT("name"), MPCName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	FString PackagePath = TEXT("/Game/Materials/");
	Params->TryGetStringField(TEXT("path"), PackagePath);
	if (!PackagePath.EndsWith(TEXT("/")))
	{
		PackagePath += TEXT("/");
	}

	FString FullPath = PackagePath + MPCName;
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("MPC already exists: %s"), *FullPath));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	UMaterialParameterCollectionFactoryNew* Factory = NewObject<UMaterialParameterCollectionFactoryNew>();
	UPackage* Package = CreatePackage(*FullPath);
	UObject* NewObj = Factory->FactoryCreateNew(
		UMaterialParameterCollection::StaticClass(), Package, *MPCName,
		RF_Standalone | RF_Public, nullptr, GWarn);

	UMaterialParameterCollection* NewMPC = Cast<UMaterialParameterCollection>(NewObj);
	if (!NewMPC)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create MPC"));
	}

	// Add scalar parameters
	const TArray<TSharedPtr<FJsonValue>>* ScalarParamsJson;
	if (Params->TryGetArrayField(TEXT("scalar_params"), ScalarParamsJson))
	{
		for (const TSharedPtr<FJsonValue>& ParamVal : *ScalarParamsJson)
		{
			const TSharedPtr<FJsonObject>* ParamObj;
			if (ParamVal->TryGetObject(ParamObj))
			{
				FCollectionScalarParameter ScalarParam;

				FString ParamName;
				if ((*ParamObj)->TryGetStringField(TEXT("name"), ParamName))
				{
					ScalarParam.ParameterName = FName(*ParamName);
				}

				double DefaultVal = 0.0;
				if ((*ParamObj)->TryGetNumberField(TEXT("default"), DefaultVal))
				{
					ScalarParam.DefaultValue = static_cast<float>(DefaultVal);
				}

				NewMPC->ScalarParameters.Add(ScalarParam);
			}
		}
	}

	// Add vector parameters
	const TArray<TSharedPtr<FJsonValue>>* VectorParamsJson;
	if (Params->TryGetArrayField(TEXT("vector_params"), VectorParamsJson))
	{
		for (const TSharedPtr<FJsonValue>& ParamVal : *VectorParamsJson)
		{
			const TSharedPtr<FJsonObject>* ParamObj;
			if (ParamVal->TryGetObject(ParamObj))
			{
				FCollectionVectorParameter VectorParam;

				FString ParamName;
				if ((*ParamObj)->TryGetStringField(TEXT("name"), ParamName))
				{
					VectorParam.ParameterName = FName(*ParamName);
				}

				// Default value as [R, G, B, A] array
				const TArray<TSharedPtr<FJsonValue>>* ColorArray;
				if ((*ParamObj)->TryGetArrayField(TEXT("default"), ColorArray) && ColorArray->Num() >= 3)
				{
					VectorParam.DefaultValue.R = static_cast<float>((*ColorArray)[0]->AsNumber());
					VectorParam.DefaultValue.G = static_cast<float>((*ColorArray)[1]->AsNumber());
					VectorParam.DefaultValue.B = static_cast<float>((*ColorArray)[2]->AsNumber());
					VectorParam.DefaultValue.A = ColorArray->Num() >= 4
						? static_cast<float>((*ColorArray)[3]->AsNumber())
						: 1.0f;
				}

				NewMPC->VectorParameters.Add(VectorParam);
			}
		}
	}

	NewMPC->PostEditChange();
	FAssetRegistryModule::AssetCreated(NewMPC);
	Package->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(FullPath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("name"), MPCName);
	Result->SetStringField(TEXT("path"), FullPath);
	Result->SetNumberField(TEXT("scalar_param_count"), NewMPC->ScalarParameters.Num());
	Result->SetNumberField(TEXT("vector_param_count"), NewMPC->VectorParameters.Num());
	return Result;
}

// ---------------------------------------------------------------------------
// add_material_expression
// Params: { "material", "expression_class", "node_x", "node_y", "properties": { ... } }
// Returns: { "expression_index": N }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleAddMaterialExpression(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material"), MaterialPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material' parameter"));
	}

	FString ExpressionClassName;
	if (!Params->TryGetStringField(TEXT("expression_class"), ExpressionClassName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'expression_class' parameter"));
	}

	FString Error;
	UMaterial* Material = LoadMaterialFromPath(MaterialPath, Error);
	if (!Material)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	// Resolve expression class by name
	// User can pass "MaterialExpressionCustom" or "UMaterialExpressionCustom"
	FString FullClassName = ExpressionClassName;
	if (!FullClassName.StartsWith(TEXT("U")))
	{
		FullClassName = TEXT("U") + FullClassName;
	}

	// Also prepare a version without the U prefix (some UE5 APIs use bare names in paths)
	FString BareClassName = FullClassName.Mid(1); // Strip the U prefix

	// Try multiple lookup strategies since UE5 class path formats vary
	UClass* ExpressionClass = nullptr;

	// Strategy 1: LoadClass with U prefix (e.g., /Script/Engine.UMaterialExpressionCustom)
	static const TCHAR* ModulePaths[] = { TEXT("Engine"), TEXT("Landscape") };
	for (const TCHAR* Module : ModulePaths)
	{
		if (ExpressionClass) break;

		// Try with U prefix
		FString ClassPath = FString::Printf(TEXT("/Script/%s.%s"), Module, *FullClassName);
		ExpressionClass = LoadClass<UMaterialExpression>(nullptr, *ClassPath);

		// Try without U prefix
		if (!ExpressionClass)
		{
			ClassPath = FString::Printf(TEXT("/Script/%s.%s"), Module, *BareClassName);
			ExpressionClass = LoadClass<UMaterialExpression>(nullptr, *ClassPath);
		}
	}

	// Strategy 2: FindFirstObjectSafe (searches all loaded packages)
	if (!ExpressionClass)
	{
		ExpressionClass = FindFirstObjectSafe<UClass>(*FullClassName);
		if (ExpressionClass && !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
		{
			ExpressionClass = nullptr;
		}
	}

	if (!ExpressionClass)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Could not find material expression class: %s (tried /Script/Engine.%s and /Script/Engine.%s)"),
				*ExpressionClassName, *FullClassName, *BareClassName));
	}

	int32 NodePosX = 0, NodePosY = 0;
	Params->TryGetNumberField(TEXT("node_x"), NodePosX);
	Params->TryGetNumberField(TEXT("node_y"), NodePosY);

	UMaterialExpression* NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(
		Material,
		ExpressionClass,
		NodePosX,
		NodePosY);

	if (!NewExpression)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material expression"));
	}

	// Set expression-specific properties via reflection
	const TSharedPtr<FJsonObject>* PropertiesObj;
	if (Params->TryGetObjectField(TEXT("properties"), PropertiesObj))
	{
		for (const auto& Pair : (*PropertiesObj)->Values)
		{
			FString PropError;
			if (!FUnrealMCPCommonUtils::SetObjectProperty(NewExpression, Pair.Key, Pair.Value, PropError))
			{
				UE_LOG(LogTemp, Warning, TEXT("add_material_expression: Could not set property '%s': %s"),
					*Pair.Key, *PropError);
			}
		}

		// Trigger PostEditChangeProperty so expressions like Custom HLSL
		// rebuild their input/output pins from reflection-set arrays
		FPropertyChangedEvent EmptyEvent(nullptr);
		NewExpression->PostEditChangeProperty(EmptyEvent);
	}

	// Find expression index
	int32 ExpressionIndex = -1;
	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Material->GetExpressions();
	for (int32 i = 0; i < Expressions.Num(); ++i)
	{
		if (Expressions[i] == NewExpression)
		{
			ExpressionIndex = i;
			break;
		}
	}

	Material->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(MaterialPath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("expression_index"), ExpressionIndex);
	Result->SetStringField(TEXT("expression_class"), NewExpression->GetClass()->GetName());
	Result->SetStringField(TEXT("expression_name"), NewExpression->GetName());
	return Result;
}

// ---------------------------------------------------------------------------
// connect_material_expressions
// Params: { "material", "from_expression_index", "from_output", "to_expression_index", "to_input" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleConnectMaterialExpressions(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material"), MaterialPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material' parameter"));
	}

	FString Error;
	UMaterial* Material = LoadMaterialFromPath(MaterialPath, Error);
	if (!Material)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	int32 FromIndex = -1, ToIndex = -1;
	if (!Params->TryGetNumberField(TEXT("from_expression_index"), FromIndex))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'from_expression_index' parameter"));
	}
	if (!Params->TryGetNumberField(TEXT("to_expression_index"), ToIndex))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'to_expression_index' parameter"));
	}

	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Material->GetExpressions();

	if (FromIndex < 0 || FromIndex >= Expressions.Num())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("from_expression_index %d out of range (0-%d)"), FromIndex, Expressions.Num() - 1));
	}
	if (ToIndex < 0 || ToIndex >= Expressions.Num())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("to_expression_index %d out of range (0-%d)"), ToIndex, Expressions.Num() - 1));
	}

	FString FromOutput;
	Params->TryGetStringField(TEXT("from_output"), FromOutput);

	FString ToInput;
	Params->TryGetStringField(TEXT("to_input"), ToInput);

	bool bSuccess = UMaterialEditingLibrary::ConnectMaterialExpressions(
		Expressions[FromIndex], FromOutput,
		Expressions[ToIndex], ToInput);

	if (!bSuccess)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to connect expression %d (output '%s') -> expression %d (input '%s')"),
				FromIndex, *FromOutput, ToIndex, *ToInput));
	}

	Material->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(MaterialPath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("connected"), true);
	Result->SetNumberField(TEXT("from_expression_index"), FromIndex);
	Result->SetNumberField(TEXT("to_expression_index"), ToIndex);
	return Result;
}

// ---------------------------------------------------------------------------
// connect_material_to_property
// Params: { "material", "from_expression_index", "from_output", "property" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleConnectMaterialToProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material"), MaterialPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material' parameter"));
	}

	FString Error;
	UMaterial* Material = LoadMaterialFromPath(MaterialPath, Error);
	if (!Material)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	int32 FromIndex = -1;
	if (!Params->TryGetNumberField(TEXT("from_expression_index"), FromIndex))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'from_expression_index' parameter"));
	}

	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Material->GetExpressions();

	if (FromIndex < 0 || FromIndex >= Expressions.Num())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("from_expression_index %d out of range (0-%d)"), FromIndex, Expressions.Num() - 1));
	}

	FString FromOutput;
	Params->TryGetStringField(TEXT("from_output"), FromOutput);

	FString PropertyName;
	if (!Params->TryGetStringField(TEXT("property"), PropertyName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property' parameter"));
	}

	// Map string property name to EMaterialProperty
	EMaterialProperty MaterialProperty = MP_MAX;
	if (PropertyName == TEXT("EmissiveColor") || PropertyName == TEXT("MP_EmissiveColor"))
	{
		MaterialProperty = MP_EmissiveColor;
	}
	else if (PropertyName == TEXT("Opacity") || PropertyName == TEXT("MP_Opacity"))
	{
		MaterialProperty = MP_Opacity;
	}
	else if (PropertyName == TEXT("OpacityMask") || PropertyName == TEXT("MP_OpacityMask"))
	{
		MaterialProperty = MP_OpacityMask;
	}
	else if (PropertyName == TEXT("BaseColor") || PropertyName == TEXT("MP_BaseColor"))
	{
		MaterialProperty = MP_BaseColor;
	}
	else if (PropertyName == TEXT("Metallic") || PropertyName == TEXT("MP_Metallic"))
	{
		MaterialProperty = MP_Metallic;
	}
	else if (PropertyName == TEXT("Specular") || PropertyName == TEXT("MP_Specular"))
	{
		MaterialProperty = MP_Specular;
	}
	else if (PropertyName == TEXT("Roughness") || PropertyName == TEXT("MP_Roughness"))
	{
		MaterialProperty = MP_Roughness;
	}
	else if (PropertyName == TEXT("Normal") || PropertyName == TEXT("MP_Normal"))
	{
		MaterialProperty = MP_Normal;
	}
	else if (PropertyName == TEXT("WorldPositionOffset") || PropertyName == TEXT("MP_WorldPositionOffset"))
	{
		MaterialProperty = MP_WorldPositionOffset;
	}
	else if (PropertyName == TEXT("AmbientOcclusion") || PropertyName == TEXT("MP_AmbientOcclusion"))
	{
		MaterialProperty = MP_AmbientOcclusion;
	}
	else if (PropertyName == TEXT("Refraction") || PropertyName == TEXT("MP_Refraction"))
	{
		MaterialProperty = MP_Refraction;
	}
	else if (PropertyName == TEXT("SubsurfaceColor") || PropertyName == TEXT("MP_SubsurfaceColor"))
	{
		MaterialProperty = MP_SubsurfaceColor;
	}
	else if (PropertyName == TEXT("Anisotropy") || PropertyName == TEXT("MP_Anisotropy"))
	{
		MaterialProperty = MP_Anisotropy;
	}
	else if (PropertyName == TEXT("Tangent") || PropertyName == TEXT("MP_Tangent"))
	{
		MaterialProperty = MP_Tangent;
	}
	else if (PropertyName == TEXT("PixelDepthOffset") || PropertyName == TEXT("MP_PixelDepthOffset"))
	{
		MaterialProperty = MP_PixelDepthOffset;
	}
	else
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unknown material property: %s. Use: EmissiveColor, Opacity, OpacityMask, BaseColor, Metallic, Specular, Roughness, Normal, etc."), *PropertyName));
	}

	bool bSuccess = UMaterialEditingLibrary::ConnectMaterialProperty(
		Expressions[FromIndex], FromOutput, MaterialProperty);

	if (!bSuccess)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to connect expression %d to material property '%s'"), FromIndex, *PropertyName));
	}

	Material->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(MaterialPath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("connected"), true);
	Result->SetStringField(TEXT("property"), PropertyName);
	return Result;
}

// ---------------------------------------------------------------------------
// set_material_property
// Params: { "material", "properties": { "MaterialDomain": "MD_PostProcess", ... } }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleSetMaterialProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material"), MaterialPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material' parameter"));
	}

	FString Error;
	UMaterial* Material = LoadMaterialFromPath(MaterialPath, Error);
	if (!Material)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	const TSharedPtr<FJsonObject>* PropertiesObj;
	if (!Params->TryGetObjectField(TEXT("properties"), PropertiesObj))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'properties' parameter"));
	}

	TArray<FString> SetProperties;
	TArray<FString> FailedProperties;

	for (const auto& Pair : (*PropertiesObj)->Values)
	{
		FString PropError;
		if (FUnrealMCPCommonUtils::SetObjectProperty(Material, Pair.Key, Pair.Value, PropError))
		{
			SetProperties.Add(Pair.Key);
		}
		else
		{
			FailedProperties.Add(FString::Printf(TEXT("%s: %s"), *Pair.Key, *PropError));
		}
	}

	Material->PreEditChange(nullptr);
	Material->PostEditChange();
	Material->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(MaterialPath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("properties_set"), SetProperties.Num());
	Result->SetNumberField(TEXT("properties_failed"), FailedProperties.Num());

	TArray<TSharedPtr<FJsonValue>> SetArr;
	for (const FString& Prop : SetProperties)
	{
		SetArr.Add(MakeShared<FJsonValueString>(Prop));
	}
	Result->SetArrayField(TEXT("set"), SetArr);

	if (FailedProperties.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> FailArr;
		for (const FString& Fail : FailedProperties)
		{
			FailArr.Add(MakeShared<FJsonValueString>(Fail));
		}
		Result->SetArrayField(TEXT("failed"), FailArr);
	}

	return Result;
}

// ---------------------------------------------------------------------------
// recompile_material
// Params: { "material": "/Game/Materials/PP_Outline" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleRecompileMaterial(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material"), MaterialPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material' parameter"));
	}

	FString Error;
	UMaterial* Material = LoadMaterialFromPath(MaterialPath, Error);
	if (!Material)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	UMaterialEditingLibrary::RecompileMaterial(Material);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("material"), MaterialPath);
	Result->SetBoolField(TEXT("recompiled"), true);
	return Result;
}

// ---------------------------------------------------------------------------
// Helper: Load a UMaterialInstanceConstant from a content path
// ---------------------------------------------------------------------------
UMaterialInstanceConstant* FUnrealMCPMaterialCommands::LoadMaterialInstanceFromPath(const FString& InstancePath, FString& OutError)
{
	UObject* LoadedObj = StaticLoadObject(UMaterialInstanceConstant::StaticClass(), nullptr, *InstancePath);
	if (!LoadedObj)
	{
		OutError = FString::Printf(TEXT("Could not load material instance at path: %s"), *InstancePath);
		return nullptr;
	}

	UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(LoadedObj);
	if (!MIC)
	{
		OutError = FString::Printf(TEXT("Object at path is not a UMaterialInstanceConstant: %s"), *InstancePath);
		return nullptr;
	}

	return MIC;
}

// ---------------------------------------------------------------------------
// set_material_instance_scalar_parameter
// Params: { "instance": "/Game/...", "parameter_name": "...", "value": 0.5 }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleSetMaterialInstanceScalarParameter(const TSharedPtr<FJsonObject>& Params)
{
	FString InstancePath;
	if (!Params->TryGetStringField(TEXT("instance"), InstancePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'instance' parameter"));
	}

	FString ParameterName;
	if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
	}

	double Value = 0.0;
	if (!Params->TryGetNumberField(TEXT("value"), Value))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' parameter"));
	}

	FString Error;
	UMaterialInstanceConstant* MIC = LoadMaterialInstanceFromPath(InstancePath, Error);
	if (!MIC)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	FMaterialParameterInfo ParamInfo{FName(*ParameterName)};
	MIC->SetScalarParameterValueEditorOnly(ParamInfo, static_cast<float>(Value));
	MIC->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(InstancePath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("instance"), InstancePath);
	Result->SetStringField(TEXT("parameter_name"), ParameterName);
	Result->SetNumberField(TEXT("value"), Value);
	Result->SetBoolField(TEXT("success"), true);
	return Result;
}

// ---------------------------------------------------------------------------
// set_material_instance_vector_parameter
// Params: { "instance": "/Game/...", "parameter_name": "...", "value": [R,G,B,A] }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleSetMaterialInstanceVectorParameter(const TSharedPtr<FJsonObject>& Params)
{
	FString InstancePath;
	if (!Params->TryGetStringField(TEXT("instance"), InstancePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'instance' parameter"));
	}

	FString ParameterName;
	if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
	}

	const TArray<TSharedPtr<FJsonValue>>* ValueArray;
	if (!Params->TryGetArrayField(TEXT("value"), ValueArray) || ValueArray->Num() < 3)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or invalid 'value' parameter (expected [R,G,B] or [R,G,B,A])"));
	}

	FLinearColor Color;
	Color.R = static_cast<float>((*ValueArray)[0]->AsNumber());
	Color.G = static_cast<float>((*ValueArray)[1]->AsNumber());
	Color.B = static_cast<float>((*ValueArray)[2]->AsNumber());
	Color.A = ValueArray->Num() >= 4 ? static_cast<float>((*ValueArray)[3]->AsNumber()) : 1.0f;

	FString Error;
	UMaterialInstanceConstant* MIC = LoadMaterialInstanceFromPath(InstancePath, Error);
	if (!MIC)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	FMaterialParameterInfo ParamInfo{FName(*ParameterName)};
	MIC->SetVectorParameterValueEditorOnly(ParamInfo, Color);
	MIC->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(InstancePath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("instance"), InstancePath);
	Result->SetStringField(TEXT("parameter_name"), ParameterName);
	Result->SetBoolField(TEXT("success"), true);
	return Result;
}

// ---------------------------------------------------------------------------
// set_material_instance_texture_parameter
// Params: { "instance": "/Game/...", "parameter_name": "...", "texture_path": "/Game/..." }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleSetMaterialInstanceTextureParameter(const TSharedPtr<FJsonObject>& Params)
{
	FString InstancePath;
	if (!Params->TryGetStringField(TEXT("instance"), InstancePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'instance' parameter"));
	}

	FString ParameterName;
	if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
	}

	FString TexturePath;
	if (!Params->TryGetStringField(TEXT("texture_path"), TexturePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'texture_path' parameter"));
	}

	FString Error;
	UMaterialInstanceConstant* MIC = LoadMaterialInstanceFromPath(InstancePath, Error);
	if (!MIC)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	UTexture* Texture = LoadObject<UTexture>(nullptr, *TexturePath);
	if (!Texture)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Could not load texture at path: %s"), *TexturePath));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	FMaterialParameterInfo ParamInfo{FName(*ParameterName)};
	MIC->SetTextureParameterValueEditorOnly(ParamInfo, Texture);
	MIC->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(InstancePath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("instance"), InstancePath);
	Result->SetStringField(TEXT("parameter_name"), ParameterName);
	Result->SetStringField(TEXT("texture_path"), TexturePath);
	Result->SetBoolField(TEXT("success"), true);
	return Result;
}

// ---------------------------------------------------------------------------
// set_material_instance_static_switch_parameter
// Params: { "instance": "/Game/...", "parameter_name": "...", "value": true/false }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleSetMaterialInstanceStaticSwitchParameter(const TSharedPtr<FJsonObject>& Params)
{
	FString InstancePath;
	if (!Params->TryGetStringField(TEXT("instance"), InstancePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'instance' parameter"));
	}

	FString ParameterName;
	if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
	}

	bool bValue = false;
	if (!Params->TryGetBoolField(TEXT("value"), bValue))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' parameter (expected bool)"));
	}

	FString Error;
	UMaterialInstanceConstant* MIC = LoadMaterialInstanceFromPath(InstancePath, Error);
	if (!MIC)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	// Get the current static parameter set, modify, and apply
	FStaticParameterSet StaticParams;
	MIC->GetStaticParameterValues(StaticParams);

	bool bFound = false;
	for (FStaticSwitchParameter& SwitchParam : StaticParams.StaticSwitchParameters)
	{
		if (SwitchParam.ParameterInfo.Name == FName(*ParameterName))
		{
			SwitchParam.Value = bValue;
			SwitchParam.bOverride = true;
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		// Parameter not found in existing set — add a new entry
		FStaticSwitchParameter NewParam;
		NewParam.ParameterInfo.Name = FName(*ParameterName);
		NewParam.Value = bValue;
		NewParam.bOverride = true;
		StaticParams.StaticSwitchParameters.Add(NewParam);
	}

	MIC->UpdateStaticPermutation(StaticParams);
	MIC->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(InstancePath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("instance"), InstancePath);
	Result->SetStringField(TEXT("parameter_name"), ParameterName);
	Result->SetBoolField(TEXT("value"), bValue);
	Result->SetBoolField(TEXT("success"), true);
	return Result;
}

// ---------------------------------------------------------------------------
// get_material_info
// Returns all expressions, their types, parameter names, and what's
// connected to material property pins (BaseColor, Roughness, etc.)
// Params: { "material": "/Game/Materials/Characters/M_Gear_Master" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleGetMaterialInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material"), MaterialPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material' parameter"));
	}

	FString Error;
	UMaterial* Material = LoadMaterialFromPath(MaterialPath, Error);
	if (!Material)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Material->GetExpressions();

	// Build expressions array
	TArray<TSharedPtr<FJsonValue>> ExpressionsArray;
	for (int32 i = 0; i < Expressions.Num(); ++i)
	{
		UMaterialExpression* Expr = Expressions[i];
		if (!Expr) continue;

		TSharedPtr<FJsonObject> ExprObj = MakeShared<FJsonObject>();
		ExprObj->SetNumberField(TEXT("index"), i);
		ExprObj->SetStringField(TEXT("class"), Expr->GetClass()->GetName());
		ExprObj->SetStringField(TEXT("name"), Expr->GetName());
		ExprObj->SetStringField(TEXT("desc"), Expr->GetDescription());

		// Extract parameter name for parameter expressions
		if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expr))
		{
			ExprObj->SetStringField(TEXT("parameter_name"), ScalarParam->ParameterName.ToString());
			ExprObj->SetNumberField(TEXT("default_value"), ScalarParam->DefaultValue);
		}
		else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(Expr))
		{
			ExprObj->SetStringField(TEXT("parameter_name"), VectorParam->ParameterName.ToString());
			TArray<TSharedPtr<FJsonValue>> ColorArr;
			ColorArr.Add(MakeShared<FJsonValueNumber>(VectorParam->DefaultValue.R));
			ColorArr.Add(MakeShared<FJsonValueNumber>(VectorParam->DefaultValue.G));
			ColorArr.Add(MakeShared<FJsonValueNumber>(VectorParam->DefaultValue.B));
			ColorArr.Add(MakeShared<FJsonValueNumber>(VectorParam->DefaultValue.A));
			ExprObj->SetArrayField(TEXT("default_value"), ColorArr);
		}
		else if (UMaterialExpressionStaticSwitchParameter* SwitchParam = Cast<UMaterialExpressionStaticSwitchParameter>(Expr))
		{
			ExprObj->SetStringField(TEXT("parameter_name"), SwitchParam->ParameterName.ToString());
			ExprObj->SetBoolField(TEXT("default_value"), SwitchParam->DefaultValue);
		}
		else if (UMaterialExpressionCustom* CustomExpr = Cast<UMaterialExpressionCustom>(Expr))
		{
			ExprObj->SetStringField(TEXT("code"), CustomExpr->Code);
			ExprObj->SetNumberField(TEXT("output_type"), (int32)CustomExpr->OutputType);
		}

		// Get input connections for this expression using FExpressionInputIterator (UE 5.7 API)
		{
			TArray<TSharedPtr<FJsonValue>> InputsArr;
			for (FExpressionInputIterator It{ Expr }; It; ++It)
			{
				TSharedPtr<FJsonObject> InputObj = MakeShared<FJsonObject>();
				InputObj->SetStringField(TEXT("name"), Expr->GetInputName(It.Index).ToString());

				if (It->Expression)
				{
					// Find the index of the connected expression
					for (int32 j = 0; j < Expressions.Num(); ++j)
					{
						if (Expressions[j] == It->Expression)
						{
							InputObj->SetNumberField(TEXT("connected_to"), j);
							InputObj->SetNumberField(TEXT("output_index"), It->OutputIndex);
							break;
						}
					}
				}

				InputsArr.Add(MakeShared<FJsonValueObject>(InputObj));
			}
			if (InputsArr.Num() > 0)
			{
				ExprObj->SetArrayField(TEXT("inputs"), InputsArr);
			}
		}

		ExpressionsArray.Add(MakeShared<FJsonValueObject>(ExprObj));
	}

	// Build property connections using GetExpressionInputForProperty (UE 5.7 API)
	TSharedPtr<FJsonObject> PropertyConnections = MakeShared<FJsonObject>();

	struct FPropertyEntry {
		const TCHAR* Name;
		EMaterialProperty Property;
	};

	const FPropertyEntry Properties[] = {
		{ TEXT("BaseColor"), MP_BaseColor },
		{ TEXT("Metallic"), MP_Metallic },
		{ TEXT("Specular"), MP_Specular },
		{ TEXT("Roughness"), MP_Roughness },
		{ TEXT("Normal"), MP_Normal },
		{ TEXT("EmissiveColor"), MP_EmissiveColor },
		{ TEXT("Opacity"), MP_Opacity },
		{ TEXT("OpacityMask"), MP_OpacityMask },
		{ TEXT("AmbientOcclusion"), MP_AmbientOcclusion },
		{ TEXT("WorldPositionOffset"), MP_WorldPositionOffset },
		{ TEXT("SubsurfaceColor"), MP_SubsurfaceColor },
		{ TEXT("Anisotropy"), MP_Anisotropy },
		{ TEXT("Tangent"), MP_Tangent },
		{ TEXT("Refraction"), MP_Refraction },
		{ TEXT("PixelDepthOffset"), MP_PixelDepthOffset },
	};

	for (const FPropertyEntry& PropEntry : Properties)
	{
		FExpressionInput* Input = Material->GetExpressionInputForProperty(PropEntry.Property);
		if (Input && Input->Expression)
		{
			TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
			for (int32 j = 0; j < Expressions.Num(); ++j)
			{
				if (Expressions[j] == Input->Expression)
				{
					ConnObj->SetNumberField(TEXT("expression_index"), j);
					ConnObj->SetNumberField(TEXT("output_index"), Input->OutputIndex);
					break;
				}
			}
			PropertyConnections->SetObjectField(PropEntry.Name, ConnObj);
		}
	}

	// Build pseudocode graph representation
	FString GraphPseudocode = BuildGraphPseudocode(Material);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("material"), MaterialPath);
	Result->SetNumberField(TEXT("expression_count"), Expressions.Num());
	Result->SetArrayField(TEXT("expressions"), ExpressionsArray);
	Result->SetObjectField(TEXT("property_connections"), PropertyConnections);
	if (!GraphPseudocode.IsEmpty())
	{
		Result->SetStringField(TEXT("graph"), GraphPseudocode);
	}
	return Result;
}

// ---------------------------------------------------------------------------
// get_custom_expression_code
// Params: { "material": "/Game/...", "expression_index": 5 }
// Returns the HLSL code, description, output type, and input names
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleGetCustomExpressionCode(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material"), MaterialPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material' parameter"));
	}

	int32 ExpressionIndex = -1;
	if (!Params->TryGetNumberField(TEXT("expression_index"), ExpressionIndex))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'expression_index' parameter"));
	}

	FString Error;
	UMaterial* Material = LoadMaterialFromPath(MaterialPath, Error);
	if (!Material)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Material->GetExpressions();
	if (ExpressionIndex < 0 || ExpressionIndex >= Expressions.Num())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("expression_index %d out of range (0-%d)"), ExpressionIndex, Expressions.Num() - 1));
	}

	UMaterialExpressionCustom* CustomExpr = Cast<UMaterialExpressionCustom>(Expressions[ExpressionIndex]);
	if (!CustomExpr)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Expression at index %d is not a Custom HLSL node (class: %s)"),
				ExpressionIndex, *Expressions[ExpressionIndex]->GetClass()->GetName()));
	}

	// Optional truncation for very large HLSL code
	int32 MaxCodeLength = -1;
	Params->TryGetNumberField(TEXT("max_code_length"), MaxCodeLength);

	FString Code = CustomExpr->Code;
	int32 FullLength = Code.Len();
	bool bTruncated = false;

	if (MaxCodeLength > 0 && FullLength > MaxCodeLength)
	{
		Code = Code.Left(MaxCodeLength) + FString::Printf(TEXT("\n// ... [truncated, full length: %d chars]"), FullLength);
		bTruncated = true;
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("material"), MaterialPath);
	Result->SetNumberField(TEXT("expression_index"), ExpressionIndex);
	Result->SetStringField(TEXT("description"), CustomExpr->GetDescription());
	Result->SetStringField(TEXT("code"), Code);
	Result->SetNumberField(TEXT("full_code_length"), FullLength);
	Result->SetBoolField(TEXT("truncated"), bTruncated);
	Result->SetNumberField(TEXT("output_type"), (int32)CustomExpr->OutputType);

	// Include input names
	TArray<TSharedPtr<FJsonValue>> InputsArr;
	for (const FCustomInput& Input : CustomExpr->Inputs)
	{
		TSharedPtr<FJsonObject> InputObj = MakeShared<FJsonObject>();
		InputObj->SetStringField(TEXT("name"), Input.InputName.ToString());
		InputsArr.Add(MakeShared<FJsonValueObject>(InputObj));
	}
	Result->SetArrayField(TEXT("inputs"), InputsArr);

	// Include additional outputs
	TArray<TSharedPtr<FJsonValue>> OutputsArr;
	for (const FCustomOutput& Output : CustomExpr->AdditionalOutputs)
	{
		TSharedPtr<FJsonObject> OutputObj = MakeShared<FJsonObject>();
		OutputObj->SetStringField(TEXT("name"), Output.OutputName.ToString());
		OutputObj->SetNumberField(TEXT("type"), (int32)Output.OutputType);
		OutputsArr.Add(MakeShared<FJsonValueObject>(OutputObj));
	}
	Result->SetArrayField(TEXT("additional_outputs"), OutputsArr);

	// Include file paths
	TArray<TSharedPtr<FJsonValue>> IncludesArr;
	for (const FString& Path : CustomExpr->IncludeFilePaths)
	{
		IncludesArr.Add(MakeShared<FJsonValueString>(Path));
	}
	Result->SetArrayField(TEXT("include_file_paths"), IncludesArr);

	return Result;
}

// ---------------------------------------------------------------------------
// set_custom_expression_code
// Params: { "material": "/Game/...", "expression_index": 5, "code": "...", "description": "..." }
// Sets the HLSL code on a Custom expression node. Optionally updates description.
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleSetCustomExpressionCode(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material"), MaterialPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material' parameter"));
	}

	int32 ExpressionIndex = -1;
	if (!Params->TryGetNumberField(TEXT("expression_index"), ExpressionIndex))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'expression_index' parameter"));
	}

	FString NewCode;
	if (!Params->TryGetStringField(TEXT("code"), NewCode))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'code' parameter"));
	}

	FString Error;
	UMaterial* Material = LoadMaterialFromPath(MaterialPath, Error);
	if (!Material)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Material->GetExpressions();
	if (ExpressionIndex < 0 || ExpressionIndex >= Expressions.Num())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("expression_index %d out of range (0-%d)"), ExpressionIndex, Expressions.Num() - 1));
	}

	UMaterialExpressionCustom* CustomExpr = Cast<UMaterialExpressionCustom>(Expressions[ExpressionIndex]);
	if (!CustomExpr)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Expression at index %d is not a Custom HLSL node (class: %s)"),
				ExpressionIndex, *Expressions[ExpressionIndex]->GetClass()->GetName()));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	// Update code
	CustomExpr->Code = NewCode;

	// Optionally update description
	FString NewDescription;
	if (Params->TryGetStringField(TEXT("description"), NewDescription))
	{
		CustomExpr->Description = NewDescription;
	}

	// Trigger rebuild of pins/compilation
	FPropertyChangedEvent EmptyEvent(nullptr);
	CustomExpr->PostEditChangeProperty(EmptyEvent);

	Material->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(MaterialPath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("material"), MaterialPath);
	Result->SetNumberField(TEXT("expression_index"), ExpressionIndex);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("code_length"), NewCode.Len());
	return Result;
}

// ---------------------------------------------------------------------------
// get_expression_properties
// Returns all editable properties on a material expression node via reflection.
// Params: { "material": "/Game/...", "expression_index": 0 }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleGetExpressionProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material"), MaterialPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material' parameter"));
	}

	int32 ExpressionIndex = -1;
	if (!Params->TryGetNumberField(TEXT("expression_index"), ExpressionIndex))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'expression_index' parameter"));
	}

	FString Error;
	UMaterial* Material = LoadMaterialFromPath(MaterialPath, Error);
	if (!Material)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Material->GetExpressions();
	if (ExpressionIndex < 0 || ExpressionIndex >= Expressions.Num())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("expression_index %d out of range (0-%d)"), ExpressionIndex, Expressions.Num() - 1));
	}

	UMaterialExpression* Expr = Expressions[ExpressionIndex];
	if (!Expr)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Expression at index is null"));
	}

	UClass* ExprClass = Expr->GetClass();
	UClass* BaseObjClass = UObject::StaticClass();
	UClass* BaseExprClass = UMaterialExpression::StaticClass();

	TArray<TSharedPtr<FJsonValue>> PropertiesArray;
	for (TFieldIterator<FProperty> It(ExprClass); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop) continue;

		// Only include editor-visible properties
		if (!Prop->HasAnyPropertyFlags(CPF_Edit))
			continue;

		// Skip internal UObject base properties
		UClass* OwnerClass = Prop->GetOwnerClass();
		if (OwnerClass == BaseObjClass)
			continue;

		TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
		PropObj->SetStringField(TEXT("name"), Prop->GetName());
		PropObj->SetStringField(TEXT("type"), Prop->GetCPPType());

		// Get value via reflection
		TSharedPtr<FJsonValue> Value = FUnrealMCPCommonUtils::GetPropertyAsJson(Prop, Expr);
		if (Value.IsValid())
		{
			PropObj->SetField(TEXT("value"), Value);
		}

		// Category metadata
		if (Prop->HasMetaData(TEXT("Category")))
		{
			PropObj->SetStringField(TEXT("category"), Prop->GetMetaData(TEXT("Category")));
		}

		// Mark base expression properties
		if (OwnerClass == BaseExprClass || (OwnerClass && OwnerClass->IsChildOf(BaseExprClass) && OwnerClass != ExprClass))
		{
			PropObj->SetBoolField(TEXT("is_base"), true);
		}

		PropertiesArray.Add(MakeShared<FJsonValueObject>(PropObj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("material"), MaterialPath);
	Result->SetNumberField(TEXT("expression_index"), ExpressionIndex);
	Result->SetStringField(TEXT("expression_class"), ExprClass->GetName());
	Result->SetStringField(TEXT("expression_name"), Expr->GetName());
	Result->SetStringField(TEXT("description"), Expr->GetDescription());
	Result->SetArrayField(TEXT("properties"), PropertiesArray);
	return Result;
}

// ---------------------------------------------------------------------------
// set_expression_property
// Sets properties on an existing material expression node via reflection.
// Params: { "material": "/Game/...", "expression_index": 0, "properties": { "SamplerType": 1 }, "save": true }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleSetExpressionProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material"), MaterialPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material' parameter"));
	}

	int32 ExpressionIndex = -1;
	if (!Params->TryGetNumberField(TEXT("expression_index"), ExpressionIndex))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'expression_index' parameter"));
	}

	const TSharedPtr<FJsonObject>* PropertiesObj;
	if (!Params->TryGetObjectField(TEXT("properties"), PropertiesObj))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'properties' parameter"));
	}

	FString Error;
	UMaterial* Material = LoadMaterialFromPath(MaterialPath, Error);
	if (!Material)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Material->GetExpressions();
	if (ExpressionIndex < 0 || ExpressionIndex >= Expressions.Num())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("expression_index %d out of range (0-%d)"), ExpressionIndex, Expressions.Num() - 1));
	}

	UMaterialExpression* Expr = Expressions[ExpressionIndex];
	if (!Expr)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Expression at index is null"));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	TArray<FString> SetProperties;
	TArray<FString> FailedProperties;

	for (const auto& Pair : (*PropertiesObj)->Values)
	{
		FString PropError;
		if (FUnrealMCPCommonUtils::SetObjectProperty(Expr, Pair.Key, Pair.Value, PropError))
		{
			SetProperties.Add(Pair.Key);
		}
		else
		{
			FailedProperties.Add(FString::Printf(TEXT("%s: %s"), *Pair.Key, *PropError));
		}
	}

	// Trigger pin rebuild and visual update
	FPropertyChangedEvent EmptyEvent(nullptr);
	Expr->PostEditChangeProperty(EmptyEvent);

	Material->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(MaterialPath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("material"), MaterialPath);
	Result->SetNumberField(TEXT("expression_index"), ExpressionIndex);
	Result->SetBoolField(TEXT("success"), FailedProperties.Num() == 0);

	TArray<TSharedPtr<FJsonValue>> SetArr;
	for (const FString& Prop : SetProperties)
	{
		SetArr.Add(MakeShared<FJsonValueString>(Prop));
	}
	Result->SetArrayField(TEXT("properties_set"), SetArr);

	if (FailedProperties.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> FailArr;
		for (const FString& Fail : FailedProperties)
		{
			FailArr.Add(MakeShared<FJsonValueString>(Fail));
		}
		Result->SetArrayField(TEXT("properties_failed"), FailArr);
	}

	return Result;
}

// ---------------------------------------------------------------------------
// disconnect_expression
// Disconnects input pin(s) on a material expression node.
// Params: { "material": "/Game/...", "expression_index": 0,
//           "input_name": "A" | "input_index": 0 | "disconnect_all": true,
//           "save": true }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleDisconnectExpression(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material"), MaterialPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material' parameter"));
	}

	int32 ExpressionIndex = -1;
	if (!Params->TryGetNumberField(TEXT("expression_index"), ExpressionIndex))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'expression_index' parameter"));
	}

	FString Error;
	UMaterial* Material = LoadMaterialFromPath(MaterialPath, Error);
	if (!Material)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Material->GetExpressions();
	if (ExpressionIndex < 0 || ExpressionIndex >= Expressions.Num())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("expression_index %d out of range (0-%d)"), ExpressionIndex, Expressions.Num() - 1));
	}

	UMaterialExpression* Expr = Expressions[ExpressionIndex];
	if (!Expr)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Expression at index is null"));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	int32 DisconnectedCount = 0;

	bool bDisconnectAll = false;
	if (Params->HasField(TEXT("disconnect_all")))
	{
		bDisconnectAll = Params->GetBoolField(TEXT("disconnect_all"));
	}

	FString InputName;
	bool bHasInputName = Params->TryGetStringField(TEXT("input_name"), InputName);

	int32 InputIndex = -1;
	bool bHasInputIndex = Params->TryGetNumberField(TEXT("input_index"), InputIndex);

	if (bDisconnectAll)
	{
		for (FExpressionInputIterator It{ Expr }; It; ++It)
		{
			if (It->Expression)
			{
				It->Expression = nullptr;
				It->OutputIndex = 0;
				DisconnectedCount++;
			}
		}
	}
	else if (bHasInputIndex)
	{
		FExpressionInput* Input = Expr->GetInput(InputIndex);
		if (!Input)
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("input_index %d is invalid for expression '%s'"), InputIndex, *Expr->GetClass()->GetName()));
		}
		if (Input->Expression)
		{
			Input->Expression = nullptr;
			Input->OutputIndex = 0;
			DisconnectedCount = 1;
		}
	}
	else if (bHasInputName)
	{
		bool bFound = false;
		for (FExpressionInputIterator It{ Expr }; It; ++It)
		{
			FString ThisInputName = Expr->GetInputName(It.Index).ToString();
			if (ThisInputName == InputName)
			{
				bFound = true;
				if (It->Expression)
				{
					It->Expression = nullptr;
					It->OutputIndex = 0;
					DisconnectedCount = 1;
				}
				break;
			}
		}
		if (!bFound)
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Input '%s' not found on expression '%s'"), *InputName, *Expr->GetClass()->GetName()));
		}
	}
	else
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Must specify 'input_name', 'input_index', or 'disconnect_all'"));
	}

	Material->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(MaterialPath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("material"), MaterialPath);
	Result->SetNumberField(TEXT("expression_index"), ExpressionIndex);
	Result->SetNumberField(TEXT("disconnected_count"), DisconnectedCount);
	Result->SetBoolField(TEXT("success"), true);
	return Result;
}

// ---------------------------------------------------------------------------
// remove_expression
// Deletes a material expression node from the graph. Auto-disconnects all links.
// WARNING: After removal, all expression indices above the removed index shift down by 1.
// Caller must re-query get_material_info before further index-based operations.
// Params: { "material": "/Game/...", "expression_index": 51, "save": true }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleRemoveExpression(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material"), MaterialPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material' parameter"));
	}

	int32 ExpressionIndex = -1;
	if (!Params->TryGetNumberField(TEXT("expression_index"), ExpressionIndex))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'expression_index' parameter"));
	}

	FString Error;
	UMaterial* Material = LoadMaterialFromPath(MaterialPath, Error);
	if (!Material)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Material->GetExpressions();
	if (ExpressionIndex < 0 || ExpressionIndex >= Expressions.Num())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("expression_index %d out of range (0-%d)"), ExpressionIndex, Expressions.Num() - 1));
	}

	UMaterialExpression* Expr = Expressions[ExpressionIndex];
	if (!Expr)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Expression at index is null"));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	// Check if this expression can be deleted
	if (!Expr->CanUserDeleteExpression())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Expression '%s' (class: %s) cannot be deleted"),
				*Expr->GetName(), *Expr->GetClass()->GetName()));
	}

	// Capture info before deletion
	FString RemovedClass = Expr->GetClass()->GetName();
	FString RemovedName = Expr->GetName();

	// Delete — UE handles: BreakLinksToExpression, clear property connections,
	// RemoveExpressionParameter, RemoveExpression, MarkAsGarbage
	UMaterialEditingLibrary::DeleteMaterialExpression(Material, Expr);

	// Get new count after deletion
	int32 NewExpressionCount = Material->GetExpressions().Num();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(MaterialPath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("material"), MaterialPath);
	Result->SetNumberField(TEXT("removed_index"), ExpressionIndex);
	Result->SetStringField(TEXT("removed_class"), RemovedClass);
	Result->SetStringField(TEXT("removed_name"), RemovedName);
	Result->SetNumberField(TEXT("new_expression_count"), NewExpressionCount);
	Result->SetBoolField(TEXT("success"), true);
	return Result;
}

// ===========================================================================
// Graph pseudocode generation
// ===========================================================================

FString FUnrealMCPMaterialCommands::SanitizeVarName(const FString& Name)
{
	FString Out = Name.ToLower();
	Out.ReplaceInline(TEXT(" "), TEXT("_"));
	FString Clean;
	for (TCHAR Ch : Out)
	{
		if (FChar::IsAlnum(Ch) || Ch == '_')
		{
			Clean.AppendChar(Ch);
		}
	}
	return Clean;
}

FString FUnrealMCPMaterialCommands::FormatOutputSuffix(int32 OutputIndex, UMaterialExpression* Expr)
{
	if (OutputIndex <= 0) return FString();

	// Texture samples: 1=R, 2=G, 3=B, 4=A, 5=RGBA
	if (Expr->IsA(UMaterialExpressionTextureSample::StaticClass()))
	{
		switch (OutputIndex)
		{
		case 1: return TEXT(".R");
		case 2: return TEXT(".G");
		case 3: return TEXT(".B");
		case 4: return TEXT(".A");
		case 5: return TEXT(".RGBA");
		default: return FString::Printf(TEXT(".Out%d"), OutputIndex);
		}
	}
	return FString::Printf(TEXT(".Out%d"), OutputIndex);
}

FString FUnrealMCPMaterialCommands::FormatMaskChannels(UMaterialExpression* Expr)
{
	UMaterialExpressionComponentMask* Mask = Cast<UMaterialExpressionComponentMask>(Expr);
	if (!Mask) return TEXT("?");

	FString Channels;
	if (Mask->R) Channels += TEXT("R");
	if (Mask->G) Channels += TEXT("G");
	if (Mask->B) Channels += TEXT("B");
	if (Mask->A) Channels += TEXT("A");
	return Channels;
}

FString FUnrealMCPMaterialCommands::FormatConstantValue(UMaterialExpression* Expr)
{
	if (UMaterialExpressionConstant* C = Cast<UMaterialExpressionConstant>(Expr))
	{
		return FString::Printf(TEXT("%g"), C->R);
	}
	if (UMaterialExpressionConstant2Vector* C2 = Cast<UMaterialExpressionConstant2Vector>(Expr))
	{
		return FString::Printf(TEXT("(%g, %g)"), C2->R, C2->G);
	}
	if (UMaterialExpressionConstant3Vector* C3 = Cast<UMaterialExpressionConstant3Vector>(Expr))
	{
		return FString::Printf(TEXT("(%g, %g, %g)"), C3->Constant.R, C3->Constant.G, C3->Constant.B);
	}
	if (UMaterialExpressionConstant4Vector* C4 = Cast<UMaterialExpressionConstant4Vector>(Expr))
	{
		return FString::Printf(TEXT("(%g, %g, %g, %g)"), C4->Constant.R, C4->Constant.G, C4->Constant.B, C4->Constant.A);
	}
	return TEXT("?");
}

// ---------------------------------------------------------------------------
// Helper: find the expression index a given input pin connects to
// ---------------------------------------------------------------------------
static bool FindConnectedInput(UMaterialExpression* Expr, int32 InputIdx,
	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions,
	int32& OutExprIndex, int32& OutOutputIndex)
{
	FExpressionInput* Input = Expr->GetInput(InputIdx);
	if (!Input || !Input->Expression) return false;

	for (int32 j = 0; j < Expressions.Num(); ++j)
	{
		if (Expressions[j] == Input->Expression)
		{
			OutExprIndex = j;
			OutOutputIndex = Input->OutputIndex;
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------
// Helper: get input by name from the expression's input iterator
// ---------------------------------------------------------------------------
static bool FindConnectedInputByName(UMaterialExpression* Expr, const FString& PinName,
	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions,
	int32& OutExprIndex, int32& OutOutputIndex)
{
	int32 Idx = 0;
	for (FExpressionInputIterator It{ Expr }; It; ++It, ++Idx)
	{
		if (Expr->GetInputName(It.Index).ToString() == PinName)
		{
			if (It->Expression)
			{
				for (int32 j = 0; j < Expressions.Num(); ++j)
				{
					if (Expressions[j] == It->Expression)
					{
						OutExprIndex = j;
						OutOutputIndex = It->OutputIndex;
						return true;
					}
				}
			}
			return false;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------
// FormatExpression — recursive pseudocode builder
// ---------------------------------------------------------------------------

FString FUnrealMCPMaterialCommands::FormatExpression(int32 Index, int32 OutputIndex, FGraphFormatContext& Ctx) const
{
	if (Index < 0 || Index >= Ctx.Expressions.Num() || !Ctx.Expressions[Index])
	{
		return TEXT("???");
	}

	UMaterialExpression* Expr = Ctx.Expressions[Index];
	FString Suffix = FormatOutputSuffix(OutputIndex, Expr);
	FString ClassName = Expr->GetClass()->GetName();

	// --- Variable indirection for shared nodes ---
	if (Ctx.VarNames.Contains(Index) && Ctx.Emitted.Contains(Index))
	{
		// Already emitted — just return the variable reference
		return TEXT("$") + Ctx.VarNames[Index] + Suffix;
	}

	// --- Helper lambda to format a named input pin ---
	auto Fmt = [&](const FString& PinName) -> FString
	{
		int32 ConnIdx = -1, ConnOut = 0;
		if (FindConnectedInputByName(Expr, PinName, Ctx.Expressions, ConnIdx, ConnOut))
		{
			return FormatExpression(ConnIdx, ConnOut, Ctx);
		}
		return FString();
	};

	// --- Helper to format input by index ---
	auto FmtIdx = [&](int32 PinIdx) -> FString
	{
		int32 ConnIdx = -1, ConnOut = 0;
		if (FindConnectedInput(Expr, PinIdx, Ctx.Expressions, ConnIdx, ConnOut))
		{
			return FormatExpression(ConnIdx, ConnOut, Ctx);
		}
		return FString();
	};

	FString Result;

	// === Leaf nodes (no inputs) ===

	if (ClassName == TEXT("MaterialExpressionScalarParameter"))
	{
		UMaterialExpressionScalarParameter* P = Cast<UMaterialExpressionScalarParameter>(Expr);
		Result = FString::Printf(TEXT("Param(\"%s\", %g)"), *P->ParameterName.ToString(), P->DefaultValue);
	}
	else if (ClassName == TEXT("MaterialExpressionVectorParameter"))
	{
		UMaterialExpressionVectorParameter* P = Cast<UMaterialExpressionVectorParameter>(Expr);
		Result = FString::Printf(TEXT("Color(\"%s\", (%g, %g, %g, %g))"),
			*P->ParameterName.ToString(), P->DefaultValue.R, P->DefaultValue.G, P->DefaultValue.B, P->DefaultValue.A);
	}
	else if (ClassName == TEXT("MaterialExpressionTextureSampleParameter2D"))
	{
		UMaterialExpressionTextureSampleParameter2D* P = Cast<UMaterialExpressionTextureSampleParameter2D>(Expr);
		Result = FString::Printf(TEXT("TexParam(\"%s\")"), *P->ParameterName.ToString());
	}
	else if (ClassName == TEXT("MaterialExpressionTextureSample"))
	{
		UMaterialExpressionTextureSample* TS = Cast<UMaterialExpressionTextureSample>(Expr);
		FString Path = TS->Texture ? TS->Texture->GetPathName() : TEXT("None");
		Result = FString::Printf(TEXT("Tex(\"%s\")"), *Path);
	}
	else if (ClassName == TEXT("MaterialExpressionConstant") ||
	         ClassName == TEXT("MaterialExpressionConstant2Vector") ||
	         ClassName == TEXT("MaterialExpressionConstant3Vector") ||
	         ClassName == TEXT("MaterialExpressionConstant4Vector"))
	{
		Result = FormatConstantValue(Expr);
	}
	else if (ClassName == TEXT("MaterialExpressionStaticSwitchParameter"))
	{
		UMaterialExpressionStaticSwitchParameter* P = Cast<UMaterialExpressionStaticSwitchParameter>(Expr);
		FString TrueExpr = Fmt(TEXT("True"));
		FString FalseExpr = Fmt(TEXT("False"));
		if (TrueExpr.IsEmpty()) TrueExpr = TEXT("???");
		if (FalseExpr.IsEmpty()) FalseExpr = TEXT("???");
		Result = FString::Printf(TEXT("Switch(\"%s\", %s, %s)"),
			*P->ParameterName.ToString(), *TrueExpr, *FalseExpr);
	}

	// === Unary ops ===

	else if (ClassName == TEXT("MaterialExpressionComponentMask"))
	{
		FString Input = Fmt(TEXT("Input"));
		FString Channels = FormatMaskChannels(Expr);
		if (Input.IsEmpty()) Input = TEXT("???");
		Result = Input + TEXT(".") + Channels;
	}
	else if (ClassName == TEXT("MaterialExpressionOneMinus"))
	{
		FString Input = FmtIdx(0);
		if (Input.IsEmpty()) Input = TEXT("???");
		Result = FString::Printf(TEXT("(1 - %s)"), *Input);
	}
	else if (ClassName == TEXT("MaterialExpressionAbs"))
	{
		FString Input = FmtIdx(0);
		Result = FString::Printf(TEXT("Abs(%s)"), *Input);
	}
	else if (ClassName == TEXT("MaterialExpressionSaturate"))
	{
		FString Input = FmtIdx(0);
		Result = FString::Printf(TEXT("Saturate(%s)"), *Input);
	}
	else if (ClassName == TEXT("MaterialExpressionNormalize"))
	{
		FString Input = FmtIdx(0);
		Result = FString::Printf(TEXT("Normalize(%s)"), *Input);
	}
	else if (ClassName == TEXT("MaterialExpressionFloor"))
	{
		FString Input = FmtIdx(0);
		Result = FString::Printf(TEXT("Floor(%s)"), *Input);
	}
	else if (ClassName == TEXT("MaterialExpressionCeil"))
	{
		FString Input = FmtIdx(0);
		Result = FString::Printf(TEXT("Ceil(%s)"), *Input);
	}
	else if (ClassName == TEXT("MaterialExpressionFrac"))
	{
		FString Input = FmtIdx(0);
		Result = FString::Printf(TEXT("Frac(%s)"), *Input);
	}
	else if (ClassName == TEXT("MaterialExpressionSine"))
	{
		FString Input = FmtIdx(0);
		Result = FString::Printf(TEXT("Sin(%s)"), *Input);
	}
	else if (ClassName == TEXT("MaterialExpressionCosine"))
	{
		FString Input = FmtIdx(0);
		Result = FString::Printf(TEXT("Cos(%s)"), *Input);
	}
	else if (ClassName == TEXT("MaterialExpressionSquareRoot"))
	{
		FString Input = FmtIdx(0);
		Result = FString::Printf(TEXT("Sqrt(%s)"), *Input);
	}

	// === Binary infix ops ===

	else if (ClassName == TEXT("MaterialExpressionMultiply"))
	{
		FString A = Fmt(TEXT("A")); if (A.IsEmpty()) A = TEXT("???");
		FString B = Fmt(TEXT("B")); if (B.IsEmpty()) B = TEXT("???");
		Result = FString::Printf(TEXT("(%s * %s)"), *A, *B);
	}
	else if (ClassName == TEXT("MaterialExpressionAdd"))
	{
		FString A = Fmt(TEXT("A")); if (A.IsEmpty()) A = TEXT("???");
		FString B = Fmt(TEXT("B")); if (B.IsEmpty()) B = TEXT("???");
		Result = FString::Printf(TEXT("(%s + %s)"), *A, *B);
	}
	else if (ClassName == TEXT("MaterialExpressionSubtract"))
	{
		FString A = Fmt(TEXT("A")); if (A.IsEmpty()) A = TEXT("???");
		FString B = Fmt(TEXT("B")); if (B.IsEmpty()) B = TEXT("???");
		Result = FString::Printf(TEXT("(%s - %s)"), *A, *B);
	}
	else if (ClassName == TEXT("MaterialExpressionDivide"))
	{
		FString A = Fmt(TEXT("A")); if (A.IsEmpty()) A = TEXT("???");
		FString B = Fmt(TEXT("B")); if (B.IsEmpty()) B = TEXT("???");
		Result = FString::Printf(TEXT("(%s / %s)"), *A, *B);
	}

	// === Binary function-call ops ===

	else if (ClassName == TEXT("MaterialExpressionMax"))
	{
		FString A = Fmt(TEXT("A")); if (A.IsEmpty()) A = TEXT("???");
		FString B = Fmt(TEXT("B")); if (B.IsEmpty()) B = TEXT("???");
		Result = FString::Printf(TEXT("Max(%s, %s)"), *A, *B);
	}
	else if (ClassName == TEXT("MaterialExpressionMin"))
	{
		FString A = Fmt(TEXT("A")); if (A.IsEmpty()) A = TEXT("???");
		FString B = Fmt(TEXT("B")); if (B.IsEmpty()) B = TEXT("???");
		Result = FString::Printf(TEXT("Min(%s, %s)"), *A, *B);
	}
	else if (ClassName == TEXT("MaterialExpressionPower"))
	{
		FString Base = Fmt(TEXT("Base"));
		FString Exp = Fmt(TEXT("Exponent"));
		if (Base.IsEmpty()) Base = TEXT("???");
		if (Exp.IsEmpty()) Exp = TEXT("???");
		Result = FString::Printf(TEXT("Pow(%s, %s)"), *Base, *Exp);
	}
	else if (ClassName == TEXT("MaterialExpressionDotProduct"))
	{
		FString A = Fmt(TEXT("A")); if (A.IsEmpty()) A = TEXT("???");
		FString B = Fmt(TEXT("B")); if (B.IsEmpty()) B = TEXT("???");
		Result = FString::Printf(TEXT("Dot(%s, %s)"), *A, *B);
	}
	else if (ClassName == TEXT("MaterialExpressionCrossProduct"))
	{
		FString A = Fmt(TEXT("A")); if (A.IsEmpty()) A = TEXT("???");
		FString B = Fmt(TEXT("B")); if (B.IsEmpty()) B = TEXT("???");
		Result = FString::Printf(TEXT("Cross(%s, %s)"), *A, *B);
	}
	else if (ClassName == TEXT("MaterialExpressionAppendVector"))
	{
		FString A = Fmt(TEXT("A")); if (A.IsEmpty()) A = TEXT("???");
		FString B = Fmt(TEXT("B")); if (B.IsEmpty()) B = TEXT("???");
		Result = FString::Printf(TEXT("Append(%s, %s)"), *A, *B);
	}

	// === Ternary / multi-input ops ===

	else if (ClassName == TEXT("MaterialExpressionLinearInterpolate"))
	{
		FString A = Fmt(TEXT("A")); if (A.IsEmpty()) A = TEXT("???");
		FString B = Fmt(TEXT("B")); if (B.IsEmpty()) B = TEXT("???");
		FString Alpha = Fmt(TEXT("Alpha")); if (Alpha.IsEmpty()) Alpha = TEXT("???");
		Result = FString::Printf(TEXT("Lerp(%s, %s, %s)"), *A, *B, *Alpha);
	}
	else if (ClassName == TEXT("MaterialExpressionClamp"))
	{
		FString Input = Fmt(TEXT("Input")); if (Input.IsEmpty()) Input = TEXT("???");
		FString Min = Fmt(TEXT("Min")); if (Min.IsEmpty()) Min = TEXT("0");
		FString Max = Fmt(TEXT("Max")); if (Max.IsEmpty()) Max = TEXT("1");
		Result = FString::Printf(TEXT("Clamp(%s, %s, %s)"), *Input, *Min, *Max);
	}
	else if (ClassName == TEXT("MaterialExpressionIf"))
	{
		FString A = Fmt(TEXT("A"));
		FString B = Fmt(TEXT("B"));
		FString Greater = Fmt(TEXT("A > B"));
		FString Less = Fmt(TEXT("A < B"));
		FString Equal = Fmt(TEXT("A == B"));
		Result = FString::Printf(TEXT("If(%s > %s, %s, %s)"), *A, *B, *Greater, *Less);
	}
	else if (ClassName == TEXT("MaterialExpressionFresnel"))
	{
		FString Exp = Fmt(TEXT("ExponentIn"));
		FString Base = Fmt(TEXT("BaseReflectFractionIn"));
		if (Exp.IsEmpty()) Exp = TEXT("5");
		if (Base.IsEmpty()) Base = TEXT("0.04");
		Result = FString::Printf(TEXT("Fresnel(%s, %s)"), *Exp, *Base);
	}
	else if (ClassName == TEXT("MaterialExpressionSphereMask"))
	{
		FString A = Fmt(TEXT("A"));
		FString B = Fmt(TEXT("B"));
		FString Radius = Fmt(TEXT("Radius"));
		FString Hardness = Fmt(TEXT("Hardness"));
		if (Hardness.IsEmpty()) Hardness = TEXT("256");
		Result = FString::Printf(TEXT("SphereMask(%s, %s, %s, %s)"), *A, *B, *Radius, *Hardness);
	}
	else if (ClassName == TEXT("MaterialExpressionDesaturation"))
	{
		FString Input = Fmt(TEXT("Input"));
		FString Fraction = Fmt(TEXT("Fraction"));
		if (Fraction.IsEmpty()) Fraction = TEXT("1");
		Result = FString::Printf(TEXT("Desaturate(%s, %s)"), *Input, *Fraction);
	}

	// === Built-in value nodes (no inputs) ===

	else if (ClassName == TEXT("MaterialExpressionTime")) { Result = TEXT("Time"); }
	else if (ClassName == TEXT("MaterialExpressionWorldPosition")) { Result = TEXT("WorldPos"); }
	else if (ClassName == TEXT("MaterialExpressionPreSkinnedPosition")) { Result = TEXT("PreSkinnedPos"); }
	else if (ClassName == TEXT("MaterialExpressionVertexNormalWS")) { Result = TEXT("VertexNormal"); }
	else if (ClassName == TEXT("MaterialExpressionPixelNormalWS")) { Result = TEXT("PixelNormal"); }
	else if (ClassName == TEXT("MaterialExpressionCameraPositionWS")) { Result = TEXT("CameraPos"); }
	else if (ClassName == TEXT("MaterialExpressionTextureCoordinate")) { Result = TEXT("UV"); }
	else if (ClassName == TEXT("MaterialExpressionObjectPositionWS")) { Result = TEXT("ObjectPos"); }
	else if (ClassName == TEXT("MaterialExpressionActorPositionWS")) { Result = TEXT("ActorPos"); }
	else if (ClassName == TEXT("MaterialExpressionViewSize")) { Result = TEXT("ViewSize"); }

	// === Custom HLSL ===

	else if (ClassName == TEXT("MaterialExpressionCustom"))
	{
		UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr);
		FString Desc = Custom->Description;
		if (Desc.IsEmpty()) Desc = TEXT("Custom");
		// Format inputs
		FString Inputs;
		int32 PinIdx = 0;
		for (FExpressionInputIterator It{ Expr }; It; ++It, ++PinIdx)
		{
			int32 ConnIdx = -1, ConnOut = 0;
			if (FindConnectedInput(Expr, PinIdx, Ctx.Expressions, ConnIdx, ConnOut))
			{
				if (!Inputs.IsEmpty()) Inputs += TEXT(", ");
				Inputs += FormatExpression(ConnIdx, ConnOut, Ctx);
			}
		}
		Result = FString::Printf(TEXT("HLSL(\"%s\"%s%s)"), *Desc,
			Inputs.IsEmpty() ? TEXT("") : TEXT(", "), *Inputs);
	}

	// === Fallback: unknown expression class ===
	else
	{
		// Strip "MaterialExpression" prefix for readability
		FString ShortName = ClassName;
		ShortName.RemoveFromStart(TEXT("MaterialExpression"));

		// Collect all connected inputs
		FString Inputs;
		int32 PinIdx = 0;
		for (FExpressionInputIterator It{ Expr }; It; ++It, ++PinIdx)
		{
			int32 ConnIdx = -1, ConnOut = 0;
			if (FindConnectedInput(Expr, PinIdx, Ctx.Expressions, ConnIdx, ConnOut))
			{
				if (!Inputs.IsEmpty()) Inputs += TEXT(", ");
				FString PinName = Expr->GetInputName(It.Index).ToString();
				Inputs += PinName + TEXT(": ") + FormatExpression(ConnIdx, ConnOut, Ctx);
			}
		}
		if (Inputs.IsEmpty())
		{
			Result = ShortName;
		}
		else
		{
			Result = FString::Printf(TEXT("%s(%s)"), *ShortName, *Inputs);
		}
	}

	// --- Handle variable assignment if this is a shared node (first encounter) ---
	if (Ctx.VarNames.Contains(Index))
	{
		const FString& VarName = Ctx.VarNames[Index];
		Ctx.Emitted.Add(Index);
		Ctx.VarSection += FString::Printf(TEXT("$%s = %s\n"), *VarName, *Result);
		return TEXT("$") + VarName + Suffix;
	}

	return Result + Suffix;
}

// ---------------------------------------------------------------------------
// BuildGraphPseudocode — entry point
// ---------------------------------------------------------------------------

FString FUnrealMCPMaterialCommands::BuildGraphPseudocode(UMaterial* Material) const
{
	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Material->GetExpressions();
	if (Expressions.Num() == 0) return FString();

	// --- Step 1: Count references ---
	TMap<int32, int32> RefCounts;
	for (int32 i = 0; i < Expressions.Num(); ++i)
	{
		RefCounts.Add(i, 0);
	}

	// Count refs from expression inputs
	for (int32 i = 0; i < Expressions.Num(); ++i)
	{
		UMaterialExpression* Expr = Expressions[i];
		if (!Expr) continue;

		for (FExpressionInputIterator It{ Expr }; It; ++It)
		{
			if (It->Expression)
			{
				for (int32 j = 0; j < Expressions.Num(); ++j)
				{
					if (Expressions[j] == It->Expression)
					{
						RefCounts[j]++;
						break;
					}
				}
			}
		}
	}

	// Material property list
	struct FPropertyEntry {
		const TCHAR* Name;
		EMaterialProperty Property;
	};
	const FPropertyEntry Properties[] = {
		{ TEXT("BaseColor"), MP_BaseColor },
		{ TEXT("Metallic"), MP_Metallic },
		{ TEXT("Specular"), MP_Specular },
		{ TEXT("Roughness"), MP_Roughness },
		{ TEXT("Normal"), MP_Normal },
		{ TEXT("EmissiveColor"), MP_EmissiveColor },
		{ TEXT("Opacity"), MP_Opacity },
		{ TEXT("OpacityMask"), MP_OpacityMask },
		{ TEXT("AmbientOcclusion"), MP_AmbientOcclusion },
		{ TEXT("WorldPositionOffset"), MP_WorldPositionOffset },
		{ TEXT("SubsurfaceColor"), MP_SubsurfaceColor },
	};

	// Count refs from property connections
	for (const FPropertyEntry& PropEntry : Properties)
	{
		FExpressionInput* Input = Material->GetExpressionInputForProperty(PropEntry.Property);
		if (Input && Input->Expression)
		{
			for (int32 j = 0; j < Expressions.Num(); ++j)
			{
				if (Expressions[j] == Input->Expression)
				{
					RefCounts[j]++;
					break;
				}
			}
		}
	}

	// --- Step 2: Assign variable names to shared nodes (ref_count > 1) ---
	TMap<int32, FString> VarNames;
	TSet<FString> UsedNames;

	for (auto& Pair : RefCounts)
	{
		if (Pair.Value <= 1) continue;

		int32 Idx = Pair.Key;
		UMaterialExpression* Expr = Expressions[Idx];
		if (!Expr) continue;

		FString Name;
		FString ClassName = Expr->GetClass()->GetName();

		// Try to derive a meaningful name
		if (UMaterialExpressionScalarParameter* SP = Cast<UMaterialExpressionScalarParameter>(Expr))
		{
			Name = SanitizeVarName(SP->ParameterName.ToString());
		}
		else if (UMaterialExpressionVectorParameter* VP = Cast<UMaterialExpressionVectorParameter>(Expr))
		{
			Name = SanitizeVarName(VP->ParameterName.ToString());
		}
		else if (UMaterialExpressionTextureSampleParameter2D* TP = Cast<UMaterialExpressionTextureSampleParameter2D>(Expr))
		{
			Name = SanitizeVarName(TP->ParameterName.ToString());
		}
		else if (UMaterialExpressionStaticSwitchParameter* SS = Cast<UMaterialExpressionStaticSwitchParameter>(Expr))
		{
			Name = SanitizeVarName(SS->ParameterName.ToString());
		}

		if (Name.IsEmpty())
		{
			// Fallback: short class name + index
			FString ShortClass = ClassName;
			ShortClass.RemoveFromStart(TEXT("MaterialExpression"));
			Name = SanitizeVarName(ShortClass) + FString::Printf(TEXT("_%d"), Idx);
		}

		// Deduplicate
		FString BaseName = Name;
		int32 Suffix = 2;
		while (UsedNames.Contains(Name))
		{
			Name = FString::Printf(TEXT("%s_%d"), *BaseName, Suffix++);
		}
		UsedNames.Add(Name);
		VarNames.Add(Idx, Name);
	}

	// --- Step 3: Build pseudocode ---
	FGraphFormatContext Ctx;
	Ctx.Expressions = Expressions;
	Ctx.VarNames = MoveTemp(VarNames);
	Ctx.RefCounts = MoveTemp(RefCounts);

	FString PropertySection;

	for (const FPropertyEntry& PropEntry : Properties)
	{
		FExpressionInput* Input = Material->GetExpressionInputForProperty(PropEntry.Property);
		if (!Input || !Input->Expression) continue;

		int32 ExprIdx = -1;
		for (int32 j = 0; j < Expressions.Num(); ++j)
		{
			if (Expressions[j] == Input->Expression)
			{
				ExprIdx = j;
				break;
			}
		}
		if (ExprIdx < 0) continue;

		FString ExprStr = FormatExpression(ExprIdx, Input->OutputIndex, Ctx);
		PropertySection += FString::Printf(TEXT("%s <- %s\n"), PropEntry.Name, *ExprStr);
	}

	// Combine: variable assignments first, then a blank line, then property assignments
	FString Output;
	if (!Ctx.VarSection.IsEmpty())
	{
		Output = Ctx.VarSection + TEXT("\n") + PropertySection;
	}
	else
	{
		Output = PropertySection;
	}

	// Trim trailing newline
	Output.TrimEndInline();
	return Output;
}
