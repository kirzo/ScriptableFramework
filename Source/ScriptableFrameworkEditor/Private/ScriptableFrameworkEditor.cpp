// Copyright 2025 kirzo

#pragma once

#include "ScriptableFrameworkEditor.h"
#include "ScriptableTypeCache.h"
#include "ScriptableFrameworkEditorStyle.h"
#include "AssetTools/AssetTypeActions_ScriptableObjectBase.h"

#include "ScriptableTasks/ScriptableTask.h"
#include "ScriptableTasks/ScriptableActionAsset.h"
#include "ScriptableTasks/ScriptableAction.h"

#include "ScriptableConditions/ScriptableCondition.h"
#include "ScriptableConditions/ScriptableRequirementAsset.h"
#include "ScriptableConditions/ScriptableRequirement.h"

#include "ScriptableFrameworkEd/Customization/ScriptableTaskCustomization.h"
#include "ScriptableFrameworkEd/Customization/ScriptableActionCustomization.h"

#include "ScriptableFrameworkEd/Customization/ScriptableRequirementCustomization.h"
#include "ScriptableFrameworkEd/Customization/ScriptableConditionCustomization.h"

#include "ScriptableConditions/ScriptableCondition_Group.h"
#include "ScriptableFrameworkEd/Customization/ScriptableConditionGroupCustomization.h"

#include "Bindings/ScriptableParameterDef.h"
#include "ScriptableFrameworkEd/Customization/ScriptableParameterDefCustomization.h"

#include "AssetToolsModule.h"

#define LOCTEXT_NAMESPACE "FScriptableFrameworkEditorModule"

void FScriptableFrameworkEditorModule::StartupModule()
{
	FScriptableFrameworkEditorStyle::Register();

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	ScriptableAssetCategoryBit = AssetTools.RegisterAdvancedAssetCategory("ScriptableFramework", INVTEXT("Scriptable Framework"));

	RegisterAssetTools();
	RegisterLayouts();
}

void FScriptableFrameworkEditorModule::ShutdownModule()
{
	UnregisterAssetTools();
	UnregisterLayouts();

	FScriptableFrameworkEditorStyle::Unregister();
}

TSharedPtr<FScriptableTypeCache> FScriptableFrameworkEditorModule::GetScriptableTypeCache()
{
	if (!ScriptableTypeCache.IsValid())
	{
		ScriptableTypeCache = MakeShareable(new FScriptableTypeCache());
		ScriptableTypeCache->AddRootClass(UScriptableTask::StaticClass());
		ScriptableTypeCache->AddRootClass(UScriptableCondition::StaticClass());
	}

	return ScriptableTypeCache;
}

template<typename T>
void FScriptableFrameworkEditorModule::RegisterAssetTypeAction()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	const auto Action = MakeShared<T>(ScriptableAssetCategoryBit);
	AssetTools.RegisterAssetTypeActions(Action);
	RegisteredAssetTypeActions.Add(Action);
}

template<typename T>
void FScriptableFrameworkEditorModule::RegisterAssetTypeAction(const FText& Name, FColor Color)
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	const auto Action = MakeShared<FAssetTypeActions_ScriptableObject>(ScriptableAssetCategoryBit, Name, Color, T::StaticClass());
	AssetTools.RegisterAssetTypeActions(Action);
	RegisteredAssetTypeActions.Add(Action);
}

template<typename T>
void FScriptableFrameworkEditorModule::RegisterClassLayout(FPropertyEditorModule& PropertyEditorModule, const FName ClassName)
{
	RegisteredClassLayouts.AddUnique(ClassName);
	PropertyEditorModule.RegisterCustomClassLayout(ClassName, FOnGetDetailCustomizationInstance::CreateStatic(&T::MakeInstance));
}

template<typename T>
void FScriptableFrameworkEditorModule::RegisterPropertyLayout(FPropertyEditorModule& PropertyEditorModule, const FName TypeName)
{
	RegisteredPropertyLayouts.AddUnique(TypeName);
	PropertyEditorModule.RegisterCustomPropertyTypeLayout(TypeName, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&T::MakeInstance));
}

void FScriptableFrameworkEditorModule::RegisterAssetTools()
{
	RegisterAssetTypeAction<UScriptableActionAsset>(INVTEXT("Scriptable Action"), FScriptableFrameworkEditorStyle::ScriptableTaskColor.ToFColor(true));
	RegisterAssetTypeAction<UScriptableRequirementAsset>(INVTEXT("Scriptable Requirement"), FScriptableFrameworkEditorStyle::ScriptableConditionColor.ToFColor(true));
}

void FScriptableFrameworkEditorModule::UnregisterAssetTools()
{
	if (auto* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools"))
	{
		IAssetTools& AssetTools = AssetToolsModule->Get();
		for (auto& Action : RegisteredAssetTypeActions)
		{
			AssetTools.UnregisterAssetTypeActions(Action);
		}
	}
}

void FScriptableFrameworkEditorModule::RegisterLayouts()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	RegisterPropertyLayout<FScriptableTaskCustomization>(PropertyEditorModule, UScriptableTask::StaticClass()->GetFName());
	RegisterPropertyLayout<FScriptableConditionCustomization>(PropertyEditorModule, UScriptableCondition::StaticClass()->GetFName());
	RegisterPropertyLayout<FScriptableActionCustomization>(PropertyEditorModule, FScriptableAction::StaticStruct()->GetFName());
	RegisterPropertyLayout<FScriptableRequirementCustomization>(PropertyEditorModule, FScriptableRequirement::StaticStruct()->GetFName());
	RegisterPropertyLayout<FScriptableConditionGroupCustomization>(PropertyEditorModule, UScriptableCondition_Group::StaticClass()->GetFName());
	RegisterPropertyLayout<FScriptableParameterDefCustomization>(PropertyEditorModule, FScriptableParameterDef::StaticStruct()->GetFName());
}

void FScriptableFrameworkEditorModule::UnregisterLayouts()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		for (const FName ClassName : RegisteredClassLayouts)
		{
			PropertyEditorModule.UnregisterCustomClassLayout(ClassName);
		}

		for (const FName TypeName : RegisteredPropertyLayouts)
		{
			PropertyEditorModule.UnregisterCustomPropertyTypeLayout(TypeName);
		}
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FScriptableFrameworkEditorModule, ScriptableFrameworkEditor);