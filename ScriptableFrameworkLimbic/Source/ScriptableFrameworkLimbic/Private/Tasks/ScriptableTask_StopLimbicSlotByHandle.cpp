// Copyright 2026 kirzo

#include "Tasks/ScriptableTask_StopLimbicSlotByHandle.h"

void UScriptableTask_StopLimbicSlotByHandle::BeginTask()
{
	ULimbicSlotLibrary::StopLimbicSlotByHandle(Handle, BlendTime);
	Finish();
}

#if WITH_EDITOR
void UScriptableTask_StopLimbicSlotByHandle::ValidateObject(TArray<FKzValidationIssue>& OutIssues) const
{
	FPropertyBindingPath HandlePath;
	HandlePath.SetStructID(GetBindingID());
	HandlePath.AddPathSegment(GET_MEMBER_NAME_CHECKED(UScriptableTask_StopLimbicSlotByHandle, Handle));

	// A handle can only come from a binding, so an unbound one silently stops nothing.
	if (!GetPropertyBindings().HasPropertyBinding(HandlePath))
	{
		OutIssues.Add(FKzValidationIssue(EKzValidationSeverity::Warning,
			FText::Format(INVTEXT("'{0}': Handle is not bound to a Set Limbic Slot; the task will stop nothing."), FText::FromString(GetName())),
			TEXT("ScriptableLimbicSlot")));
	}
}
#endif