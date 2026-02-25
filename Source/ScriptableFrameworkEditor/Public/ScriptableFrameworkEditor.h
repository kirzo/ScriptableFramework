// Copyright 2026 kirzo

#pragma once

#include "KzLibEditorModule_Base.h"

struct FScriptableTypeCache;

class FScriptableFrameworkEditorModule : public FKzLibEditorModule_Base
{
private:
	TSharedPtr<FScriptableTypeCache> ScriptableTypeCache;
	EAssetTypeCategories::Type ScriptableAssetCategoryBit = EAssetTypeCategories::None;

protected:
	virtual void OnStartupModule() override;
	virtual void OnShutdownModule() override;

public:
	TSharedPtr<FScriptableTypeCache> GetScriptableTypeCache();
};