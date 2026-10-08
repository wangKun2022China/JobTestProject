// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class JobTestProjectEditor : ModuleRules
{
	public JobTestProjectEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"Blutility",
			"UMG",
			"Slate",
			"SlateCore",
			"LevelSequence",
			"MovieScene",
			"MovieSceneTracks",
			"MovieRenderPipelineCore",
			"MovieRenderPipelineEditor",
			"MovieRenderPipelineRenderPasses",
			"AssetRegistry",
			"CinematicCamera",
			"Json",
			"JsonUtilities",
			"ToolMenus",
			"LevelEditor",
			"WorkspaceMenuStructure",
			"InputCore",
			"EditorScriptingUtilities",
			"PropertyEditor",
			"ScriptableEditorWidgets",
			"ControlRig",
			"RigLogicModule",
			"RigLogicLib",
			"AnimationCore",
			"MetaHumanCoreTech"
		});
	}
}
