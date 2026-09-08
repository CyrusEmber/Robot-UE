// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Robot : ModuleRules
{
	public Robot(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"PhysicsCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Robot",
			"Robot/RobotBuilder",
			"Robot/Variant_Platforming",
			"Robot/Variant_Platforming/Animation",
			"Robot/Variant_Combat",
			"Robot/Variant_Combat/AI",
			"Robot/Variant_Combat/Animation",
			"Robot/Variant_Combat/Gameplay",
			"Robot/Variant_Combat/Interfaces",
			"Robot/Variant_Combat/UI",
			"Robot/Variant_SideScrolling",
			"Robot/Variant_SideScrolling/AI",
			"Robot/Variant_SideScrolling/Gameplay",
			"Robot/Variant_SideScrolling/Interfaces",
			"Robot/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
