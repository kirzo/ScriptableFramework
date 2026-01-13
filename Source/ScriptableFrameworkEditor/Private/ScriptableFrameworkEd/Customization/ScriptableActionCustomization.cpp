// Copyright 2026 kirzo

#include "ScriptableActionCustomization.h"
#include "ScriptableTasks/ScriptableTask.h"
#include "ScriptableTasks/ScriptableAction.h"
#include "ScriptableTasks/ScriptableActionAsset.h"
#include "ScriptableFrameworkEditorStyle.h"

#define LOCTEXT_NAMESPACE "FScriptableActionCustomization"

UClass* FScriptableActionCustomization::GetBaseClass() const
{
	return UScriptableTask::StaticClass();
}

FName FScriptableActionCustomization::GetListPropertyName() const
{
	return GET_MEMBER_NAME_CHECKED(FScriptableAction, Tasks);
}

FName FScriptableActionCustomization::GetModePropertyName() const
{
	return GET_MEMBER_NAME_CHECKED(FScriptableAction, Mode);
}

FSlateColor FScriptableActionCustomization::GetIconColor() const
{
	return FScriptableFrameworkEditorStyle::ScriptableTaskColor;
}

FText FScriptableActionCustomization::GetAddButtonTooltip() const
{
	return LOCTEXT("AddTaskTooltip", "Add new Task.");
}

UClass* FScriptableActionCustomization::GetWrapperClass() const
{
	return UScriptableTask_RunAsset::StaticClass();
}

FText FScriptableActionCustomization::GetModeText() const
{
	uint8 Value;
	if (ModeHandle.IsValid() && ModeHandle->GetValue(Value) == FPropertyAccess::Success)
	{
		const EScriptableActionMode Mode = (EScriptableActionMode)Value;
		switch (Mode)
		{
			case EScriptableActionMode::Sequence: return LOCTEXT("ModeSequence", "Sequence");
			case EScriptableActionMode::Parallel: return LOCTEXT("ModeParallel", "Parallel");
		}
	}
	return FText::GetEmpty();
}

FSlateColor FScriptableActionCustomization::GetModeColor() const
{
	uint8 Value;
	if (ModeHandle.IsValid() && ModeHandle->GetValue(Value) == FPropertyAccess::Success)
	{
		const EScriptableActionMode Mode = (EScriptableActionMode)Value;
		// Green for Sequence, Blue/Purple for Parallel
		return Mode == EScriptableActionMode::Sequence ? FLinearColor(0.02f, 0.4f, 0.1f) : FLinearColor(0.1f, 0.2f, 0.4f);
	}
	return FSlateColor::UseForeground();
}

FText FScriptableActionCustomization::GetModeTooltip() const
{
	return LOCTEXT("ToggleModeTooltip", "Toggle between Sequence and Parallel execution.");
}

#undef LOCTEXT_NAMESPACE