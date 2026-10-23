// Copyright 2026 kirzo

#include "Tasks/ScriptableTask_SetLimbicSlotByQuery.h"
#include "Components/KzDatabaseComponent.h"
#include "GameFramework/Actor.h"
#include "LimbicPuppeteer.h"

void UScriptableTask_SetLimbicSlotByQuery::BeginTask()
{
	Handle = FLimbicSlotHandle();

	if (IsValid(TargetActor))
	{
		if (ULimbicPuppeteer* Puppeteer = UKzDatabaseComponent::ResolveFor(SourceActor.Get(), TargetActor.Get(), Query, TSoftObjectPtr<ULimbicPuppeteer>()).LoadSynchronous())
		{
			FLimbicSlotParameters SlotParameters;
			SlotParameters.Values = Parameters;
			Handle = ULimbicSlotLibrary::SetLimbicSlotOnActor(TargetActor, Slot, Puppeteer, BlendTime, SlotParameters);
		}
	}

	// Holding nothing there is nothing to wait for
	if (!bHoldUntilStopped || !Handle.IsValid())
	{
		Finish();
	}
}

void UScriptableTask_SetLimbicSlotByQuery::StopTask()
{
	ULimbicSlotLibrary::StopLimbicSlotByHandle(Handle, BlendTime);
	Handle = FLimbicSlotHandle();
}

#if WITH_EDITOR
FText UScriptableTask_SetLimbicSlotByQuery::GetDisplayTitle() const
{
	const FText SlotName = Slot.IsValid() ? FText::FromName(Slot.GetTagLeafName()) : INVTEXT("None");
	const FText Asked = Query.RequireTags.IsEmpty() ? INVTEXT("Empty Query") : FText::FromName(Query.RequireTags.First().GetTagName());
	return FText::Format(bHoldUntilStopped ? INVTEXT("Hold Limbic Slot {0} by {1}") : INVTEXT("Set Limbic Slot {0} by {1}"), SlotName, Asked);
}

void UScriptableTask_SetLimbicSlotByQuery::AddParametersFrom(ULimbicPuppeteer* Puppeteer)
{
	const UPropertyBag* Compiled = Puppeteer ? Puppeteer->CompiledParameters.GetPropertyBagStruct() : nullptr;
	if (!Compiled)
	{
		return;
	}

	TArray<FPropertyBagPropertyDesc> Missing;
	for (const FPropertyBagPropertyDesc& Desc : Compiled->GetPropertyDescs())
	{
		if (ULimbicPuppeteer::IsParameterExposed(Desc) && !Parameters.FindPropertyDescByName(Desc.Name))
		{
			Missing.Add(Desc);
		}
	}

	if (!Missing.IsEmpty())
	{
		// The new ones take the puppeteer's defaults; the ones already there keep their values.
		FInstancedPropertyBag Defaults;
		Defaults.AddProperties(Missing);
		Defaults.CopyMatchingValuesByID(Puppeteer->CompiledParameters);
		Parameters.AddProperties(Missing);
		Parameters.CopyMatchingValuesByName(Defaults);
	}
}
#endif