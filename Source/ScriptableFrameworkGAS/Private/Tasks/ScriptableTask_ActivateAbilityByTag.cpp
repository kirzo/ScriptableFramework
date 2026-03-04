// Copyright 2026 kirzo

#include "Tasks/ScriptableTask_ActivateAbilityByTag.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

void UScriptableTask_ActivateAbilityByTag::BeginTask()
{
	if (IsValid(TargetActor) && AbilityTag.IsValid())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor))
		{
			// Create a container with our single tag
			FGameplayTagContainer TagContainer(AbilityTag);

			// Natively instruct GAS to attempt activation of any ability matching this tag.
			// The second parameter (bAllowRemoteActivation) is set to true to support networking.
			ASC->TryActivateAbilitiesByTag(TagContainer, true);
		}
	}

	// Always notify the framework that this task has finished its execution
	Finish();
}

#if WITH_EDITOR
FText UScriptableTask_ActivateAbilityByTag::GetDisplayTitle() const
{
	FString TargetName;

	if (!GetBindingDisplayText(GET_MEMBER_NAME_CHECKED(UScriptableTask_ActivateAbilityByTag, TargetActor), TargetName))
	{
		TargetName = TargetActor ? TargetActor->GetActorLabel() : TEXT("None");
	}

	if (AbilityTag.IsValid())
	{
		return FText::Format(INVTEXT("Activate GA [{0}] on {1}"), FText::FromName(AbilityTag.GetTagName()), FText::FromString(TargetName));
	}

	return INVTEXT("Activate Ability By Tag");
}
#endif