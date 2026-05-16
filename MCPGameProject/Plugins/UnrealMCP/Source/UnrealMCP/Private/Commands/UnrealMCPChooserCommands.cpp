#include "Commands/UnrealMCPChooserCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"

// UE core
#include "EditorAssetLibrary.h"

// Chooser plugin
#include "Chooser.h"
#include "MultiEnumColumn.h"
#include "EnumColumn.h"
#include "BoolColumn.h"
#include "FloatRangeColumn.h"
#include "GameplayTagColumn.h"
#include "OutputStructColumn.h"
#include "IChooserColumn.h"
#include "IChooserParameterBase.h"

FUnrealMCPChooserCommands::FUnrealMCPChooserCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPChooserCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("read_chooser_table"))
	{
		return HandleReadChooserTable(Params);
	}
	else if (CommandType == TEXT("set_chooser_column_value"))
	{
		return HandleSetChooserColumnValue(Params);
	}

	return FUnrealMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unknown Chooser command: %s"), *CommandType));
}

// ── Helpers ──────────────────────────────────────────────────────────────────

namespace ChooserHelpers
{
	/**
	 * Load a UChooserTable asset and optionally navigate into a nested sub-table.
	 * SubTablePath is dot-separated (e.g. "Stand Walks.Stand Walks F").
	 * Returns nullptr and sets OutError on failure.
	 */
	static UChooserTable* LoadAndNavigate(const FString& AssetPath, const FString& SubTablePath, FString& OutError)
	{
		UObject* LoadedObj = UEditorAssetLibrary::LoadAsset(AssetPath);
		if (!LoadedObj)
		{
			OutError = FString::Printf(TEXT("Could not load asset: %s"), *AssetPath);
			return nullptr;
		}

		UChooserTable* Table = Cast<UChooserTable>(LoadedObj);
		if (!Table)
		{
			OutError = FString::Printf(TEXT("Asset is not a UChooserTable: %s (class: %s)"),
				*AssetPath, *LoadedObj->GetClass()->GetName());
			return nullptr;
		}

		// Navigate into nested sub-tables if requested
		if (!SubTablePath.IsEmpty())
		{
#if WITH_EDITORONLY_DATA
			TArray<FString> Parts;
			SubTablePath.ParseIntoArray(Parts, TEXT("."));

			for (const FString& Part : Parts)
			{
				UChooserTable* Found = nullptr;

				// Search NestedChoosers first (the preferred array in newer engine versions)
				for (UChooserTable* Nested : Table->NestedChoosers)
				{
					if (Nested && Nested->GetName() == Part)
					{
						Found = Nested;
						break;
					}
				}

				// Fallback: search NestedObjects for UChooserTable*
				if (!Found)
				{
					for (UObject* Obj : Table->NestedObjects)
					{
						UChooserTable* AsChooser = Cast<UChooserTable>(Obj);
						if (AsChooser && AsChooser->GetName() == Part)
						{
							Found = AsChooser;
							break;
						}
					}
				}

				if (!Found)
				{
					OutError = FString::Printf(TEXT("Nested sub-table not found: '%s' in table '%s'"),
						*Part, *Table->GetName());
					return nullptr;
				}
				Table = Found;
			}
#else
			OutError = TEXT("Sub-table navigation requires editor-only data (WITH_EDITORONLY_DATA)");
			return nullptr;
#endif
		}

		return Table;
	}

	/**
	 * Extract the binding display name from a column's InputValue FInstancedStruct.
	 * Returns the last element of PropertyBindingChain, or the debug name from the parameter.
	 */
	static FString GetBindingName(const FInstancedStruct& InputValue)
	{
		if (!InputValue.IsValid())
		{
			return TEXT("");
		}

		const FChooserParameterBase* ParamBase = InputValue.GetPtr<FChooserParameterBase>();
		if (ParamBase)
		{
			return ParamBase->GetDebugName();
		}

		return TEXT("");
	}

