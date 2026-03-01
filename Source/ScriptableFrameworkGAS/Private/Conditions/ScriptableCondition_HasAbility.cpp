// Copyright 2026 kirzo

#include "Conditions/ScriptableCondition_HasAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"

bool UScriptableCondition_HasAbility::Evaluate_Implementation() const
{
	// Ensure we have a valid ability class to look for
	if (!IsValid(TargetActor) || !IsValid(AbilityClass))
	{
		return false;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	if (!ASC)
	{
		return false;
	}

	// Search for the spec corresponding to the granted ability class
	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(AbilityClass);

	if (!Spec)
	{
		// The actor does not have this ability granted
		return false;
	}

	// If the user requested it to be active, check the spec's active state
	if (bMustBeActive)
	{
		return Spec->IsActive();
	}

	// Just having the ability granted is enough to pass the condition
	return true;
}

#if WITH_EDITOR
FText UScriptableCondition_HasAbility::GetDisplayTitle() const
{
	FString TargetName;
	if (!GetBindingDisplayText(GET_MEMBER_NAME_CHECKED(UScriptableCondition_HasAbility, TargetActor), TargetName))
	{
		TargetName = TargetActor ? TargetActor->GetActorLabel() : TEXT("None");
	}

	if (AbilityClass)
	{
		return FText::Format(INVTEXT("Does {0} have Ability [{1}]"), FText::FromString(TargetName), AbilityClass->GetDisplayNameText());
	}

	return INVTEXT("Has Gameplay Ability");
}
#endif