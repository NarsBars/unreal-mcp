#include "Commands/UnrealMCPLevelCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"

// UE core
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "LevelEditorSubsystem.h"
#include "Engine/World.h"

FUnrealMCPLevelCommands::FUnrealMCPLevelCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("open_level"))
	{
		return HandleOpenLevel(Params);
	}
	else if (CommandType == TEXT("save_level"))
	{
		return HandleSaveLevel(Params);
	}
	else if (CommandType == TEXT("list_levels"))
	{
		return HandleListLevels(Params);
	}
	else if (CommandType == TEXT("create_level"))
	{
		return HandleCreateLevel(Params);
	}

	return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown level command: %s"), *CommandType));
}

// ---------------------------------------------------------------------------
// open_level
// Params: { "level_path" }
// WARNING: This will discard the current map!
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleOpenLevel(const TSharedPtr<FJsonObject>& Params)
{
	FString LevelPath;
	if (!Params->TryGetStringField(TEXT("level_path"), LevelPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'level_path' parameter"));
	}

	if (!UEditorAssetLibrary::DoesAssetExist(LevelPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Level does not exist: %s"), *LevelPath));
	}

	ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
	if (!LevelEditorSubsystem)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("LevelEditorSubsystem not available"));
	}

	bool bSuccess = LevelEditorSubsystem->LoadLevel(LevelPath);
	if (!bSuccess)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to open level: %s"), *LevelPath));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("opened_level"), LevelPath);
	return Result;
}

// ---------------------------------------------------------------------------
// save_level
// Params: { "path" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleSaveLevel(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available"));
	}

	FString SavePath;
	if (Params->TryGetStringField(TEXT("path"), SavePath) && !SavePath.IsEmpty())
	{
		// Save As to new path
		FString PackageFilename;
		if (!FPackageName::TryConvertLongPackageNameToFilename(SavePath, PackageFilename, FPackageName::GetMapPackageExtension()))
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid save path: %s"), *SavePath));
		}

		bool bSuccess = FEditorFileUtils::SaveLevel(World->PersistentLevel, *PackageFilename);
		if (!bSuccess)
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to save level"));
		}

		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("saved_path"), SavePath);
		return Result;
	}
	else
	{
		// Save current level in place
		bool bSuccess = FEditorFileUtils::SaveCurrentLevel();
		if (!bSuccess)
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to save current level"));
		}

		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("saved_level"), World->GetMapName());
		return Result;
	}
}

// ---------------------------------------------------------------------------
// list_levels
// Params: { "path" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleListLevels(const TSharedPtr<FJsonObject>& Params)
{
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UWorld::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;

	FString Path;
	if (Params->TryGetStringField(TEXT("path"), Path) && !Path.IsEmpty())
	{
		Filter.PackagePaths.Add(FName(*Path));
	}
	else
	{
		Filter.PackagePaths.Add(FName(TEXT("/Game")));
	}

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	TArray<TSharedPtr<FJsonValue>> LevelsArray;
	for (const FAssetData& AssetData : AssetDataList)
	{
		TSharedPtr<FJsonObject> LevelObj = MakeShared<FJsonObject>();
		LevelObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		LevelObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
		LevelObj->SetStringField(TEXT("package_path"), AssetData.PackagePath.ToString());
		LevelsArray.Add(MakeShared<FJsonValueObject>(LevelObj));
	}

	// Include current level info
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	FString CurrentLevel = World ? World->GetMapName() : TEXT("Unknown");

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("current_level"), CurrentLevel);
	Result->SetArrayField(TEXT("levels"), LevelsArray);
	Result->SetNumberField(TEXT("count"), LevelsArray.Num());
	return Result;
}

// ---------------------------------------------------------------------------
// create_level
// Params: { "name", "path", "save" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleCreateLevel(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	FString Path = TEXT("/Game/Maps");
	Params->TryGetStringField(TEXT("path"), Path);

	FString FullPath = Path / Name;

	// Check if level already exists
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Level already exists: %s"), *FullPath));
	}

	// Create a new empty level via LevelEditorSubsystem
	ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
	if (!LevelEditorSubsystem)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("LevelEditorSubsystem not available"));
	}

	bool bCreated = LevelEditorSubsystem->NewLevel(FullPath);
	if (!bCreated)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to create level: %s"), *FullPath));
	}

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		FEditorFileUtils::SaveCurrentLevel();
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("name"), Name);
	Result->SetStringField(TEXT("path"), FullPath);
	return Result;
}
