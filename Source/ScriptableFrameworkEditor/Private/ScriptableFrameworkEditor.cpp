// Copyright 2026 kirzo

#pragma once

#include "ScriptableFrameworkEditor.h"
#include "ScriptableTypeCache.h"
#include "ScriptableFrameworkEditorStyle.h"

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

#define LOCTEXT_NAMESPACE "FScriptableFrameworkEditorModule"

void FScriptableFrameworkEditorModule::OnStartupModule()
{
	FScriptableFrameworkEditorStyle::Register();

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	ScriptableAssetCategoryBit = AssetTools.RegisterAdvancedAssetCategory("ScriptableFramework", INVTEXT("Scriptable Framework"));

	RegisterAssetTypeAction<UScriptableActionAsset>(ScriptableAssetCategoryBit, INVTEXT("Scriptable Action"), FScriptableFrameworkEditorStyle::ScriptableTaskColor.ToFColor(true));
	RegisterAssetTypeAction<UScriptableRequirementAsset>(ScriptableAssetCategoryBit, INVTEXT("Scriptable Requirement"), FScriptableFrameworkEditorStyle::ScriptableConditionColor.ToFColor(true));

	RegisterPropertyLayout<UScriptableTask, FScriptableTaskCustomization>();
	RegisterPropertyLayout<UScriptableCondition, FScriptableConditionCustomization>();
	RegisterPropertyLayout<FScriptableAction, FScriptableActionCustomization>();
	RegisterPropertyLayout<FScriptableRequirement, FScriptableRequirementCustomization>();
	RegisterPropertyLayout<UScriptableCondition_Group, FScriptableConditionGroupCustomization>();
}

void FScriptableFrameworkEditorModule::OnShutdownModule()
{
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

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FScriptableFrameworkEditorModule, ScriptableFrameworkEditor);