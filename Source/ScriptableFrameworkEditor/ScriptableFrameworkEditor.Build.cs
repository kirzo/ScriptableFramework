// Copyright 2025 kirzo

using UnrealBuildTool;

public class ScriptableFrameworkEditor : ModuleRules
{
    public ScriptableFrameworkEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "ScriptableFramework",
                "PropertyAccessEditor",
                "PropertyBindingUtils"
            }
            );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "UnrealEd",
                "PropertyEditor",
                "InputCore",
                "Projects",
                "BlueprintGraph",
                "KismetWidgets"
            }
            );
    }
}