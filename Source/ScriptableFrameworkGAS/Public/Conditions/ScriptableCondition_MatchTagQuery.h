// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableConditions/ScriptableCondition.h"
#include "GameplayTagContainer.h"
#include "ScriptableCondition_MatchTagQuery.generated.h"

/** Checks if a target actor or Ability System Component matches a specific Gameplay Tag Query. */
UCLASS(DisplayName = "Match Tag Query", meta = (ConditionCategory = "Gameplay|Abilities"))
class SCRIPTABLEFRAMEWORKGAS_API UScriptableCondition_MatchTagQuery : public UScriptableCondition
{
	GENERATED_BODY()

protected:
	/** Actor to check. */
	UPROPERTY(EditAnywhere, Category = "Condition")
	TObjectPtr<AActor> TargetActor;

	/** Tags on the TargetActor that need to match this query for the condition to evaluate true. */
	UPROPERTY(EditAnywhere, Category = "Condition")
	FGameplayTagQuery TagQuery;

protected:
	virtual bool Evaluate_Implementation() const override;

#if WITH_EDITOR
	virtual FText GetDisplayTitle() const override;
#endif
};