// Copyright 2025 kirzo

#include "ScriptableFrameworkEditorStyle.h"

#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )

FLinearColor FScriptableFrameworkEditorStyle::ScriptableTaskColor = FLinearColor(0.0f, 0.66f, 1.0f);
FLinearColor FScriptableFrameworkEditorStyle::ScriptableConditionColor = FLinearColor(0.57f, 0.0f, 0.09f);

FLinearColor FScriptableFrameworkEditorStyle::InactiveColor = FLinearColor(0.2f, 0.2f, 0.2f, 0.5f);
FLinearColor FScriptableFrameworkEditorStyle::NegateColor = FLinearColor(0.8f, 0.1f, 0.1f);

void FScriptableFrameworkEditorStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FScriptableFrameworkEditorStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}

FScriptableFrameworkEditorStyle& FScriptableFrameworkEditorStyle::Get()
{
	static FScriptableFrameworkEditorStyle Instance;
	return Instance;
}

FScriptableFrameworkEditorStyle::FScriptableFrameworkEditorStyle()
	: FSlateStyleSet(TEXT("ScriptableFrameworkEditorStyle"))
{
	const FVector2D Icon16(16.0f, 16.0f);
	const FVector2D Icon20(20.0f, 20.0f);
	const FVector2D Icon30(30.0f, 30.0f);
	const FVector2D Icon40(40.0f, 40.0f);
	const FVector2D Icon64(64.0f, 64.0f);

	// Engine assets
	SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate/"));
	SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	// Scriptable Framework assets
	SetContentRoot(IPluginManager::Get().FindPlugin("ScriptableFramework")->GetBaseDir() / TEXT("Resources"));

	Set("ClassIcon.ScriptableActionAsset", new IMAGE_BRUSH(TEXT("ScriptableTask_16x"), Icon16));
	Set("ClassThumbnail.ScriptableActionAsset", new IMAGE_BRUSH(TEXT("ScriptableTask_64x"), Icon64));

	Set("ClassIcon.ScriptableRequirementAsset", new IMAGE_BRUSH(TEXT("ScriptableCondition_16x"), Icon16));
	Set("ClassThumbnail.ScriptableRequirementAsset", new IMAGE_BRUSH(TEXT("ScriptableCondition_64x"), Icon64));

	// Parameter labels
	const FTextBlockStyle& NormalText = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");

	Set("ScriptableFramework.Param.Label", FTextBlockStyle(NormalText)
								.SetFont(FAppStyle::GetFontStyle(TEXT("PropertyWindow.BoldFont")))
								.SetFontSize(8));

	Set("ScriptableFramework.Param.Background", new FSlateRoundedBoxBrush(FStyleColors::Hover, 6.f));
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH