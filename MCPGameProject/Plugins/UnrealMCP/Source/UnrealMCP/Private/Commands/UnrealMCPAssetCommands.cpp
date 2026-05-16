#include "Commands/UnrealMCPAssetCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"

// UE core
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetImportTask.h"
#include "Misc/Paths.h"

FUnrealMCPAssetCommands::FUnrealMCPAssetCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("search_assets"))
	{
		return HandleSearchAssets(Params);
	}
	else if (CommandType == TEXT("import_asset"))
	{
		return HandleImportAsset(Params);
	}
	else if (CommandType == TEXT("duplicate_asset"))
	{
		return HandleDuplicateAsset(Params);
	}
	else if (CommandType == TEXT("rename_asset"))
	{
		return HandleRenameAsset(Params);
	}
	else if (CommandType == TEXT("move_asset"))
	{
		return HandleMoveAsset(Params);
	}
	else if (CommandType == TEXT("delete_asset"))
	{
		return HandleDeleteAsset(Params);
	}
	else if (CommandType == TEXT("get_asset_dependencies"))
	{
		return HandleGetAssetDependencies(Params);
	}
	else if (CommandType == TEXT("save_asset"))
	{
		return HandleSaveAsset(Params);
	}

	return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown asset command: %s"), *CommandType));
}

// ---------------------------------------------------------------------------
// search_assets
// Params: { "query", "class_filter", "path", "recursive" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleSearchAssets(const TSharedPtr<FJsonObject>& Params)
{
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;

	// Optional path filter
	FString Path;
	if (Params->TryGetStringField(TEXT("path"), Path) && !Path.IsEmpty())
	{
		Filter.PackagePaths.Add(FName(*Path));

		bool bRecursive = true;
		if (Params->HasField(TEXT("recursive")))
		{
			bRecursive = Params->GetBoolField(TEXT("recursive"));
		}
		Filter.bRecursivePaths = bRecursive;
	}
	else
	{
		// Default: search all game content
		Filter.PackagePaths.Add(FName(TEXT("/Game")));
		Filter.bRecursivePaths = true;
	}

	// Optional class filter — supports "ClassName" or "ModuleName.ClassName"
	FString ClassFilter;
	if (Params->TryGetStringField(TEXT("class_filter"), ClassFilter) && !ClassFilter.IsEmpty())
	{
		if (ClassFilter.Contains(TEXT(".")))
		{
			// Explicit module: "PoseSearch.PoseSearchDatabase"
			FString ModuleName, ClassName;
			ClassFilter.Split(TEXT("."), &ModuleName, &ClassName);
			Filter.ClassPaths.Add(FTopLevelAssetPath(*FString::Printf(TEXT("/Script/%s"), *ModuleName), *ClassName));
		}
		else
		{
			// Try common modules in order — first match wins
			static const TCHAR* ModulesToSearch[] = {
				TEXT("/Script/Engine"),
				TEXT("/Script/CoreUObject"),
				TEXT("/Script/PoseSearch"),
				TEXT("/Script/Niagara"),
				TEXT("/Script/Paper2D"),
				TEXT("/Script/UMG"),
			};

			bool bFound = false;
			for (const TCHAR* Module : ModulesToSearch)
			{
				FTopLevelAssetPath TestPath(Module, *ClassFilter);
				UClass* TestClass = FindObject<UClass>(TestPath);
				if (TestClass)
				{
					Filter.ClassPaths.Add(TestPath);
					bFound = true;
					break;
				}
			}

			if (!bFound)
			{
				// Fallback: search all loaded classes by name
				for (TObjectIterator<UClass> It; It; ++It)
				{
					if (It->GetName() == ClassFilter)
					{
						Filter.ClassPaths.Add(It->GetClassPathName());
						bFound = true;
						break;
					}
				}
			}

			if (!bFound)
			{
				// Last resort: assume Engine (original behavior)
				Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), *ClassFilter));
			}
		}
	}

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	// Optional name query filter (substring match)
	FString Query;
	Params->TryGetStringField(TEXT("query"), Query);

	TArray<TSharedPtr<FJsonValue>> AssetsArray;
	int32 Limit = 100;
	if (Params->HasField(TEXT("limit")))
	{
		Limit = static_cast<int32>(Params->GetNumberField(TEXT("limit")));
	}

	for (const FAssetData& AssetData : AssetDataList)
	{
		if (AssetsArray.Num() >= Limit)
		{
			break;
		}

		FString AssetName = AssetData.AssetName.ToString();
		if (!Query.IsEmpty() && !AssetName.Contains(Query))
		{
			continue;
		}

		TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
		AssetObj->SetStringField(TEXT("name"), AssetName);
		AssetObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
		AssetObj->SetStringField(TEXT("package_path"), AssetData.PackagePath.ToString());
		AssetObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());
		AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("assets"), AssetsArray);
	Result->SetNumberField(TEXT("count"), AssetsArray.Num());
	Result->SetNumberField(TEXT("total_matching"), AssetDataList.Num());
	return Result;
}

