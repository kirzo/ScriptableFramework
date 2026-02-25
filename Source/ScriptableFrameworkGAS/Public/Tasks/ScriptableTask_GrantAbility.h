// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableTasks/ScriptableTask.h"
#include "GameplayAbilitySpecHandle.h"
#include "ScriptableTask_GrantAbility.generated.h"

class UGameplayAbility;

/** Grants a Gameplay Ability to the target actor. */
UCLASS(DisplayName = "Grant Ability", meta = (TaskCategory = "Gameplay|Abilities"))
class SCRIPTABLEFRAMEWORKGAS_API UScriptableTask_GrantAbility : public UScriptableTask
{
	GENERATED_BODY()

public:
	/** The actor that will receive the ability. */
	UPROPERTY(EditAnywhere, Category = "Ability")
	TObjectPtr<AActor> TargetActor;

	/** The Gameplay Ability class to grant. */
	UPROPERTY(EditAnywhere, Category = "Ability")
	TSubclassOf<UGameplayAbility> AbilityClass;

	/** If true, calling Reset() on this task will remove the ability that was granted. */
	UPROPERTY(EditAnywhere, Category = "Ability")
	bool bRevertOnReset = false;

protected:
	virtual void BeginTask() override;
	virtual void ResetTask() override;

private:
	/** Stored handle to keep track of the granted ability so it can be revoked later. */
	UPROPERTY(Transient)
	FGameplayAbilitySpecHandle GrantedHandle;

#if WITH_EDITOR
	virtual FText GetDisplayTitle() const override;
#endif
};