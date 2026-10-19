// Copyright 2026 kirzo

using UnrealBuildTool;

public class ScriptableFrameworkLimbic : ModuleRules
{
	public ScriptableFrameworkLimbic(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"GameplayTags",
				"KzLib",
				"Limbic"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"ScriptableFramework"
			}
			);
	}
}