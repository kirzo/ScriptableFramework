// Copyright 2026 kirzo

#include "Tasks/ScriptableTask_GrantAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/GameplayAbility.h"

void UScriptableTask_GrantAbility::BeginTask()
{
	if (IsValid(TargetActor) && AbilityClass)
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor))
		{
			// Grant the ability and store the Handle
			FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, nullptr);
			GrantedHandle = ASC->GiveAbility(Spec);
		}
	}

	Finish();
}

void UScriptableTask_GrantAbility::ResetTask()
{
	if (bRevertOnReset && GrantedHandle.IsValid() && IsValid(TargetActor))
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor))
		{
			// Revoke the exact instance of the ability we granted
			ASC->ClearAbility(GrantedHandle);
			GrantedHandle = FGameplayAbilitySpecHandle(); // Invalidate the handle
		}
	}
}

#if WITH_EDITOR
FText UScriptableTask_GrantAbility::GetDisplayTitle() const
{
	FString TargetName;
	if (!GetBindingDisplayText(GET_MEMBER_NAME_CHECKED(UScriptableTask_GrantAbility, TargetActor), TargetName))
	{
		TargetName = TargetActor ? TargetActor->GetActorLabel() : TEXT("None");
	}

	if (AbilityClass)
	{
		return FText::Format(INVTEXT("Grant Ability [{0}] to {1}"), AbilityClass->GetDisplayNameText(), FText::FromString(TargetName));
	}

	return INVTEXT("Grant Gameplay Ability");
}
#endif