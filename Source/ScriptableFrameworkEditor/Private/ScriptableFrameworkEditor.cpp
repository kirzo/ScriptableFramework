// Copyright 2025 kirzo

#pragma once

#include "ScriptableFrameworkEditor.h"
#include "ScriptableTypeCache.h"
#include "ScriptableFrameworkEditorStyle.h"
#include "AssetTools/AssetTypeActions_ScriptableObjectBase.h"

#include "ScriptableTasks/ScriptableTask.h"
#include "ScriptableTasks/ScriptableTaskAsset.h"
#include "ScriptableConditions/ScriptableCondition.h"
#include "ScriptableConditions/ScriptableConditionAsset.h"

#include "ScriptableFrameworkEd/Customization/ScriptableObjectCustomization.h"
#include "ScriptableFrameworkEd/Customization/ScriptableConditionCustomization.h"

#include "AssetToolsModule.h"

#define LOCTEXT_NAMESPACE "FScriptableFrameworkEditorModule"

void FScriptableFrameworkEditorModule::StartupModule()
{
	FScriptableFrameworkEditorStyle::Initialize();

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	ScriptableAssetCategoryBit = AssetTools.RegisterAdvancedAssetCategory("ScriptableFramework", INVTEXT("Scriptable Framework"));

	RegisterAssetTools();
	RegisterPropertyLayouts();
}

void FScriptableFrameworkEditorModule::ShutdownModule()
{
	UnregisterAssetTools();
	UnregisterPropertyLayouts();

	FScriptableFrameworkEditorStyle::Shutdown();
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

template<typename TPropertyType>
void FScriptableFrameworkEditorModule::RegisterPropertyLayout(FPropertyEditorModule& PropertyEditorModule, const FName TypeName)
{
	RegisteredPropertyLayouts.AddUnique(TypeName);
	PropertyEditorModule.RegisterCustomPropertyTypeLayout(TypeName, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&TPropertyType::MakeInstance));
}

void FScriptableFrameworkEditorModule::RegisterAssetTools()
{
	RegisterAssetTypeAction<UScriptableTaskAsset>(INVTEXT("Scriptable Task"), FColor(0, 169, 255));
	RegisterAssetTypeAction<UScriptableConditionAsset>(INVTEXT("Scriptable Condition"), FColor(145, 2, 23));
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

void FScriptableFrameworkEditorModule::RegisterPropertyLayouts()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	RegisterPropertyLayout<FScriptableObjectCustomization>(PropertyEditorModule, UScriptableTask::StaticClass()->GetFName());
	RegisterPropertyLayout<FScriptableConditionCustomization>(PropertyEditorModule, UScriptableCondition::StaticClass()->GetFName());
}

void FScriptableFrameworkEditorModule::UnregisterPropertyLayouts()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		for (const FName TypeName : RegisteredPropertyLayouts)
		{
			PropertyEditorModule.UnregisterCustomPropertyTypeLayout(TypeName);
		}
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FScriptableFrameworkEditorModule, ScriptableFrameworkEditor);