	/** Detect column type name from the FInstancedStruct script struct. */
	static FString GetColumnTypeName(const FInstancedStruct& ColStruct)
	{
		if (!ColStruct.IsValid()) return TEXT("Unknown");

		if (ColStruct.GetPtr<FMultiEnumColumn>())    return TEXT("MultiEnum");
		if (ColStruct.GetPtr<FEnumColumn>())          return TEXT("Enum");
		if (ColStruct.GetPtr<FBoolColumn>())          return TEXT("Bool");
		if (ColStruct.GetPtr<FFloatRangeColumn>())    return TEXT("FloatRange");
		if (ColStruct.GetPtr<FGameplayTagColumn>())   return TEXT("GameplayTag");
		if (ColStruct.GetPtr<FOutputStructColumn>())  return TEXT("OutputStruct");

		// Fallback: use the struct name
		const UScriptStruct* Struct = ColStruct.GetScriptStruct();
		if (Struct)
		{
			return Struct->GetName();
		}
		return TEXT("Unknown");
	}

	/** Serialize column row values to a JSON array based on column type. */
	static TArray<TSharedPtr<FJsonValue>> SerializeRowValues(const FInstancedStruct& ColStruct, int32 RowCount)
	{
		TArray<TSharedPtr<FJsonValue>> RowArray;

		if (const FMultiEnumColumn* MultiCol = ColStruct.GetPtr<FMultiEnumColumn>())
		{
			for (int32 i = 0; i < RowCount && i < MultiCol->RowValues.Num(); ++i)
			{
				// Bitmask as integer
				RowArray.Add(MakeShared<FJsonValueNumber>(static_cast<double>(MultiCol->RowValues[i].Value)));
			}
		}
		else if (const FEnumColumn* EnumCol = ColStruct.GetPtr<FEnumColumn>())
		{
			for (int32 i = 0; i < RowCount && i < EnumCol->RowValues.Num(); ++i)
			{
				TSharedPtr<FJsonObject> RowObj = MakeShared<FJsonObject>();
				RowObj->SetNumberField(TEXT("value"), static_cast<double>(EnumCol->RowValues[i].Value));
				FString CompStr;
				switch (EnumCol->RowValues[i].Comparison)
				{
				case EEnumColumnCellValueComparison::MatchEqual:    CompStr = TEXT("MatchEqual"); break;
				case EEnumColumnCellValueComparison::MatchNotEqual: CompStr = TEXT("MatchNotEqual"); break;
				case EEnumColumnCellValueComparison::MatchAny:      CompStr = TEXT("MatchAny"); break;
				default:                                            CompStr = TEXT("Unknown"); break;
				}
				RowObj->SetStringField(TEXT("comparison"), CompStr);
#if WITH_EDITORONLY_DATA
				RowObj->SetStringField(TEXT("value_name"), EnumCol->RowValues[i].ValueName.ToString());
#endif
				RowArray.Add(MakeShared<FJsonValueObject>(RowObj));
			}
		}
		else if (const FBoolColumn* BoolCol = ColStruct.GetPtr<FBoolColumn>())
		{
			for (int32 i = 0; i < RowCount && i < BoolCol->RowValuesWithAny.Num(); ++i)
			{
				FString BoolStr;
				switch (BoolCol->RowValuesWithAny[i])
				{
				case EBoolColumnCellValue::MatchFalse: BoolStr = TEXT("MatchFalse"); break;
				case EBoolColumnCellValue::MatchTrue:  BoolStr = TEXT("MatchTrue"); break;
				case EBoolColumnCellValue::MatchAny:   BoolStr = TEXT("MatchAny"); break;
				default:                               BoolStr = TEXT("Unknown"); break;
				}
				RowArray.Add(MakeShared<FJsonValueString>(BoolStr));
			}
		}
		else if (const FFloatRangeColumn* FloatCol = ColStruct.GetPtr<FFloatRangeColumn>())
		{
			for (int32 i = 0; i < RowCount && i < FloatCol->RowValues.Num(); ++i)
			{
				TSharedPtr<FJsonObject> RowObj = MakeShared<FJsonObject>();
				RowObj->SetNumberField(TEXT("min"), FloatCol->RowValues[i].Min);
				RowObj->SetNumberField(TEXT("max"), FloatCol->RowValues[i].Max);
				RowObj->SetBoolField(TEXT("no_min"), FloatCol->RowValues[i].bNoMin);
				RowObj->SetBoolField(TEXT("no_max"), FloatCol->RowValues[i].bNoMax);
				RowArray.Add(MakeShared<FJsonValueObject>(RowObj));
			}
		}
		else if (const FGameplayTagColumn* TagCol = ColStruct.GetPtr<FGameplayTagColumn>())
		{
			for (int32 i = 0; i < RowCount && i < TagCol->RowValues.Num(); ++i)
			{
				RowArray.Add(MakeShared<FJsonValueString>(TagCol->RowValues[i].ToStringSimple()));
			}
		}
		else if (const FOutputStructColumn* StructCol = ColStruct.GetPtr<FOutputStructColumn>())
		{
			for (int32 i = 0; i < RowCount && i < StructCol->RowValues.Num(); ++i)
			{
				// Export the InstancedStruct to text for each row
				const FInstancedStruct& RowVal = StructCol->RowValues[i];
				if (RowVal.IsValid())
				{
					FString ExportText;
					const UScriptStruct* RowStruct = RowVal.GetScriptStruct();
					if (RowStruct)
					{
						RowStruct->ExportText(ExportText, RowVal.GetMemory(), nullptr, nullptr, PPF_None, nullptr);
					}
					TSharedPtr<FJsonObject> RowObj = MakeShared<FJsonObject>();
					RowObj->SetStringField(TEXT("struct_type"), RowStruct ? RowStruct->GetName() : TEXT("None"));
					RowObj->SetStringField(TEXT("export_text"), ExportText);
					RowArray.Add(MakeShared<FJsonValueObject>(RowObj));
				}
				else
				{
					RowArray.Add(MakeShared<FJsonValueNull>());
				}
			}
		}
		// For unknown column types, return empty array (row_values will be [])

		return RowArray;
	}

