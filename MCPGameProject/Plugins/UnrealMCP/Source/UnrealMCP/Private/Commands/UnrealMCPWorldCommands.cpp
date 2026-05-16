#include "Commands/UnrealMCPWorldCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"

// UE core
#include "EditorAssetLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "EngineUtils.h"

// Foliage
#include "FoliageType_InstancedStaticMesh.h"
#include "InstancedFoliageActor.h"
#include "InstancedFoliage.h"
#include "FoliageType.h"

// Landscape
#include "LandscapeProxy.h"
#include "LandscapeInfo.h"
#include "LandscapeComponent.h"

// Mesh
#include "Engine/StaticMesh.h"

FUnrealMCPWorldCommands::FUnrealMCPWorldCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPWorldCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("add_foliage_type"))
	{
		return HandleAddFoliageType(Params);
	}
	else if (CommandType == TEXT("paint_foliage"))
	{
		return HandlePaintFoliage(Params);
	}
	else if (CommandType == TEXT("list_foliage_types"))
	{
		return HandleListFoliageTypes(Params);
	}
	else if (CommandType == TEXT("get_landscape_info"))
	{
		return HandleGetLandscapeInfo(Params);
	}

	return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown world command: %s"), *CommandType));
}

// ---------------------------------------------------------------------------
// add_foliage_type
// Params: { "name", "mesh_path", "path", "min_scale", "max_scale", "align_to_normal", "save" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPWorldCommands::HandleAddFoliageType(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	FString MeshPath;
	if (!Params->TryGetStringField(TEXT("mesh_path"), MeshPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'mesh_path' parameter"));
	}

	FString Path = TEXT("/Game/Foliage");
	Params->TryGetStringField(TEXT("path"), Path);

	// Load mesh
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
	if (!Mesh)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Could not load mesh: %s"), *MeshPath));
	}

	FString FullPath = Path / Name;
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Foliage type already exists: %s"), *FullPath));
	}

	if (auto PIEError = FUnrealMCPCommonUtils::EnsurePIEStopped()) return PIEError;

	// Create foliage type asset
	IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(Name, Path, UFoliageType_InstancedStaticMesh::StaticClass(), nullptr);

	UFoliageType_InstancedStaticMesh* FoliageType = Cast<UFoliageType_InstancedStaticMesh>(NewAsset);
	if (!FoliageType)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to create foliage type: %s"), *FullPath));
	}

	// Set mesh
	FoliageType->SetStaticMesh(Mesh);

	// Optional scale
	double MinScale = 1.0;
	double MaxScale = 1.0;
	Params->TryGetNumberField(TEXT("min_scale"), MinScale);
	Params->TryGetNumberField(TEXT("max_scale"), MaxScale);

	FoliageType->ScaleX.Min = static_cast<float>(MinScale);
	FoliageType->ScaleX.Max = static_cast<float>(MaxScale);
	FoliageType->ScaleY.Min = static_cast<float>(MinScale);
	FoliageType->ScaleY.Max = static_cast<float>(MaxScale);
	FoliageType->ScaleZ.Min = static_cast<float>(MinScale);
	FoliageType->ScaleZ.Max = static_cast<float>(MaxScale);

	// Optional align to normal
	bool bAlignToNormal = false;
	if (Params->HasField(TEXT("align_to_normal")))
	{
		bAlignToNormal = Params->GetBoolField(TEXT("align_to_normal"));
	}
	FoliageType->AlignToNormal = bAlignToNormal;

	FAssetRegistryModule::AssetCreated(FoliageType);
	FoliageType->GetOutermost()->MarkPackageDirty();

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
	Result->SetStringField(TEXT("type"), TEXT("FoliageType_InstancedStaticMesh"));
	Result->SetStringField(TEXT("mesh"), MeshPath);
	return Result;
}

