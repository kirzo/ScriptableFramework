// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableTasks/ScriptableTask.h"
#include "GameplayTagContainer.h"
#include "ScriptableTask_ActivateAbilityByTag.generated.h"

/** Attempts to activate a Gameplay Ability on the target actor using its native Ability Tag. */
UCLASS(DisplayName = "Activate Ability By Tag", meta = (TaskCategory = "Gameplay|Abilities"))
class SCRIPTABLEFRAMEWORKGAS_API UScriptableTask_ActivateAbilityByTag : public UScriptableTask
{
	GENERATED_BODY()

public:
	/** The actor whose Ability System Component will activate the ability. */
	UPROPERTY(EditAnywhere, Category = "Ability")
	TObjectPtr<AActor> TargetActor;

	/** The Ability Tag of the Gameplay Ability to activate. */
	UPROPERTY(EditAnywhere, Category = "Ability")
	FGameplayTag AbilityTag;

protected:
	virtual void BeginTask() override;

#if WITH_EDITOR
	virtual FText GetDisplayTitle() const override;
#endif
};