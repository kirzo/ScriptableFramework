// Copyright 2026 kirzo

#include "Conditions/ScriptableCondition_CanActivateByTag.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"

bool UScriptableCondition_CanActivateByTag::Evaluate_Implementation() const
{
	// Ensure valid setup
	if (!IsValid(TargetActor) || !AbilityTag.IsValid())
	{
		return false;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	if (!ASC)
	{
		return false;
	}

	// Iterate through all abilities currently granted to the target
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		// Check if this ability has the required conceptual tag
		if (Spec.Ability && Spec.Ability->GetAssetTags().HasTagExact(AbilityTag))
		{
			// We found an ability that represents this interaction concept!
			// Now we ask GAS if it meets all requirements to activate right now.
			if (Spec.Ability->CanActivateAbility(Spec.Handle, ASC->AbilityActorInfo.Get()))
			{
				return true;
			}
		}
	}

	// No matching ability found, or none could be activated
	return false;
}

#if WITH_EDITOR
FText UScriptableCondition_CanActivateByTag::GetDisplayTitle() const
{
	FString TargetName;
	if (!GetBindingDisplayText(GET_MEMBER_NAME_CHECKED(UScriptableCondition_CanActivateByTag, TargetActor), TargetName))
	{
		TargetName = TargetActor ? TargetActor->GetActorLabel() : TEXT("None");
	}

	if (AbilityTag.IsValid())
	{
		return FText::Format(INVTEXT("Can {0} Activate GA [{1}]"), FText::FromString(TargetName), FText::FromName(AbilityTag.GetTagName()));
	}

	return INVTEXT("Can Activate Ability By Tag");
}
#endif