	/** Get the InputValue FInstancedStruct from a column, if it has one. */
	static const FInstancedStruct* GetColumnInputValue(const FInstancedStruct& ColStruct)
	{
		if (const FMultiEnumColumn* Col = ColStruct.GetPtr<FMultiEnumColumn>())   return &Col->InputValue;
		if (const FEnumColumn* Col = ColStruct.GetPtr<FEnumColumn>())             return &Col->InputValue;
		if (const FBoolColumn* Col = ColStruct.GetPtr<FBoolColumn>())             return &Col->InputValue;
		if (const FFloatRangeColumn* Col = ColStruct.GetPtr<FFloatRangeColumn>()) return &Col->InputValue;
		if (const FGameplayTagColumn* Col = ColStruct.GetPtr<FGameplayTagColumn>()) return &Col->InputValue;
		if (const FOutputStructColumn* Col = ColStruct.GetPtr<FOutputStructColumn>()) return &Col->InputValue;
		return nullptr;
	}

	/** Get the row count from a table. Uses ColumnsStructs[0] row count as reference, or ResultsStructs. */
	static int32 GetRowCount(const UChooserTable* Table)
	{
#if WITH_EDITORONLY_DATA
		return Table->ResultsStructs.Num();
#else
		// Fallback: try to infer from column row arrays
		for (const FInstancedStruct& Col : Table->ColumnsStructs)
		{
			if (const FMultiEnumColumn* C = Col.GetPtr<FMultiEnumColumn>()) return C->RowValues.Num();
			if (const FEnumColumn* C = Col.GetPtr<FEnumColumn>())           return C->RowValues.Num();
			if (const FBoolColumn* C = Col.GetPtr<FBoolColumn>())           return C->RowValuesWithAny.Num();
			if (const FFloatRangeColumn* C = Col.GetPtr<FFloatRangeColumn>()) return C->RowValues.Num();
			if (const FGameplayTagColumn* C = Col.GetPtr<FGameplayTagColumn>()) return C->RowValues.Num();
		}
		return 0;
#endif
	}

} // namespace ChooserHelpers

