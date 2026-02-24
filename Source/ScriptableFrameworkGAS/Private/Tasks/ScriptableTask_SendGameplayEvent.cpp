// Copyright 2026 kirzo

#include "Tasks/ScriptableTask_SendGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameFramework/Actor.h"

void UScriptableTask_SendGameplayEvent::BeginTask()
{
	if (IsValid(TargetActor) && EventTag.IsValid())
	{
		// Construct the payload dynamically using our bound properties
		FGameplayEventData Payload;
		Payload.EventTag = EventTag;
		Payload.EventMagnitude = EventMagnitude;
		Payload.Instigator = Instigator;

		// If an explicit target is set in the data, use it. Otherwise, use the actor receiving the event.
		Payload.Target = Target ? Target.Get() : TargetActor.Get();

		Payload.OptionalObject = OptionalObject;
		Payload.OptionalObject2 = OptionalObject2;

		// Dispatch the event
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(TargetActor, EventTag, Payload);
	}

	Finish();
}

#if WITH_EDITOR
FText UScriptableTask_SendGameplayEvent::GetDisplayTitle() const
{
	FString TargetName;

	// Try to get the name of the bound variable (e.g. "Context.Target")
	if (!GetBindingDisplayText(GET_MEMBER_NAME_CHECKED(UScriptableTask_SendGameplayEvent, TargetActor), TargetName))
	{
		// Fallback to the literal actor's name if placed in the level, or "None"
		TargetName = TargetActor ? TargetActor->GetActorLabel() : TEXT("None");
	}

	if (EventTag.IsValid())
	{
		return FText::Format(INVTEXT("Send Event [{0}] to {1}"), FText::FromName(EventTag.GetTagName()), FText::FromString(TargetName));
	}

	return INVTEXT("Send Gameplay Event");
}
#endif