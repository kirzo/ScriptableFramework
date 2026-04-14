// Copyright 2026 kirzo

using UnrealBuildTool;

public class ScriptableFrameworkAI : ModuleRules
{
	public ScriptableFrameworkAI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"StateTreeModule",
				"GameplayStateTreeModule"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"AIModule",
				"ScriptableFramework"
			}
			);
	}
}