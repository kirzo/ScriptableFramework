// Copyright 2026 kirzo

#include "ScriptableFrameworkEditorStyle.h"

FLinearColor FScriptableFrameworkEditorStyle::ScriptableTaskColor = FLinearColor(0.0f, 0.66f, 1.0f);
FLinearColor FScriptableFrameworkEditorStyle::ScriptableConditionColor = FLinearColor(0.57f, 0.0f, 0.09f);

FLinearColor FScriptableFrameworkEditorStyle::InactiveColor = FLinearColor(0.2f, 0.2f, 0.2f, 0.5f);
FLinearColor FScriptableFrameworkEditorStyle::NegateColor = FLinearColor(0.8f, 0.1f, 0.1f);

FLinearColor FScriptableFrameworkEditorStyle::ContextColor = FLinearColor(0.32f, 0.65f, 0.52f);

FScriptableFrameworkEditorStyle::FScriptableFrameworkEditorStyle()
	: TKzEditorStyle_Base(FName("ScriptableFrameworkEditorStyle"))
{
	SetupPluginResources(TEXT("ScriptableFramework"));

	AddClassIcon(TEXT("ScriptableActionAsset"), TEXT("ScriptableTask_16x"));
	AddClassThumbnail(TEXT("ScriptableActionAsset"), TEXT("ScriptableTask_64x"));

	AddClassIcon(TEXT("ScriptableRequirementAsset"), TEXT("ScriptableCondition_16x"));
	AddClassThumbnail(TEXT("ScriptableRequirementAsset"), TEXT("ScriptableCondition_64x"));

	// Parameter labels
	const FTextBlockStyle& NormalText = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");

	Set("ScriptableFramework.Param.Label", FTextBlockStyle(NormalText)
								.SetFont(FAppStyle::GetFontStyle(TEXT("PropertyWindow.BoldFont")))
								.SetFontSize(8));

	Set("ScriptableFramework.Param.Background", new FSlateRoundedBoxBrush(FStyleColors::Hover, 6.f));
}