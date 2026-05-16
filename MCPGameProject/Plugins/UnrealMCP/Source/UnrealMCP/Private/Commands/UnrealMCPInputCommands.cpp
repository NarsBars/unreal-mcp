#include "Commands/UnrealMCPInputCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"

// UE core
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"

// Enhanced Input
#include "InputAction.h"
#include "InputMappingContext.h"
#include "EnhancedActionKeyMapping.h"

FUnrealMCPInputCommands::FUnrealMCPInputCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPInputCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("create_input_action"))
	{
		return HandleCreateInputAction(Params);
	}
	else if (CommandType == TEXT("create_input_mapping_context"))
	{
		return HandleCreateInputMappingContext(Params);
	}
	else if (CommandType == TEXT("add_input_mapping"))
	{
		return HandleAddInputMapping(Params);
	}
	else if (CommandType == TEXT("get_input_info"))
	{
		return HandleGetInputInfo(Params);
	}

	return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown input command: %s"), *CommandType));
}

// ---------------------------------------------------------------------------
// create_input_action
// Params: { "name", "path", "value_type" (Digital/Axis1D/Axis2D/Axis3D), "save" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPInputCommands::HandleCreateInputAction(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	FString Path = TEXT("/Game/Input");
	Params->TryGetStringField(TEXT("path"), Path);

	FString FullPath = Path / Name;
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("InputAction already exists: %s"), *FullPath));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(Name, Path, UInputAction::StaticClass(), nullptr);

	UInputAction* InputAction = Cast<UInputAction>(NewAsset);
	if (!InputAction)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to create InputAction: %s"), *FullPath));
	}

	// Set value type
	FString ValueTypeStr;
	if (Params->TryGetStringField(TEXT("value_type"), ValueTypeStr))
	{
		if (ValueTypeStr.Equals(TEXT("Digital"), ESearchCase::IgnoreCase) || ValueTypeStr.Equals(TEXT("bool"), ESearchCase::IgnoreCase))
		{
			InputAction->ValueType = EInputActionValueType::Boolean;
		}
		else if (ValueTypeStr.Equals(TEXT("Axis1D"), ESearchCase::IgnoreCase) || ValueTypeStr.Equals(TEXT("float"), ESearchCase::IgnoreCase))
		{
			InputAction->ValueType = EInputActionValueType::Axis1D;
		}
		else if (ValueTypeStr.Equals(TEXT("Axis2D"), ESearchCase::IgnoreCase) || ValueTypeStr.Equals(TEXT("2d"), ESearchCase::IgnoreCase))
		{
			InputAction->ValueType = EInputActionValueType::Axis2D;
		}
		else if (ValueTypeStr.Equals(TEXT("Axis3D"), ESearchCase::IgnoreCase) || ValueTypeStr.Equals(TEXT("3d"), ESearchCase::IgnoreCase))
		{
			InputAction->ValueType = EInputActionValueType::Axis3D;
		}
	}

	FAssetRegistryModule::AssetCreated(InputAction);
	InputAction->GetOutermost()->MarkPackageDirty();

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
	Result->SetStringField(TEXT("name"), Name);
	Result->SetStringField(TEXT("path"), FullPath);
	Result->SetStringField(TEXT("type"), TEXT("InputAction"));
	Result->SetStringField(TEXT("value_type"), UEnum::GetValueAsString(InputAction->ValueType));
	return Result;
}

// ---------------------------------------------------------------------------
// create_input_mapping_context
// Params: { "name", "path", "save" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPInputCommands::HandleCreateInputMappingContext(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	FString Path = TEXT("/Game/Input");
	Params->TryGetStringField(TEXT("path"), Path);

	FString FullPath = Path / Name;
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("InputMappingContext already exists: %s"), *FullPath));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(Name, Path, UInputMappingContext::StaticClass(), nullptr);

	UInputMappingContext* Context = Cast<UInputMappingContext>(NewAsset);
	if (!Context)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to create InputMappingContext: %s"), *FullPath));
	}

	FAssetRegistryModule::AssetCreated(Context);
	Context->GetOutermost()->MarkPackageDirty();

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
	Result->SetStringField(TEXT("name"), Name);
	Result->SetStringField(TEXT("path"), FullPath);
	Result->SetStringField(TEXT("type"), TEXT("InputMappingContext"));
	return Result;
}

// ---------------------------------------------------------------------------
// add_input_mapping
// Params: { "context_path", "action_path", "key", "save" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPInputCommands::HandleAddInputMapping(const TSharedPtr<FJsonObject>& Params)
{
	FString ContextPath;
	if (!Params->TryGetStringField(TEXT("context_path"), ContextPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'context_path' parameter"));
	}

	FString ActionPath;
	if (!Params->TryGetStringField(TEXT("action_path"), ActionPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'action_path' parameter"));
	}

	FString KeyName;
	if (!Params->TryGetStringField(TEXT("key"), KeyName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'key' parameter"));
	}

	UInputMappingContext* Context = Cast<UInputMappingContext>(UEditorAssetLibrary::LoadAsset(ContextPath));
	if (!Context)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Could not load InputMappingContext: %s"), *ContextPath));
	}

	UInputAction* Action = Cast<UInputAction>(UEditorAssetLibrary::LoadAsset(ActionPath));
	if (!Action)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Could not load InputAction: %s"), *ActionPath));
	}

	FKey MappedKey{FName{*KeyName}};
	if (!MappedKey.IsValid())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid key name: %s"), *KeyName));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, MappedKey);
	Context->GetOutermost()->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(ContextPath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("context_path"), ContextPath);
	Result->SetStringField(TEXT("action_path"), ActionPath);
	Result->SetStringField(TEXT("key"), KeyName);
	Result->SetNumberField(TEXT("total_mappings"), Context->GetMappings().Num());
	return Result;
}

// ---------------------------------------------------------------------------
// get_input_info
// Params: { "asset_path" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPInputCommands::HandleGetInputInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
	}

	if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset does not exist: %s"), *AssetPath));
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!Asset)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("asset_name"), Asset->GetName());
	Result->SetStringField(TEXT("asset_class"), Asset->GetClass()->GetName());

	if (UInputAction* InputAction = Cast<UInputAction>(Asset))
	{
		Result->SetStringField(TEXT("type"), TEXT("InputAction"));
		Result->SetStringField(TEXT("value_type"), UEnum::GetValueAsString(InputAction->ValueType));
		Result->SetBoolField(TEXT("consume_input"), InputAction->bConsumeInput);
	}
	else if (UInputMappingContext* Context = Cast<UInputMappingContext>(Asset))
	{
		Result->SetStringField(TEXT("type"), TEXT("InputMappingContext"));
		Result->SetNumberField(TEXT("mapping_count"), Context->GetMappings().Num());

		TArray<TSharedPtr<FJsonValue>> MappingsArray;
		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			TSharedPtr<FJsonObject> MappingObj = MakeShared<FJsonObject>();
			MappingObj->SetStringField(TEXT("action"), Mapping.Action ? Mapping.Action->GetPathName() : TEXT("None"));
			MappingObj->SetStringField(TEXT("key"), Mapping.Key.GetFName().ToString());
			MappingsArray.Add(MakeShared<FJsonValueObject>(MappingObj));
		}
		Result->SetArrayField(TEXT("mappings"), MappingsArray);
	}
	else
	{
		Result->SetStringField(TEXT("type"), TEXT("Unknown"));
	}

	return Result;
}
