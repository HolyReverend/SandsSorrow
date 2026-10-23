// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SandsOfSorrow : ModuleRules
{
	public SandsOfSorrow(ReadOnlyTargetRules Target) : base(Target)
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
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "SlateCore" });

		PublicIncludePaths.AddRange(new string[] {
			"SandsOfSorrow",
			"SandsOfSorrow/Template",
			"SandsOfSorrow/Template/ThirdPerson",
			"SandsOfSorrow/Template/Variant_Platforming",
			"SandsOfSorrow/Template/Variant_Platforming/Animation",
			"SandsOfSorrow/Template/Variant_Combat",
			"SandsOfSorrow/Template/Variant_Combat/AI",
			"SandsOfSorrow/Template/Variant_Combat/Animation",
			"SandsOfSorrow/Template/Variant_Combat/Gameplay",
			"SandsOfSorrow/Template/Variant_Combat/Interfaces",
			"SandsOfSorrow/Template/Variant_Combat/UI",
			"SandsOfSorrow/Template/Variant_SideScrolling",
			"SandsOfSorrow/Template/Variant_SideScrolling/AI",
			"SandsOfSorrow/Template/Variant_SideScrolling/Gameplay",
			"SandsOfSorrow/Template/Variant_SideScrolling/Interfaces",
			"SandsOfSorrow/Template/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
