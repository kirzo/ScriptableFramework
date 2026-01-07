// Copyright 2025 kirzo

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "AssetTypeCategories.h"

struct FScriptableTypeCache;

class FScriptableFrameworkEditorModule : public IModuleInterface
{
private:
	TSharedPtr<FScriptableTypeCache> ScriptableTypeCache;
	EAssetTypeCategories::Type ScriptableAssetCategoryBit = EAssetTypeCategories::None;

	TArray<TSharedRef<class IAssetTypeActions>> RegisteredAssetTypeActions;
	TArray<FName> RegisteredClassLayouts;
	TArray<FName> RegisteredPropertyLayouts;

public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	TSharedPtr<FScriptableTypeCache> GetScriptableTypeCache();

private:
	template<typename T>
	void RegisterAssetTypeAction();

	template<typename T>
	void RegisterAssetTypeAction(const FText& Name, FColor Color);

	template<typename T>
	void RegisterClassLayout(FPropertyEditorModule& PropertyEditorModule, const FName ClassName);

	template<typename T>
	void RegisterPropertyLayout(FPropertyEditorModule& PropertyEditorModule, const FName TypeName);

	void RegisterAssetTools();
	void UnregisterAssetTools();

	void RegisterLayouts();
	void UnregisterLayouts();
};