// ── read_chooser_table ───────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FUnrealMCPChooserCommands::HandleReadChooserTable(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	if (AssetPath.IsEmpty())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required field: asset_path"));
	}

	FString SubTable;
	Params->TryGetStringField(TEXT("sub_table"), SubTable);

	FString LoadError;
	UChooserTable* Table = ChooserHelpers::LoadAndNavigate(AssetPath, SubTable, LoadError);
	if (!Table)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	const int32 RowCount = ChooserHelpers::GetRowCount(Table);

	// Build columns array
	TArray<TSharedPtr<FJsonValue>> ColumnsArray;
	for (int32 ColIdx = 0; ColIdx < Table->ColumnsStructs.Num(); ++ColIdx)
	{
		const FInstancedStruct& ColStruct = Table->ColumnsStructs[ColIdx];
		TSharedPtr<FJsonObject> ColObj = MakeShared<FJsonObject>();

		ColObj->SetNumberField(TEXT("index"), ColIdx);
		ColObj->SetStringField(TEXT("type"), ChooserHelpers::GetColumnTypeName(ColStruct));

		// Extract binding name
		const FInstancedStruct* InputValue = ChooserHelpers::GetColumnInputValue(ColStruct);
		if (InputValue)
		{
			ColObj->SetStringField(TEXT("binding"), ChooserHelpers::GetBindingName(*InputValue));
		}
		else
		{
			ColObj->SetStringField(TEXT("binding"), TEXT(""));
		}

		// Serialize row values
		ColObj->SetArrayField(TEXT("row_values"), ChooserHelpers::SerializeRowValues(ColStruct, RowCount));

		ColumnsArray.Add(MakeShared<FJsonValueObject>(ColObj));
	}

	// Build nested tables list
	TArray<TSharedPtr<FJsonValue>> NestedArray;
#if WITH_EDITORONLY_DATA
	for (UChooserTable* Nested : Table->NestedChoosers)
	{
		if (Nested)
		{
			NestedArray.Add(MakeShared<FJsonValueString>(Nested->GetName()));
		}
	}
	// Also include any UChooserTable* in NestedObjects not already in NestedChoosers
	for (UObject* Obj : Table->NestedObjects)
	{
		UChooserTable* AsChooser = Cast<UChooserTable>(Obj);
		if (AsChooser && !Table->NestedChoosers.Contains(AsChooser))
		{
			NestedArray.Add(MakeShared<FJsonValueString>(AsChooser->GetName()));
		}
	}
#endif

	// Build response
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetBoolField(TEXT("success"), true);
	ResultData->SetStringField(TEXT("name"), Table->GetName());
	ResultData->SetStringField(TEXT("asset_path"), AssetPath);
	ResultData->SetNumberField(TEXT("row_count"), RowCount);
	ResultData->SetArrayField(TEXT("columns"), ColumnsArray);
	ResultData->SetArrayField(TEXT("nested_tables"), NestedArray);

	return ResultData;
}

// ── set_chooser_column_value ─────────────────────────────────────────────────

