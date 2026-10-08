using UnrealBuildTool;
using System.IO;
using System;

public class NodeUpgrade : ModuleRules
{
	public NodeUpgrade(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		// FactoryGame transitive dependencies (copied from the Alpakit "C++ & Blueprint" template, SML 3.12.0)
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject",
			"Engine",
			"DeveloperSettings",
			"PhysicsCore",
			"InputCore",
			"GeometryCollectionEngine",
			"AnimGraphRuntime",
			"AssetRegistry",
			"NavigationSystem",
			"AIModule",
			"GameplayTasks",
			"SlateCore", "Slate", "UMG",
			"RenderCore",
			"CinematicCamera",
			"Foliage",
			"EnhancedInput",
			"NetCore",
			"GameplayTags",
			"Json", "JsonUtilities"
		});

		// Header stubs
		PublicDependencyModuleNames.AddRange(new string[] {
			"DummyHeaders",
		});

		if (Target.Type == TargetRules.TargetType.Editor) {
			PublicDependencyModuleNames.AddRange(new string[] {"AnimGraph"});
		}
		PublicDependencyModuleNames.AddRange(new string[] {"FactoryGame", "SML"});

		// IPluginManager, used to locate Resources/costs.json
		PrivateDependencyModuleNames.AddRange(new string[] {
			"Projects"
		});
	}
}
