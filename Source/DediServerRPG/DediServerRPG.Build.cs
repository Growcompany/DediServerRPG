// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DediServerRPG : ModuleRules
{
	public DediServerRPG(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"AnimGraphRuntime",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"NetCore",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "Sockets" });

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd", "AnimationBlueprintLibrary", "AnimGraph", "BlueprintGraph",
				"BehaviorTreeEditor", "AIGraph"
			});
		}
	}
}