TSharedPtr<FJsonObject> FUnrealMCPChooserCommands::HandleSetChooserColumnValue(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	if (AssetPath.IsEmpty())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required field: asset_path"));
	}

	if (!Params->HasField(TEXT("column_index")) || !Params->HasField(TEXT("row_index")) || !Params->HasField(TEXT("value")))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required fields: column_index, row_index, value"));
	}

	const int32 ColumnIndex = static_cast<int32>(Params->GetNumberField(TEXT("column_index")));
	const int32 RowIndex = static_cast<int32>(Params->GetNumberField(TEXT("row_index")));

	FString SubTable;
	Params->TryGetStringField(TEXT("sub_table"), SubTable);

	FString LoadError;
	UChooserTable* Table = ChooserHelpers::LoadAndNavigate(AssetPath, SubTable, LoadError);
	if (!Table)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(LoadError);
	}

	// Validate column index
	if (!Table->ColumnsStructs.IsValidIndex(ColumnIndex))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Column index %d out of range [0, %d)"), ColumnIndex, Table->ColumnsStructs.Num()));
	}

	FInstancedStruct& ColStruct = Table->ColumnsStructs[ColumnIndex];

	// Detect column type and set value accordingly
	if (FMultiEnumColumn* MultiCol = ColStruct.GetMutablePtr<FMultiEnumColumn>())
	{
		if (!MultiCol->RowValues.IsValidIndex(RowIndex))
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Row index %d out of range [0, %d) for MultiEnum column %d"),
					RowIndex, MultiCol->RowValues.Num(), ColumnIndex));
		}

		const uint32 NewValue = static_cast<uint32>(Params->GetNumberField(TEXT("value")));
		MultiCol->RowValues[RowIndex].Value = NewValue;
	}
	else if (FEnumColumn* EnumCol = ColStruct.GetMutablePtr<FEnumColumn>())
	{
		if (!EnumCol->RowValues.IsValidIndex(RowIndex))
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Row index %d out of range [0, %d) for Enum column %d"),
					RowIndex, EnumCol->RowValues.Num(), ColumnIndex));
		}

		// Value can be a number (enum value) or an object with value + comparison
		const TSharedPtr<FJsonObject>* ValueObj = nullptr;
		if (Params->TryGetObjectField(TEXT("value"), ValueObj))
		{
			if ((*ValueObj)->HasField(TEXT("value")))
			{
				EnumCol->RowValues[RowIndex].Value = static_cast<uint8>((*ValueObj)->GetNumberField(TEXT("value")));
			}
			if ((*ValueObj)->HasField(TEXT("comparison")))
			{
				FString CompStr = (*ValueObj)->GetStringField(TEXT("comparison"));
				if (CompStr == TEXT("MatchEqual"))         EnumCol->RowValues[RowIndex].Comparison = EEnumColumnCellValueComparison::MatchEqual;
				else if (CompStr == TEXT("MatchNotEqual")) EnumCol->RowValues[RowIndex].Comparison = EEnumColumnCellValueComparison::MatchNotEqual;
				else if (CompStr == TEXT("MatchAny"))      EnumCol->RowValues[RowIndex].Comparison = EEnumColumnCellValueComparison::MatchAny;
			}
		}
		else
		{
			// Simple numeric value
			EnumCol->RowValues[RowIndex].Value = static_cast<uint8>(Params->GetNumberField(TEXT("value")));
		}

		// Allow separate comparison param at top level too
		FString ComparisonStr;
		if (Params->TryGetStringField(TEXT("comparison"), ComparisonStr))
		{
			if (ComparisonStr == TEXT("MatchEqual"))         EnumCol->RowValues[RowIndex].Comparison = EEnumColumnCellValueComparison::MatchEqual;
			else if (ComparisonStr == TEXT("MatchNotEqual")) EnumCol->RowValues[RowIndex].Comparison = EEnumColumnCellValueComparison::MatchNotEqual;
			else if (ComparisonStr == TEXT("MatchAny"))      EnumCol->RowValues[RowIndex].Comparison = EEnumColumnCellValueComparison::MatchAny;
		}
	}
	else if (FBoolColumn* BoolCol = ColStruct.GetMutablePtr<FBoolColumn>())
	{
		if (!BoolCol->RowValuesWithAny.IsValidIndex(RowIndex))
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Row index %d out of range [0, %d) for Bool column %d"),
					RowIndex, BoolCol->RowValuesWithAny.Num(), ColumnIndex));
		}

		FString BoolStr = Params->GetStringField(TEXT("value"));
		if (BoolStr == TEXT("MatchTrue") || BoolStr == TEXT("true"))
		{
			BoolCol->RowValuesWithAny[RowIndex] = EBoolColumnCellValue::MatchTrue;
		}
		else if (BoolStr == TEXT("MatchFalse") || BoolStr == TEXT("false"))
		{
			BoolCol->RowValuesWithAny[RowIndex] = EBoolColumnCellValue::MatchFalse;
		}
		else if (BoolStr == TEXT("MatchAny") || BoolStr == TEXT("any"))
		{
			BoolCol->RowValuesWithAny[RowIndex] = EBoolColumnCellValue::MatchAny;
		}
		else
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Invalid Bool value: '%s'. Expected: MatchTrue, MatchFalse, MatchAny, true, false, any"), *BoolStr));
		}
	}
	else if (FFloatRangeColumn* FloatCol = ColStruct.GetMutablePtr<FFloatRangeColumn>())
	{
		if (!FloatCol->RowValues.IsValidIndex(RowIndex))
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Row index %d out of range [0, %d) for FloatRange column %d"),
					RowIndex, FloatCol->RowValues.Num(), ColumnIndex));
		}

		const TSharedPtr<FJsonObject>* ValueObj = nullptr;
		if (!Params->TryGetObjectField(TEXT("value"), ValueObj))
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("FloatRange value must be a JSON object with min, max, no_min, no_max fields"));
		}

		FChooserFloatRangeRowData& RowData = FloatCol->RowValues[RowIndex];
		if ((*ValueObj)->HasField(TEXT("min")))    RowData.Min = static_cast<float>((*ValueObj)->GetNumberField(TEXT("min")));
		if ((*ValueObj)->HasField(TEXT("max")))    RowData.Max = static_cast<float>((*ValueObj)->GetNumberField(TEXT("max")));
		if ((*ValueObj)->HasField(TEXT("no_min"))) RowData.bNoMin = (*ValueObj)->GetBoolField(TEXT("no_min"));
		if ((*ValueObj)->HasField(TEXT("no_max"))) RowData.bNoMax = (*ValueObj)->GetBoolField(TEXT("no_max"));
	}
	else if (FGameplayTagColumn* TagCol = ColStruct.GetMutablePtr<FGameplayTagColumn>())
	{
		if (!TagCol->RowValues.IsValidIndex(RowIndex))
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Row index %d out of range [0, %d) for GameplayTag column %d"),
					RowIndex, TagCol->RowValues.Num(), ColumnIndex));
		}

		FString TagString = Params->GetStringField(TEXT("value"));
		FGameplayTagContainer NewTags;
		NewTags.FromExportString(TagString);
		TagCol->RowValues[RowIndex] = NewTags;
	}
	else
	{
		FString TypeName = ChooserHelpers::GetColumnTypeName(ColStruct);
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unsupported column type for set_chooser_column_value: %s (column %d)"), *TypeName, ColumnIndex));
	}

	// Mark dirty
	Table->MarkPackageDirty();

	// Optionally save
	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		// Recompile the table after modification
		Table->Compile(true);
		UEditorAssetLibrary::SaveAsset(AssetPath, false);
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetBoolField(TEXT("success"), true);
	ResultData->SetStringField(TEXT("asset_path"), AssetPath);
	ResultData->SetNumberField(TEXT("column_index"), ColumnIndex);
	ResultData->SetNumberField(TEXT("row_index"), RowIndex);
	ResultData->SetStringField(TEXT("column_type"), ChooserHelpers::GetColumnTypeName(ColStruct));
	return ResultData;
}
