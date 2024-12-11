// Copyright Kirzo. All Rights Reserved.

using UnrealBuildTool;

public class ScriptableFramework : ModuleRules
{
    public ScriptableFramework(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
            });


        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
            });
    }
}