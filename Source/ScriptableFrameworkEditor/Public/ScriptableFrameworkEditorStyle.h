// Copyright 2026 kirzo

#pragma once

#include "Styling/SlateStyle.h"

class SCRIPTABLEFRAMEWORKEDITOR_API FScriptableFrameworkEditorStyle : public FSlateStyleSet
{
public:
	static FScriptableFrameworkEditorStyle& Get();

	static FColor ScriptableTaskColor;
	static FColor ScriptableConditionColor;

protected:
	friend class FScriptableFrameworkEditorModule;

	static void Register();
	static void Unregister();

private:
	FScriptableFrameworkEditorStyle();
};