// ---------------------------------------------------------------------------
// paint_foliage
// Params: { "foliage_type_path", "locations": [{"x":0,"y":0,"z":0}, ...], "rotation_range", "scale_range" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPWorldCommands::HandlePaintFoliage(const TSharedPtr<FJsonObject>& Params)
{
	FString FoliageTypePath;
	if (!Params->TryGetStringField(TEXT("foliage_type_path"), FoliageTypePath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'foliage_type_path' parameter"));
	}

	const TArray<TSharedPtr<FJsonValue>>* LocationsArray;
	if (!Params->TryGetArrayField(TEXT("locations"), LocationsArray))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'locations' array parameter"));
	}

	// Load foliage type
	UFoliageType* FoliageType = LoadObject<UFoliageType>(nullptr, *FoliageTypePath);
	if (!FoliageType)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Could not load foliage type: %s"), *FoliageTypePath));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available"));
	}

	// Build transform array from locations
	TArray<FTransform> Transforms;
	for (const TSharedPtr<FJsonValue>& LocationValue : *LocationsArray)
	{
		const TSharedPtr<FJsonObject>& LocationObj = LocationValue->AsObject();
		if (!LocationObj.IsValid())
		{
			continue;
		}

		double X = 0, Y = 0, Z = 0;
		LocationObj->TryGetNumberField(TEXT("x"), X);
		LocationObj->TryGetNumberField(TEXT("y"), Y);
		LocationObj->TryGetNumberField(TEXT("z"), Z);

		FTransform Transform;
		Transform.SetLocation(FVector(X, Y, Z));
		Transforms.Add(Transform);
	}

	if (Transforms.Num() == 0)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No valid locations provided"));
	}

	// Find or create the InstancedFoliageActor for this level
	AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(World, true);
	if (!IFA)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Could not get or create InstancedFoliageActor for current level"));
	}

	// Ensure foliage info exists for this type
	TUniqueObj<FFoliageInfo>& FoliageInfo = IFA->AddFoliageInfo(FoliageType);

	// Add each instance
	int32 AddedCount = 0;
	for (const FTransform& Transform : Transforms)
	{
		FFoliageInstance Instance;
		Instance.Location = Transform.GetLocation();
		Instance.DrawScale3D = FVector3f(1.f, 1.f, 1.f);
		FoliageInfo->AddInstance(FoliageType, Instance);
		AddedCount++;
	}

	IFA->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("foliage_type"), FoliageTypePath);
	Result->SetNumberField(TEXT("instances_added"), AddedCount);
	return Result;
}

// ---------------------------------------------------------------------------
// list_foliage_types
// Params: { "path" }
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPWorldCommands::HandleListFoliageTypes(const TSharedPtr<FJsonObject>& Params)
{
	FString SearchPath = TEXT("/Game");
	Params->TryGetStringField(TEXT("path"), SearchPath);

	// Search asset registry for foliage types
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UFoliageType_InstancedStaticMesh::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(*SearchPath));
	Filter.bRecursivePaths = true;

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	TArray<TSharedPtr<FJsonValue>> FoliageTypes;
	for (const FAssetData& AssetData : AssetDataList)
	{
		TSharedPtr<FJsonObject> TypeObj = MakeShared<FJsonObject>();
		TypeObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		TypeObj->SetStringField(TEXT("path"), AssetData.GetSoftObjectPath().ToString());
		FoliageTypes.Add(MakeShared<FJsonValueObject>(TypeObj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("foliage_types"), FoliageTypes);
	Result->SetNumberField(TEXT("count"), FoliageTypes.Num());
	return Result;
}

// ---------------------------------------------------------------------------
// get_landscape_info
// Params: { "name" } (optional - if omitted, returns info for all landscapes)
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPWorldCommands::HandleGetLandscapeInfo(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available"));
	}

	FString TargetName;
	Params->TryGetStringField(TEXT("name"), TargetName);

	TArray<TSharedPtr<FJsonValue>> Landscapes;

	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		ALandscapeProxy* Landscape = *It;
		if (!Landscape)
		{
			continue;
		}

		if (!TargetName.IsEmpty() && Landscape->GetActorLabel() != TargetName && Landscape->GetName() != TargetName)
		{
			continue;
		}

		TSharedPtr<FJsonObject> LandscapeObj = MakeShared<FJsonObject>();
		LandscapeObj->SetStringField(TEXT("name"), Landscape->GetActorLabel());
		LandscapeObj->SetStringField(TEXT("class"), Landscape->GetClass()->GetName());

		// Location
		FVector Location = Landscape->GetActorLocation();
		TSharedPtr<FJsonObject> LocationObj = MakeShared<FJsonObject>();
		LocationObj->SetNumberField(TEXT("x"), Location.X);
		LocationObj->SetNumberField(TEXT("y"), Location.Y);
		LocationObj->SetNumberField(TEXT("z"), Location.Z);
		LandscapeObj->SetObjectField(TEXT("location"), LocationObj);

		// Scale
		FVector Scale = Landscape->GetActorScale3D();
		TSharedPtr<FJsonObject> ScaleObj = MakeShared<FJsonObject>();
		ScaleObj->SetNumberField(TEXT("x"), Scale.X);
		ScaleObj->SetNumberField(TEXT("y"), Scale.Y);
		ScaleObj->SetNumberField(TEXT("z"), Scale.Z);
		LandscapeObj->SetObjectField(TEXT("scale"), ScaleObj);

		// Component count
		TArray<ULandscapeComponent*> LandscapeComponents;
		Landscape->GetComponents<ULandscapeComponent>(LandscapeComponents);
		LandscapeObj->SetNumberField(TEXT("component_count"), LandscapeComponents.Num());

		// Material
		if (Landscape->GetLandscapeMaterial())
		{
			LandscapeObj->SetStringField(TEXT("material"), Landscape->GetLandscapeMaterial()->GetPathName());
		}

		Landscapes.Add(MakeShared<FJsonValueObject>(LandscapeObj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("landscapes"), Landscapes);
	Result->SetNumberField(TEXT("count"), Landscapes.Num());
	return Result;
}
