#include "Commands/UnrealMCPDataAssetCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"

// UE core
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Engine/DataAsset.h"
#include "UObject/UObjectIterator.h"

FUnrealMCPDataAssetCommands::FUnrealMCPDataAssetCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPDataAssetCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("create_data_asset"))
	{
		return HandleCreateDataAsset(Params);
	}
	else if (CommandType == TEXT("create_asset"))
	{
		return HandleCreateAsset(Params);
	}
	else if (CommandType == TEXT("set_data_asset_property"))
	{
		return HandleSetDataAssetProperty(Params);
	}
	else if (CommandType == TEXT("get_data_asset_properties"))
	{
		return HandleGetDataAssetProperties(Params);
	}
	else if (CommandType == TEXT("get_array_element"))
	{
		return HandleGetArrayElement(Params);
	}
	else if (CommandType == TEXT("set_array_element"))
	{
		return HandleSetArrayElement(Params);
	}
	else if (CommandType == TEXT("add_array_element"))
	{
		return HandleAddArrayElement(Params);
	}
	else if (CommandType == TEXT("remove_array_element"))
	{
		return HandleRemoveArrayElement(Params);
	}
	else if (CommandType == TEXT("get_array_length"))
	{
		return HandleGetArrayLength(Params);
	}
	else if (CommandType == TEXT("import_property_text"))
	{
		return HandleImportPropertyText(Params);
	}

	return FUnrealMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unknown DataAsset command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPDataAssetCommands::HandleCreateDataAsset(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetName = Params->GetStringField(TEXT("asset_name"));
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString ParentClassName = Params->GetStringField(TEXT("parent_class"));

	if (AssetName.IsEmpty() || AssetPath.IsEmpty() || ParentClassName.IsEmpty())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing required fields: asset_name, asset_path, parent_class"));
	}

	// Check if asset already exists
	FString FullPath = AssetPath / AssetName;
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Asset already exists: %s"), *FullPath));
	}

	// Find the UClass — support both native C++ and AngelScript classes
	UClass* AssetClass = nullptr;

	// First try loading as a full path (e.g., "/Script/MyProject.UPerkDefinition")
	AssetClass = LoadObject<UClass>(nullptr, *ParentClassName);

	// If that fails, try finding by short name (e.g., "UPerkDefinition" or "PerkDefinition")
	if (!AssetClass)
	{
		AssetClass = FindFirstObject<UClass>(*ParentClassName, EFindFirstObjectOptions::NativeFirst);
	}

	// Try with 'U' prefix if not found
	if (!AssetClass && !ParentClassName.StartsWith(TEXT("U")))
	{
		FString WithPrefix = TEXT("U") + ParentClassName;
		AssetClass = FindFirstObject<UClass>(*WithPrefix, EFindFirstObjectOptions::NativeFirst);
	}

	// Try without 'U' prefix if not found
	if (!AssetClass && ParentClassName.StartsWith(TEXT("U")))
	{
		FString WithoutPrefix = ParentClassName.Mid(1);
		AssetClass = FindFirstObject<UClass>(*WithoutPrefix, EFindFirstObjectOptions::NativeFirst);
	}

	if (!AssetClass)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Could not find class: %s. Make sure AngelScript is loaded."), *ParentClassName),
			TEXT("Try class names like 'UPerkDefinition', 'PerkDefinition', or full path '/Script/Module.ClassName'"));
	}

	// Verify it's a UDataAsset subclass
	if (!AssetClass->IsChildOf(UDataAsset::StaticClass()))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Class %s is not a UDataAsset subclass"), *AssetClass->GetName()));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	// Create the asset via IAssetTools
	IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, AssetPath, AssetClass, nullptr);

	if (!NewAsset)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to create asset: %s/%s of class %s"), *AssetPath, *AssetName, *AssetClass->GetName()));
	}

	// Apply initial properties if provided
	const TSharedPtr<FJsonObject>* PropertiesObj = nullptr;
	if (Params->TryGetObjectField(TEXT("properties"), PropertiesObj))
	{
		for (const auto& Pair : (*PropertiesObj)->Values)
		{
			FString ErrorMsg;
			if (!FUnrealMCPCommonUtils::SetObjectProperty(NewAsset, Pair.Key, Pair.Value, ErrorMsg))
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to set property %s on new asset: %s"), *Pair.Key, *ErrorMsg);
			}
		}
	}

	// Save
	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(FullPath);
	}

	// Build response
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetBoolField(TEXT("success"), true);
	ResultData->SetStringField(TEXT("asset_path"), FullPath);
	ResultData->SetStringField(TEXT("class"), AssetClass->GetName());
	return ResultData;
}

