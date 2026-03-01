// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableConditions/ScriptableCondition.h"
#include "ScriptableCondition_HasAbility.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;

/** Checks if a target actor or Ability System Component has a specific Gameplay Ability granted. */
UCLASS(DisplayName = "Has Gameplay Ability", meta = (ConditionCategory = "Gameplay|Abilities"))
class SCRIPTABLEFRAMEWORKGAS_API UScriptableCondition_HasAbility : public UScriptableCondition
{
	GENERATED_BODY()

protected:
	/** Actor to check. */
	UPROPERTY(EditAnywhere, Category = "Ability")
	TObjectPtr<AActor> TargetActor;

	/** The Gameplay Ability Class to check for. */
	UPROPERTY(EditAnywhere, Category = "Ability")
	TSubclassOf<UGameplayAbility> AbilityClass;

	/**
	 * If true, the condition only passes if the ability is currently active.
	 * If false, it passes as long as the ability is granted (even if inactive or on cooldown).
	 */
	UPROPERTY(EditAnywhere, Category = "Ability")
	bool bMustBeActive = false;

protected:
	virtual bool Evaluate_Implementation() const override;

#if WITH_EDITOR
	virtual FText GetDisplayTitle() const override;
#endif
};