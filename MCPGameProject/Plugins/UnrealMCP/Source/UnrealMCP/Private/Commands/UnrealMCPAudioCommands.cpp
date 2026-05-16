#include "Commands/UnrealMCPAudioCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"

// UE core
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"

// Audio
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

// Factories
#include "Factories/SoundClassFactory.h"
#include "Factories/SoundMixFactory.h"

FUnrealMCPAudioCommands::FUnrealMCPAudioCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPAudioCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("create_sound_class"))
	{
		return HandleCreateSoundClass(Params);
	}
	else if (CommandType == TEXT("create_sound_mix"))
	{
		return HandleCreateSoundMix(Params);
	}
	else if (CommandType == TEXT("set_sound_class_parent"))
	{
		return HandleSetSoundClassParent(Params);
	}
	else if (CommandType == TEXT("get_audio_info"))
	{
		return HandleGetAudioInfo(Params);
	}

	return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown audio command: %s"), *CommandType));
}

// ---------------------------------------------------------------------------
// create_sound_class
// Params: { "name", "path", "parent_class", "properties": { "volume", "pitch" }, "save" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPAudioCommands::HandleCreateSoundClass(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	FString Path = TEXT("/Game/Audio");
	Params->TryGetStringField(TEXT("path"), Path);

	// Check if asset already exists
	FString FullPath = Path / Name;
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("SoundClass already exists: %s"), *FullPath));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	// Create via AssetTools (auto-discovers USoundClassFactory)
	IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(Name, Path, USoundClass::StaticClass(), nullptr);

	USoundClass* SoundClass = Cast<USoundClass>(NewAsset);
	if (!SoundClass)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to create SoundClass: %s"), *FullPath));
	}

	// Set optional properties
	const TSharedPtr<FJsonObject>* PropertiesObj;
	if (Params->TryGetObjectField(TEXT("properties"), PropertiesObj))
	{
		double Volume;
		if ((*PropertiesObj)->TryGetNumberField(TEXT("volume"), Volume))
		{
			SoundClass->Properties.Volume = static_cast<float>(Volume);
		}
		double Pitch;
		if ((*PropertiesObj)->TryGetNumberField(TEXT("pitch"), Pitch))
		{
			SoundClass->Properties.Pitch = static_cast<float>(Pitch);
		}
	}

	// Set optional parent class
	FString ParentClassPath;
	if (Params->TryGetStringField(TEXT("parent_class"), ParentClassPath) && !ParentClassPath.IsEmpty())
	{
		USoundClass* ParentClass = LoadObject<USoundClass>(nullptr, *ParentClassPath);
		if (ParentClass)
		{
			SoundClass->ParentClass = ParentClass;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Could not load parent SoundClass: %s"), *ParentClassPath);
		}
	}

	FAssetRegistryModule::AssetCreated(SoundClass);
	SoundClass->GetOutermost()->MarkPackageDirty();

	// Optional save
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
	Result->SetStringField(TEXT("type"), TEXT("SoundClass"));
	return Result;
}

// ---------------------------------------------------------------------------
// create_sound_mix
// Params: { "name", "path", "save" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPAudioCommands::HandleCreateSoundMix(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	FString Path = TEXT("/Game/Audio");
	Params->TryGetStringField(TEXT("path"), Path);

	FString FullPath = Path / Name;
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("SoundMix already exists: %s"), *FullPath));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(Name, Path, USoundMix::StaticClass(), nullptr);

	USoundMix* SoundMix = Cast<USoundMix>(NewAsset);
	if (!SoundMix)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to create SoundMix: %s"), *FullPath));
	}

	FAssetRegistryModule::AssetCreated(SoundMix);
	SoundMix->GetOutermost()->MarkPackageDirty();

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
	Result->SetStringField(TEXT("type"), TEXT("SoundMix"));
	return Result;
}

// ---------------------------------------------------------------------------
// set_sound_class_parent
// Params: { "asset_path", "parent_path", "save" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPAudioCommands::HandleSetSoundClassParent(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
	}

	FString ParentPath;
	if (!Params->TryGetStringField(TEXT("parent_path"), ParentPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parent_path' parameter"));
	}

	USoundClass* SoundClass = LoadObject<USoundClass>(nullptr, *AssetPath);
	if (!SoundClass)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Could not load SoundClass: %s"), *AssetPath));
	}

	USoundClass* ParentClass = LoadObject<USoundClass>(nullptr, *ParentPath);
	if (!ParentClass)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Could not load parent SoundClass: %s"), *ParentPath));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	SoundClass->ParentClass = ParentClass;
	SoundClass->GetOutermost()->MarkPackageDirty();

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(AssetPath, false);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("parent_path"), ParentPath);
	return Result;
}

// ---------------------------------------------------------------------------
// get_audio_info
// Params: { "asset_path" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPAudioCommands::HandleGetAudioInfo(const TSharedPtr<FJsonObject>& Params)
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

	if (USoundClass* SoundClass = Cast<USoundClass>(Asset))
	{
		Result->SetStringField(TEXT("type"), TEXT("SoundClass"));
		Result->SetNumberField(TEXT("volume"), SoundClass->Properties.Volume);
		Result->SetNumberField(TEXT("pitch"), SoundClass->Properties.Pitch);
		Result->SetStringField(TEXT("parent_class"),
			SoundClass->ParentClass ? SoundClass->ParentClass->GetPathName() : TEXT("None"));

		// List child classes
		TArray<TSharedPtr<FJsonValue>> ChildArray;
		for (USoundClass* Child : SoundClass->ChildClasses)
		{
			if (Child)
			{
				ChildArray.Add(MakeShared<FJsonValueString>(Child->GetPathName()));
			}
		}
		Result->SetArrayField(TEXT("child_classes"), ChildArray);
	}
	else if (USoundMix* SoundMix = Cast<USoundMix>(Asset))
	{
		Result->SetStringField(TEXT("type"), TEXT("SoundMix"));
		Result->SetNumberField(TEXT("initial_delay"), SoundMix->InitialDelay);
		Result->SetNumberField(TEXT("fade_in_time"), SoundMix->FadeInTime);
		Result->SetNumberField(TEXT("duration"), SoundMix->Duration);
		Result->SetNumberField(TEXT("fade_out_time"), SoundMix->FadeOutTime);
	}
	else
	{
		Result->SetStringField(TEXT("type"), TEXT("Unknown"));
	}

	return Result;
}