TSharedPtr<FJsonObject> FUnrealMCPDataAssetCommands::HandleCreateAsset(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetName = Params->GetStringField(TEXT("asset_name"));
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString ClassName = Params->GetStringField(TEXT("asset_class"));

	if (AssetName.IsEmpty() || AssetPath.IsEmpty() || ClassName.IsEmpty())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing required fields: asset_name, asset_path, asset_class"));
	}

	// Check if asset already exists
	FString FullPath = AssetPath / AssetName;
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Asset already exists: %s"), *FullPath));
	}

	// Find the UClass
	UClass* AssetClass = nullptr;
	AssetClass = LoadObject<UClass>(nullptr, *ClassName);
	if (!AssetClass)
	{
		AssetClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::NativeFirst);
	}
	if (!AssetClass && !ClassName.StartsWith(TEXT("U")))
	{
		AssetClass = FindFirstObject<UClass>(*(TEXT("U") + ClassName), EFindFirstObjectOptions::NativeFirst);
	}
	if (!AssetClass && ClassName.StartsWith(TEXT("U")))
	{
		AssetClass = FindFirstObject<UClass>(*ClassName.Mid(1), EFindFirstObjectOptions::NativeFirst);
	}

	if (!AssetClass)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Could not find class: %s"), *ClassName));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	// Create via IAssetTools (handles factory lookup automatically)
	IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, AssetPath, AssetClass, nullptr);

	if (!NewAsset)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to create asset: %s/%s of class %s. The engine may not have a factory for this type."),
				*AssetPath, *AssetName, *AssetClass->GetName()));
	}

	// Apply initial properties if provided
	const TSharedPtr<FJsonObject>* PropertiesObj = nullptr;
	if (Params->TryGetObjectField(TEXT("properties"), PropertiesObj))
	{
		for (const auto& Pair : (*PropertiesObj)->Values)
		{
			FString ErrorMsg;
			if (!FUnrealMCPCommonUtils::SetObjectProperty(NewAsset, Pair.Key, Pair.Value, ErrorMsg))
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to set property %s on new asset: %s"), *Pair.Key, *ErrorMsg);
			}
		}
	}

	// Save
	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(FullPath);
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetBoolField(TEXT("success"), true);
	ResultData->SetStringField(TEXT("asset_path"), FullPath);
	ResultData->SetStringField(TEXT("class"), AssetClass->GetName());
	return ResultData;
}

TSharedPtr<FJsonObject> FUnrealMCPDataAssetCommands::HandleSetDataAssetProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString PropertyName = Params->GetStringField(TEXT("property_name"));

	if (AssetPath.IsEmpty() || PropertyName.IsEmpty())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing required fields: asset_path, property_name"));
	}

	if (!Params->HasField(TEXT("property_value")))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required field: property_value"));
	}

	// Load the asset
	UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
	if (!Asset)
	{
		// Try with full object path
		Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	}
	if (!Asset)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Could not load asset: %s"), *AssetPath));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	// Set the property
	TSharedPtr<FJsonValue> PropertyValue = Params->TryGetField(TEXT("property_value"));
	FString ErrorMsg;
	if (!FUnrealMCPCommonUtils::SetObjectProperty(Asset, PropertyName, PropertyValue, ErrorMsg))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to set property %s: %s"), *PropertyName, *ErrorMsg));
	}

	// Mark dirty and save
	Asset->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(AssetPath);
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetBoolField(TEXT("success"), true);
	ResultData->SetStringField(TEXT("property"), PropertyName);
	return ResultData;
}

