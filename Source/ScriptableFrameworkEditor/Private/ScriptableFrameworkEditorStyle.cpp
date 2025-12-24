// Copyright 2025 kirzo

#include "ScriptableFrameworkEditorStyle.h"

#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( StyleSet->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( StyleSet->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( StyleSet->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )

TSharedPtr<FSlateStyleSet> FScriptableFrameworkEditorStyle::StyleSet = nullptr;

FName FScriptableFrameworkEditorStyle::GetStyleSetName()
{
	static FName ScriptableFrameworkEditorStyleName(TEXT("ScriptableFrameworkEditorStyle"));
	return ScriptableFrameworkEditorStyleName;
}

void FScriptableFrameworkEditorStyle::Initialize()
{
	StyleSet = MakeShareable(new FSlateStyleSet(TEXT("ScriptableFrameworkEditorStyle")));

	const FVector2D Icon16(16.0f, 16.0f);
	const FVector2D Icon20(20.0f, 20.0f);
	const FVector2D Icon30(30.0f, 30.0f);
	const FVector2D Icon40(40.0f, 40.0f);
	const FVector2D Icon64(64.0f, 64.0f);

	// Engine assets
	StyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate/"));
	StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	// Scriptable Framework assets
	StyleSet->SetContentRoot(IPluginManager::Get().FindPlugin("ScriptableFramework")->GetBaseDir() / TEXT("Resources"));

	StyleSet->Set("ClassIcon.ScriptableTaskAsset", new IMAGE_BRUSH(TEXT("ScriptableTask_16x"), Icon16));
	StyleSet->Set("ClassThumbnail.ScriptableTaskAsset", new IMAGE_BRUSH(TEXT("ScriptableTask_64x"), Icon64));

	StyleSet->Set("ClassIcon.ScriptableConditionAsset", new IMAGE_BRUSH(TEXT("ScriptableCondition_16x"), Icon16));
	StyleSet->Set("ClassThumbnail.ScriptableConditionAsset", new IMAGE_BRUSH(TEXT("ScriptableCondition_64x"), Icon64));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
};

void FScriptableFrameworkEditorStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
	ensure(StyleSet.IsUnique());
	StyleSet.Reset();
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH