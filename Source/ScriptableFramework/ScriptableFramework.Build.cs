// Copyright 2026 kirzo

using UnrealBuildTool;

public class ScriptableFramework : ModuleRules
{
	public ScriptableFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"PropertyBindingUtils",
				"KzLib"
			});


		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore"
			});
	}
}