TSharedPtr<FJsonObject> FUnrealMCPDataAssetCommands::HandleGetDataAssetProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	if (AssetPath.IsEmpty())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required field: asset_path"));
	}

	// Load the asset
	UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
	if (!Asset)
	{
		Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	}
	if (!Asset)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Could not load asset: %s"), *AssetPath));
	}

	// Read all properties from the asset's class (skip UObject base properties)
	TSharedPtr<FJsonObject> PropertiesObj = MakeShared<FJsonObject>();
	UClass* AssetClass = Asset->GetClass();

	// Determine base class to skip: UDataAsset if it's a DataAsset, otherwise UObject
	UClass* BaseClass = AssetClass->IsChildOf(UDataAsset::StaticClass())
		? UDataAsset::StaticClass()
		: UObject::StaticClass();

	for (TFieldIterator<FProperty> It(AssetClass); It; ++It)
	{
		FProperty* Property = *It;

		// Skip properties from the base class and UObject itself
		UClass* OwnerClass = Property->GetOwnerClass();
		if (OwnerClass == UObject::StaticClass() || OwnerClass == BaseClass)
		{
			continue;
		}

		TSharedPtr<FJsonValue> JsonValue = FUnrealMCPCommonUtils::GetPropertyAsJson(Property, Asset);
		PropertiesObj->SetField(Property->GetName(), JsonValue);
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetBoolField(TEXT("success"), true);
	ResultData->SetStringField(TEXT("asset_path"), AssetPath);
	ResultData->SetStringField(TEXT("class"), AssetClass->GetName());
	ResultData->SetObjectField(TEXT("properties"), PropertiesObj);
	return ResultData;
}

// ============================================================================
// Shared helper: resolve asset + array property
// ============================================================================

bool FUnrealMCPDataAssetCommands::ResolveArrayProperty(
	const TSharedPtr<FJsonObject>& Params, UObject*& OutObject,
	FArrayProperty*& OutArrayProp, void*& OutArrayAddr, FString& OutError)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString PropertyName = Params->GetStringField(TEXT("property_name"));

	if (AssetPath.IsEmpty() || PropertyName.IsEmpty())
	{
		OutError = TEXT("Missing required fields: asset_path, property_name");
		return false;
	}

	OutObject = LoadObject<UObject>(nullptr, *AssetPath);
	if (!OutObject)
	{
		OutObject = UEditorAssetLibrary::LoadAsset(AssetPath);
	}
	if (!OutObject)
	{
		OutError = FString::Printf(TEXT("Could not load asset: %s"), *AssetPath);
		return false;
	}

	FProperty* Property = OutObject->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		OutError = FString::Printf(TEXT("Property '%s' not found on %s"), *PropertyName, *OutObject->GetClass()->GetName());
		return false;
	}

	OutArrayProp = CastField<FArrayProperty>(Property);
	if (!OutArrayProp)
	{
		OutError = FString::Printf(TEXT("Property '%s' is not an array (type: %s)"), *PropertyName, *Property->GetClass()->GetName());
		return false;
	}

	OutArrayAddr = OutArrayProp->ContainerPtrToValuePtr<void>(OutObject);
	return true;
}

// ============================================================================
// get_array_element
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPDataAssetCommands::HandleGetArrayElement(const TSharedPtr<FJsonObject>& Params)
{
	UObject* Asset = nullptr;
	FArrayProperty* ArrayProp = nullptr;
	void* ArrayAddr = nullptr;
	FString Error;

	if (!ResolveArrayProperty(Params, Asset, ArrayProp, ArrayAddr, Error))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	int32 Index = static_cast<int32>(Params->GetNumberField(TEXT("index")));
	FScriptArrayHelper ArrayHelper(ArrayProp, ArrayAddr);

	if (Index < 0 || Index >= ArrayHelper.Num())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Index %d out of bounds (array length: %d)"), Index, ArrayHelper.Num()));
	}

	const void* ElemAddr = ArrayHelper.GetRawPtr(Index);
	TSharedPtr<FJsonValue> ElemJson = FUnrealMCPCommonUtils::GetPropertyValueAsJson(ArrayProp->Inner, ElemAddr);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetBoolField(TEXT("success"), true);
	ResultData->SetNumberField(TEXT("index"), Index);
	ResultData->SetField(TEXT("element"), ElemJson);
	return ResultData;
}

