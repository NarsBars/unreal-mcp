// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealMCP : ModuleRules
{
	public UnrealMCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		// Use IWYUSupport instead of the deprecated bEnforceIWYU in UE5.5
		IWYUSupport = IWYUSupport.Full;

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
		);
		
		PrivateIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(EngineDirectory, "Source/Editor/AnimGraph/Internal"), // UAnimGraphNodeBinding (bind_anim_pin_to_property)
			}
		);
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Networking",
				"Sockets",
				"HTTP",
				"Json",
				"JsonUtilities",
				"DeveloperSettings",
			"GameplayTags"
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"EditorScriptingUtilities",
				"EditorSubsystem",
				"Slate",
				"SlateCore",
				"UMG",
				"Kismet",
				"KismetCompiler",
				"BlueprintGraph",
				"Projects",
				"AssetRegistry",
				"MaterialEditor",         // For UMaterialEditingLibrary (material creation/editing tools)
				"AudioEditor",            // For USoundClassFactory, USoundMixFactory (audio asset creation)
				"EnhancedInput",          // For UInputAction, UInputMappingContext (input asset creation)
				"LevelEditor",            // For ULevelEditorSubsystem (level management)
				"LevelSequence",          // For ULevelSequence, ULevelSequencePlayer (sequence tools)
				"MovieScene",             // For UMovieScene, FMovieScenePossessable (sequence binding)
				"Niagara",                // For UNiagaraSystem, UNiagaraEmitter
				"NiagaraEditor",          // For Niagara asset factories
				"Landscape",              // For ALandscapeProxy, ULandscapeInfo
				"Foliage",                // For UFoliageType, AInstancedFoliageActor
				"AnimGraph",              // For UAnimGraphNode_Base, UAnimationGraphSchema (AnimGraph editing)
				"AnimGraphRuntime",       // For FAnimNode_* runtime structs
				"PythonScriptPlugin",     // For IPythonScriptPlugin, FPythonCommandEx (execute_python tool)
				"Chooser",                // For UChooserTable, column types (ChooserTable editing tools)
			}
		);

		// Optional: GMC plugin support — adds GMC-specific telemetry fields to the
		// PIE driver (gmc_movement_mode + active_tags from UGMC_AbilitySystemComponent).
		// Auto-detected by scanning the project's Plugins/ folder for any GMC*.uplugin.
		// When absent, the PIE driver omits these fields; everything else still works.
		bool bGMCAvailable = false;
		if (Target.ProjectFile != null)
		{
			string ProjectPluginsDir = System.IO.Path.Combine(
				Target.ProjectFile.Directory.FullName, "Plugins");
			if (System.IO.Directory.Exists(ProjectPluginsDir))
			{
				foreach (string PluginFile in System.IO.Directory.EnumerateFiles(
					ProjectPluginsDir, "GMC*.uplugin", System.IO.SearchOption.AllDirectories))
				{
					bGMCAvailable = true;
					break;
				}
			}
		}
		if (bGMCAvailable)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "GMCCore", "GMCAbilitySystem" });
			PublicDefinitions.Add("UNREALMCP_WITH_GMC=1");
		}
		else
		{
			PublicDefinitions.Add("UNREALMCP_WITH_GMC=0");
		}
		
		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"PropertyEditor",      // For widget property editing
					"ToolMenus",           // For editor UI
					"BlueprintEditorLibrary", // For Blueprint utilities
					"UMGEditor"           // For WidgetBlueprint.h and other UMG editor functionality
				}
			);
		}
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
		);
	}
} 