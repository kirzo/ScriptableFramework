// Copyright 2026 kirzo

#include "Tasks/ScriptableTask_SetLimbicSlot.h"
#include "GameFramework/Actor.h"
#include "LimbicPuppeteer.h"

#if WITH_EDITOR
#include "UObject/ObjectSaveContext.h"
#endif

void UScriptableTask_SetLimbicSlot::BeginTask()
{
	Handle = FLimbicSlotHandle();

	if (IsValid(TargetActor) && Puppeteer)
	{
		FLimbicSlotParameters SlotParameters;
		if (const UPropertyBag* Bag = Parameters.GetPropertyBagStruct())
		{
			// Bound members go even when they hold the default, as wired pins do on the Blueprint node.
			TArray<FName> BoundNames;
			for (const FScriptablePropertyBinding& Binding : GetPropertyBindings().Bindings)
			{
				const FPropertyBindingPath& Path = Binding.TargetPath;
				if (Path.NumSegments() >= 2 && Path.GetSegment(0).GetName() == GET_MEMBER_NAME_CHECKED(UScriptableTask_SetLimbicSlot, Parameters))
				{
					BoundNames.Add(Path.GetSegment(1).GetName());
				}
			}

			const uint8* Values = Parameters.GetValue().GetMemory();
			const uint8* Defaults = Puppeteer->CompiledParameters.GetValue().GetMemory();
			TArray<FPropertyBagPropertyDesc> Sent;
			for (const FPropertyBagPropertyDesc& Desc : Bag->GetPropertyDescs())
			{
				const FPropertyBagPropertyDesc* Default = Puppeteer->CompiledParameters.FindPropertyDescByName(Desc.Name);
				const bool bChanged = !Default || !Default->CachedProperty || !Desc.CachedProperty->SameType(Default->CachedProperty)
					|| !Desc.CachedProperty->Identical(Desc.CachedProperty->ContainerPtrToValuePtr<void>(Values), Default->CachedProperty->ContainerPtrToValuePtr<void>(Defaults));
				if (bChanged || BoundNames.Contains(Desc.Name))
				{
					Sent.Add(Desc);
				}
			}

			if (!Sent.IsEmpty())
			{
				SlotParameters.Values.AddProperties(Sent);
				SlotParameters.Values.CopyMatchingValuesByID(Parameters);
			}
		}

		Handle = ULimbicSlotLibrary::SetLimbicSlotOnActor(TargetActor, Slot, Puppeteer, BlendTime, SlotParameters);
	}

	// Holding nothing there is nothing to wait for
	if (!bHoldUntilStopped || !Handle.IsValid())
	{
		Finish();
	}
}

void UScriptableTask_SetLimbicSlot::StopTask()
{
	ULimbicSlotLibrary::StopLimbicSlotByHandle(Handle, BlendTime);
	Handle = FLimbicSlotHandle();
}

#if WITH_EDITOR
FText UScriptableTask_SetLimbicSlot::GetDisplayTitle() const
{
	const FText SlotName = Slot.IsValid() ? FText::FromName(Slot.GetTagLeafName()) : INVTEXT("None");
	const FText PuppeteerName = Puppeteer ? FText::FromString(Puppeteer->GetName()) : INVTEXT("None");
	return FText::Format(bHoldUntilStopped ? INVTEXT("Hold Limbic Slot {0}: {1}") : INVTEXT("Set Limbic Slot {0}: {1}"), SlotName, PuppeteerName);
}

void UScriptableTask_SetLimbicSlot::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UScriptableTask_SetLimbicSlot, Puppeteer))
	{
		SyncParameters();
	}
}

void UScriptableTask_SetLimbicSlot::PreSave(FObjectPreSaveContext SaveContext)
{
	// Absorb puppeteer recompiles before bindings are baked and validated.
	SyncParameters();
	Super::PreSave(SaveContext);
}

void UScriptableTask_SetLimbicSlot::SyncParameters()
{
	TArray<FPropertyBagPropertyDesc> Exposed;
	if (const UPropertyBag* Compiled = Puppeteer ? Puppeteer->CompiledParameters.GetPropertyBagStruct() : nullptr)
	{
		for (const FPropertyBagPropertyDesc& Desc : Compiled->GetPropertyDescs())
		{
			if (ULimbicPuppeteer::IsParameterExposed(Desc))
			{
				Exposed.Add(Desc);
			}
		}
	}

	// Shape-compare first: rebuilding the bag would dirty the asset on every save.
	const UPropertyBag* Current = Parameters.GetPropertyBagStruct();
	const TConstArrayView<FPropertyBagPropertyDesc> CurrentDescs = Current ? Current->GetPropertyDescs() : TConstArrayView<FPropertyBagPropertyDesc>();
	bool bInSync = CurrentDescs.Num() == Exposed.Num();
	for (int32 i = 0; bInSync && i < Exposed.Num(); ++i)
	{
		bInSync = CurrentDescs[i].Name == Exposed[i].Name && CurrentDescs[i].CompatibleType(Exposed[i]) && CurrentDescs[i].ValueTypeObject == Exposed[i].ValueTypeObject;
	}
	if (bInSync)
	{
		return;
	}

	FInstancedPropertyBag Synced;
	if (!Exposed.IsEmpty())
	{
		Synced.AddProperties(Exposed);
		Synced.CopyMatchingValuesByID(Puppeteer->CompiledParameters);
		Synced.CopyMatchingValuesByName(Parameters);
	}
	Parameters = MoveTemp(Synced);
}
#endif