// ============================================================================
// set_array_element
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPDataAssetCommands::HandleSetArrayElement(const TSharedPtr<FJsonObject>& Params)
{
	UObject* Asset = nullptr;
	FArrayProperty* ArrayProp = nullptr;
	void* ArrayAddr = nullptr;
	FString Error;

	if (!ResolveArrayProperty(Params, Asset, ArrayProp, ArrayAddr, Error))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	if (!Params->HasField(TEXT("value")))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required field: value"));
	}

	int32 Index = static_cast<int32>(Params->GetNumberField(TEXT("index")));
	FScriptArrayHelper ArrayHelper(ArrayProp, ArrayAddr);

	if (Index < 0 || Index >= ArrayHelper.Num())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Index %d out of bounds (array length: %d)"), Index, ArrayHelper.Num()));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
	void* ElemAddr = ArrayHelper.GetRawPtr(Index);

	FString SetError;
	if (!FUnrealMCPCommonUtils::SetPropertyValue(ArrayProp->Inner, ElemAddr, Value, SetError))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to set element %d: %s"), Index, *SetError));
	}

	Asset->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		FString AssetPath = Params->GetStringField(TEXT("asset_path"));
		UEditorAssetLibrary::SaveAsset(AssetPath);
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetBoolField(TEXT("success"), true);
	ResultData->SetNumberField(TEXT("index"), Index);
	return ResultData;
}

// ============================================================================
// add_array_element
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPDataAssetCommands::HandleAddArrayElement(const TSharedPtr<FJsonObject>& Params)
{
	UObject* Asset = nullptr;
	FArrayProperty* ArrayProp = nullptr;
	void* ArrayAddr = nullptr;
	FString Error;

	if (!ResolveArrayProperty(Params, Asset, ArrayProp, ArrayAddr, Error))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	if (!Params->HasField(TEXT("value")))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required field: value"));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	FScriptArrayHelper ArrayHelper(ArrayProp, ArrayAddr);
	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));

	// Support both single value and array of values
	TArray<TSharedPtr<FJsonValue>> ValuesToAdd;
	const TArray<TSharedPtr<FJsonValue>>* AsArray = nullptr;
	if (Value->TryGetArray(AsArray))
	{
		ValuesToAdd = *AsArray;
	}
	else
	{
		ValuesToAdd.Add(Value);
	}

	TArray<TSharedPtr<FJsonValue>> NewIndices;
	for (const auto& ElemValue : ValuesToAdd)
	{
		int32 NewIndex = ArrayHelper.AddValue();
		void* ElemAddr = ArrayHelper.GetRawPtr(NewIndex);

		FString SetError;
		if (!FUnrealMCPCommonUtils::SetPropertyValue(ArrayProp->Inner, ElemAddr, ElemValue, SetError))
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Failed to add element at index %d: %s"), NewIndex, *SetError));
		}
		NewIndices.Add(MakeShared<FJsonValueNumber>(NewIndex));
	}

	Asset->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		FString AssetPath = Params->GetStringField(TEXT("asset_path"));
		UEditorAssetLibrary::SaveAsset(AssetPath);
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetBoolField(TEXT("success"), true);
	ResultData->SetField(TEXT("new_indices"), MakeShared<FJsonValueArray>(NewIndices));
	ResultData->SetNumberField(TEXT("new_length"), ArrayHelper.Num());
	return ResultData;
}

// ============================================================================
// remove_array_element
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPDataAssetCommands::HandleRemoveArrayElement(const TSharedPtr<FJsonObject>& Params)
{
	UObject* Asset = nullptr;
	FArrayProperty* ArrayProp = nullptr;
	void* ArrayAddr = nullptr;
	FString Error;

	if (!ResolveArrayProperty(Params, Asset, ArrayProp, ArrayAddr, Error))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	int32 Index = static_cast<int32>(Params->GetNumberField(TEXT("index")));
	FScriptArrayHelper ArrayHelper(ArrayProp, ArrayAddr);

	if (Index < 0 || Index >= ArrayHelper.Num())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Index %d out of bounds (array length: %d)"), Index, ArrayHelper.Num()));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	ArrayHelper.RemoveValues(Index, 1);
	Asset->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		FString AssetPath = Params->GetStringField(TEXT("asset_path"));
		UEditorAssetLibrary::SaveAsset(AssetPath);
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetBoolField(TEXT("success"), true);
	ResultData->SetNumberField(TEXT("removed_index"), Index);
	ResultData->SetNumberField(TEXT("new_length"), ArrayHelper.Num());
	return ResultData;
}

