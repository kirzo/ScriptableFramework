// Copyright 2026 kirzo

#include "Tasks/ScriptableTask_StopLimbicSlot.h"
#include "GameFramework/Actor.h"
#include "LimbicSlotLibrary.h"

void UScriptableTask_StopLimbicSlot::BeginTask()
{
	if (IsValid(TargetActor))
	{
		ULimbicSlotLibrary::StopLimbicSlotOnActor(TargetActor, Slot, BlendTime);
	}

	Finish();
}

#if WITH_EDITOR
FText UScriptableTask_StopLimbicSlot::GetDisplayTitle() const
{
	const FText SlotName = Slot.IsValid() ? FText::FromName(Slot.GetTagLeafName()) : INVTEXT("None");
	return FText::Format(INVTEXT("Stop Limbic Slot {0}"), SlotName);
}
#endif