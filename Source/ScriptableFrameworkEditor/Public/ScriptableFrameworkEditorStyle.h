// Copyright 2026 kirzo

#pragma once

#include "Styling/SlateStyle.h"

class SCRIPTABLEFRAMEWORKEDITOR_API FScriptableFrameworkEditorStyle : public FSlateStyleSet
{
public:
	static FScriptableFrameworkEditorStyle& Get();

	static FLinearColor ScriptableTaskColor;
	static FLinearColor ScriptableConditionColor;

	static FLinearColor InactiveColor;
	static FLinearColor NegateColor;

protected:
	friend class FScriptableFrameworkEditorModule;

	static void Register();
	static void Unregister();

private:
	FScriptableFrameworkEditorStyle();
};