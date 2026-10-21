// Copyright 2026 kirzo

#pragma once

#include "Styling/SlateStyle.h"
#include "Textures/SlateIcon.h"
#include "KzEditorStyle_Base.h"

class SCRIPTABLEFRAMEWORKEDITOR_API FScriptableFrameworkEditorStyle : public TKzEditorStyle_Base<FScriptableFrameworkEditorStyle>
{
public:
	FScriptableFrameworkEditorStyle();

	/** The palette of the plugin icons, one colour per kind of block; graph nodes, asset strips and icon tints take theirs from here. */
	static FLinearColor EventColor;
	static FLinearColor ActionColor;
	static FLinearColor GraphColor;
	static FLinearColor TaskColor;
	static FLinearColor RequirementColor;
	static FLinearColor ConditionColor;
	static FLinearColor FlowColor;
	static FLinearColor ExitColor;

	static FLinearColor InactiveColor;
	static FLinearColor NegateColor;

	static FLinearColor ContextColor;

	/** The icon of a task or condition class, tinted like the blocks they are (TaskColor, ConditionColor); unset for anything else. */
	static FSlateIcon GetScriptableObjectIcon(const UClass* Class, FLinearColor& OutTint);
};