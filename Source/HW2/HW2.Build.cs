// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HW2 : ModuleRules
{
	public HW2(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"HW2",
			"HW2/Variant_Platforming",
			"HW2/Variant_Platforming/Animation",
			"HW2/Variant_Combat",
			"HW2/Variant_Combat/AI",
			"HW2/Variant_Combat/Animation",
			"HW2/Variant_Combat/Gameplay",
			"HW2/Variant_Combat/Interfaces",
			"HW2/Variant_Combat/UI",
			"HW2/Variant_SideScrolling",
			"HW2/Variant_SideScrolling/AI",
			"HW2/Variant_SideScrolling/Gameplay",
			"HW2/Variant_SideScrolling/Interfaces",
			"HW2/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
