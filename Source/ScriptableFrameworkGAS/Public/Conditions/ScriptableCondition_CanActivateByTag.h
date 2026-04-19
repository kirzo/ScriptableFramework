// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableConditions/ScriptableCondition.h"
#include "GameplayTagContainer.h"
#include "ScriptableCondition_CanActivateByTag.generated.h"

/**
 * Checks if the target actor has a Gameplay Ability with a specific Ability Tag
 * AND if that ability can currently be activated (checking tags, costs, cooldowns, etc.).
 */
UCLASS(DisplayName = "Can Activate Ability By Tag", meta = (ConditionCategory = "Gameplay|Abilities"))
class SCRIPTABLEFRAMEWORKGAS_API UScriptableCondition_CanActivateByTag : public UScriptableCondition
{
	GENERATED_BODY()

protected:
	/** The actor whose Ability System Component will be checked. */
	UPROPERTY(EditAnywhere, Category = "Condition", meta = (ScriptableContext))
	TObjectPtr<AActor> TargetActor;

	/** The native Ability Tag that categorizes the target ability. */
	UPROPERTY(EditAnywhere, Category = "Condition")
	FGameplayTag AbilityTag;

protected:
	virtual bool Evaluate_Implementation() const override;

#if WITH_EDITOR
	virtual FText GetDisplayTitle() const override;
#endif
};