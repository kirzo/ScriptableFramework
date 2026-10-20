// Copyright 2026 kirzo

#pragma once

#include "Styling/SlateStyle.h"
#include "Textures/SlateIcon.h"
#include "KzEditorStyle_Base.h"

class SCRIPTABLEFRAMEWORKEDITOR_API FScriptableFrameworkEditorStyle : public TKzEditorStyle_Base<FScriptableFrameworkEditorStyle>
{
public:
	FScriptableFrameworkEditorStyle();

	static FLinearColor ScriptableTaskColor;
	static FLinearColor ScriptableConditionColor;
	static FLinearColor ScriptableGraphColor;

	static FLinearColor InactiveColor;
	static FLinearColor NegateColor;

	static FLinearColor ContextColor;

	static FLinearColor ScriptableEntryNodeColor;
	static FLinearColor ScriptableExitNodeColor;
	static FLinearColor ScriptableSystemNodeColor;

	/** The icon of a task or condition class, tinted like the blocks they are (green tasks, lilac conditions); unset for anything else. */
	static FSlateIcon GetScriptableObjectIcon(const UClass* Class, FLinearColor& OutTint);
};