// ---------------------------------------------------------------------------
// import_asset
// Params: { "source_path", "destination_path", "asset_name", "save" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleImportAsset(const TSharedPtr<FJsonObject>& Params)
{
	FString SourcePath;
	if (!Params->TryGetStringField(TEXT("source_path"), SourcePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_path' parameter"));
	}

	FString DestinationPath;
	if (!Params->TryGetStringField(TEXT("destination_path"), DestinationPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'destination_path' parameter"));
	}

	// Verify source file exists
	FString FullSourcePath = FPaths::ConvertRelativePathToFull(SourcePath);
	if (!FPaths::FileExists(FullSourcePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Source file does not exist: %s"), *FullSourcePath));
	}

	UAssetImportTask* ImportTask = NewObject<UAssetImportTask>();
	ImportTask->bAutomated = true;
	ImportTask->bReplaceExisting = true;
	ImportTask->bSave = true;
	ImportTask->Filename = FullSourcePath;
	ImportTask->DestinationPath = DestinationPath;

	FString AssetName;
	if (Params->TryGetStringField(TEXT("asset_name"), AssetName) && !AssetName.IsEmpty())
	{
		ImportTask->DestinationName = AssetName;
	}

	if (Params->HasField(TEXT("save")))
	{
		ImportTask->bSave = Params->GetBoolField(TEXT("save"));
	}

	IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	TArray<UAssetImportTask*> Tasks;
	Tasks.Add(ImportTask);
	AssetTools.ImportAssetTasks(Tasks);

	// Check results via GetObjects()
	const TArray<UObject*>& ImportedObjects = ImportTask->GetObjects();
	if (ImportedObjects.Num() > 0 && ImportedObjects[0])
	{
		UObject* ImportedObj = ImportedObjects[0];
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("imported_path"), ImportedObj->GetPathName());
		Result->SetStringField(TEXT("asset_name"), ImportedObj->GetName());
		Result->SetStringField(TEXT("class"), ImportedObj->GetClass()->GetName());
		return Result;
	}

	return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to import asset from: %s"), *FullSourcePath));
}

// ---------------------------------------------------------------------------
// duplicate_asset
// Params: { "source_path", "destination_path", "save" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleDuplicateAsset(const TSharedPtr<FJsonObject>& Params)
{
	FString SourcePath;
	if (!Params->TryGetStringField(TEXT("source_path"), SourcePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_path' parameter"));
	}

	FString DestinationPath;
	if (!Params->TryGetStringField(TEXT("destination_path"), DestinationPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'destination_path' parameter"));
	}

	if (!UEditorAssetLibrary::DoesAssetExist(SourcePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Source asset does not exist: %s"), *SourcePath));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	UObject* DuplicatedAsset = UEditorAssetLibrary::DuplicateAsset(SourcePath, DestinationPath);
	if (!DuplicatedAsset)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to duplicate asset: %s -> %s"), *SourcePath, *DestinationPath));
	}

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(DestinationPath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("source_path"), SourcePath);
	Result->SetStringField(TEXT("destination_path"), DestinationPath);
	Result->SetStringField(TEXT("asset_name"), DuplicatedAsset->GetName());
	return Result;
}

// ---------------------------------------------------------------------------
// rename_asset
// Params: { "source_path", "new_name", "save" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleRenameAsset(const TSharedPtr<FJsonObject>& Params)
{
	FString SourcePath;
	if (!Params->TryGetStringField(TEXT("source_path"), SourcePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_path' parameter"));
	}

	FString NewName;
	if (!Params->TryGetStringField(TEXT("new_name"), NewName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'new_name' parameter"));
	}

	if (!UEditorAssetLibrary::DoesAssetExist(SourcePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset does not exist: %s"), *SourcePath));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	// Build new path: same directory, new name
	FString PackagePath = FPackageName::GetLongPackagePath(SourcePath);
	FString NewPath = PackagePath / NewName;

	bool bSuccess = UEditorAssetLibrary::RenameAsset(SourcePath, NewPath);
	if (!bSuccess)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to rename asset: %s -> %s"), *SourcePath, *NewPath));
	}

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(NewPath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("old_path"), SourcePath);
	Result->SetStringField(TEXT("new_path"), NewPath);
	Result->SetStringField(TEXT("new_name"), NewName);
	return Result;
}

// ---------------------------------------------------------------------------
// move_asset
// Params: { "source_path", "destination_path", "save" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleMoveAsset(const TSharedPtr<FJsonObject>& Params)
{
	FString SourcePath;
	if (!Params->TryGetStringField(TEXT("source_path"), SourcePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_path' parameter"));
	}

	FString DestinationPath;
	if (!Params->TryGetStringField(TEXT("destination_path"), DestinationPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'destination_path' parameter"));
	}

	if (!UEditorAssetLibrary::DoesAssetExist(SourcePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset does not exist: %s"), *SourcePath));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	bool bSuccess = UEditorAssetLibrary::RenameAsset(SourcePath, DestinationPath);
	if (!bSuccess)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to move asset: %s -> %s"), *SourcePath, *DestinationPath));
	}

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(DestinationPath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("source_path"), SourcePath);
	Result->SetStringField(TEXT("destination_path"), DestinationPath);
	return Result;
}

// ---------------------------------------------------------------------------
// delete_asset
// Params: { "asset_path", "force" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params)
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

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	bool bSuccess = UEditorAssetLibrary::DeleteAsset(AssetPath);
	if (!bSuccess)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to delete asset: %s"), *AssetPath));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("deleted_path"), AssetPath);
	return Result;
}

// ---------------------------------------------------------------------------
// get_asset_dependencies
// Params: { "asset_path", "recursive" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleGetAssetDependencies(const TSharedPtr<FJsonObject>& Params)
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

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	// Convert asset path to package name for dependency lookup
	FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);

	TArray<FName> Dependencies;
	bool bRecursive = false;
	if (Params->HasField(TEXT("recursive")))
	{
		bRecursive = Params->GetBoolField(TEXT("recursive"));
	}

	if (bRecursive)
	{
		// Get recursive dependencies
		AssetRegistry.GetDependencies(FName(*PackageName), Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
	}
	else
	{
		AssetRegistry.GetDependencies(FName(*PackageName), Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
	}

	TArray<TSharedPtr<FJsonValue>> DepsArray;
	for (const FName& Dep : Dependencies)
	{
		DepsArray.Add(MakeShared<FJsonValueString>(Dep.ToString()));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetArrayField(TEXT("dependencies"), DepsArray);
	Result->SetNumberField(TEXT("count"), DepsArray.Num());
	return Result;
}

// ---------------------------------------------------------------------------
// save_asset
// Params: { "asset_path" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleSaveAsset(const TSharedPtr<FJsonObject>& Params)
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

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	bool bSuccess = UEditorAssetLibrary::SaveAsset(AssetPath, false);
	if (!bSuccess)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to save asset: %s"), *AssetPath));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("saved_path"), AssetPath);
	return Result;
}
