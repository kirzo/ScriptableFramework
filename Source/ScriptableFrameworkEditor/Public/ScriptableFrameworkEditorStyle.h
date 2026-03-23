// Copyright 2026 kirzo

#pragma once

#include "Styling/SlateStyle.h"
#include "KzEditorStyle_Base.h"

class SCRIPTABLEFRAMEWORKEDITOR_API FScriptableFrameworkEditorStyle : public TKzEditorStyle_Base<FScriptableFrameworkEditorStyle>
{
public:
	FScriptableFrameworkEditorStyle();

	static FLinearColor ScriptableTaskColor;
	static FLinearColor ScriptableConditionColor;

	static FLinearColor InactiveColor;
	static FLinearColor NegateColor;

	static FLinearColor ContextColor;
};