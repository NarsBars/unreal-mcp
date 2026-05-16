#include "Commands/UnrealMCPNiagaraCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"

// UE core
#include "EditorAssetLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"

// Niagara
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraSystemFactoryNew.h"
#include "NiagaraEmitterFactoryNew.h"

FUnrealMCPNiagaraCommands::FUnrealMCPNiagaraCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPNiagaraCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("create_niagara_system"))
	{
		return HandleCreateNiagaraSystem(Params);
	}
	else if (CommandType == TEXT("create_niagara_emitter"))
	{
		return HandleCreateNiagaraEmitter(Params);
	}

	return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown niagara command: %s"), *CommandType));
}

// ---------------------------------------------------------------------------
// create_niagara_system
// Params: { "name", "path", "template", "save" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPNiagaraCommands::HandleCreateNiagaraSystem(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	FString Path = TEXT("/Game/VFX");
	Params->TryGetStringField(TEXT("path"), Path);

	FString FullPath = Path / Name;
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Niagara system already exists: %s"), *FullPath));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	// Optionally copy from a template system
	UNiagaraSystemFactoryNew* Factory = NewObject<UNiagaraSystemFactoryNew>();

	FString TemplatePath;
	if (Params->TryGetStringField(TEXT("template"), TemplatePath) && !TemplatePath.IsEmpty())
	{
		UNiagaraSystem* Template = LoadObject<UNiagaraSystem>(nullptr, *TemplatePath);
		if (Template)
		{
			Factory->SystemToCopy = Template;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Could not load Niagara system template: %s, creating empty system"), *TemplatePath);
		}
	}

	IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(Name, Path, UNiagaraSystem::StaticClass(), Factory);

	if (!NewAsset)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to create Niagara system: %s"), *FullPath));
	}

	// Initialize with default nodes
	UNiagaraSystem* System = Cast<UNiagaraSystem>(NewAsset);
	if (System)
	{
		UNiagaraSystemFactoryNew::InitializeSystem(System, true);
	}

	FAssetRegistryModule::AssetCreated(NewAsset);
	NewAsset->GetOutermost()->MarkPackageDirty();

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
	Result->SetStringField(TEXT("type"), TEXT("NiagaraSystem"));
	return Result;
}

// ---------------------------------------------------------------------------
// create_niagara_emitter
// Params: { "name", "path", "template", "save" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPNiagaraCommands::HandleCreateNiagaraEmitter(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	FString Path = TEXT("/Game/VFX");
	Params->TryGetStringField(TEXT("path"), Path);

	FString FullPath = Path / Name;
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Niagara emitter already exists: %s"), *FullPath));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	// Optionally copy from a template emitter
	UNiagaraEmitterFactoryNew* Factory = NewObject<UNiagaraEmitterFactoryNew>();
	Factory->bAddDefaultModulesAndRenderersToEmptyEmitter = true;

	FString TemplatePath;
	if (Params->TryGetStringField(TEXT("template"), TemplatePath) && !TemplatePath.IsEmpty())
	{
		UNiagaraEmitter* Template = LoadObject<UNiagaraEmitter>(nullptr, *TemplatePath);
		if (Template)
		{
			Factory->EmitterToCopy = Template;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Could not load Niagara emitter template: %s, creating empty emitter"), *TemplatePath);
		}
	}

	IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(Name, Path, UNiagaraEmitter::StaticClass(), Factory);

	if (!NewAsset)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to create Niagara emitter: %s"), *FullPath));
	}

	FAssetRegistryModule::AssetCreated(NewAsset);
	NewAsset->GetOutermost()->MarkPackageDirty();

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
	Result->SetStringField(TEXT("type"), TEXT("NiagaraEmitter"));
	return Result;
}