// ============================================================================
// get_array_length
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPDataAssetCommands::HandleGetArrayLength(const TSharedPtr<FJsonObject>& Params)
{
	UObject* Asset = nullptr;
	FArrayProperty* ArrayProp = nullptr;
	void* ArrayAddr = nullptr;
	FString Error;

	if (!ResolveArrayProperty(Params, Asset, ArrayProp, ArrayAddr, Error))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FScriptArrayHelper ArrayHelper(ArrayProp, ArrayAddr);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetBoolField(TEXT("success"), true);
	ResultData->SetNumberField(TEXT("length"), ArrayHelper.Num());
	return ResultData;
}

TSharedPtr<FJsonObject> FUnrealMCPDataAssetCommands::HandleImportPropertyText(const TSharedPtr<FJsonObject>& Params)
{
	// import_property_text: Use FProperty::ImportText to set a property from its T3D text representation.
	// This handles complex types like TArray<FInstancedStruct> that can't be set via JSON.
	//
	// Params:
	//   asset_path: Full content path (e.g., "/Game/Path/MyAsset")
	//   property_name: Property name (e.g., "ColumnsStructs")
	//   text: The T3D text value for the property
	//   sub_object_path: Optional sub-object path within the asset (e.g., "AssetName:SubObjectName")
	//   save: Whether to save after (default true)

	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString PropertyName = Params->GetStringField(TEXT("property_name"));
	FString TextValue = Params->GetStringField(TEXT("text"));
	FString SubObjectPath = Params->GetStringField(TEXT("sub_object_path"));
	bool bSave = !Params->HasField(TEXT("save")) || Params->GetBoolField(TEXT("save"));

	if (AssetPath.IsEmpty() || PropertyName.IsEmpty() || TextValue.IsEmpty())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing required fields: asset_path, property_name, text"));
	}

	UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
	if (!Asset)
	{
		// Try with .AssetName suffix
		FString FullPath = AssetPath + TEXT(".") + FPackageName::GetShortName(AssetPath);
		Asset = LoadObject<UObject>(nullptr, *FullPath);
	}
	if (!Asset)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Could not load asset: %s"), *AssetPath));
	}

	// If sub_object_path is specified, find the sub-object
	UObject* TargetObject = Asset;
	if (!SubObjectPath.IsEmpty())
	{
		FString FullSubPath = Asset->GetPathName() + TEXT(":") + SubObjectPath;
		TargetObject = FindObject<UObject>(nullptr, *FullSubPath);
		if (!TargetObject)
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Could not find sub-object: %s"), *FullSubPath));
		}
	}

	// Find the property
	FProperty* Property = TargetObject->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Property '%s' not found on %s"), *PropertyName, *TargetObject->GetClass()->GetName()));
	}

	// Get the property address
	void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(TargetObject);

	// PreEditChange
	TargetObject->PreEditChange(Property);

	// Import the text
	const TCHAR* Buffer = *TextValue;
	const TCHAR* Result = Property->ImportText_Direct(Buffer, PropertyAddr, TargetObject, PPF_None);

	if (!Result)
	{
		TargetObject->PostEditChange();
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("ImportText failed for property '%s'"), *PropertyName));
	}

	// PostEditChange
	FPropertyChangedEvent ChangedEvent(Property);
	TargetObject->PostEditChangeProperty(ChangedEvent);

	// Mark dirty
	TargetObject->MarkPackageDirty();
	if (Asset != TargetObject)
	{
		Asset->MarkPackageDirty();
	}

	// Save if requested
	if (bSave)
	{
		UEditorAssetLibrary::SaveLoadedAsset(Asset);
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetBoolField(TEXT("success"), true);
	ResultData->SetStringField(TEXT("message"),
		FString::Printf(TEXT("Property '%s' imported successfully on %s"), *PropertyName, *TargetObject->GetName()));
	return ResultData;
}
