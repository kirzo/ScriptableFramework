// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableTasks/ScriptableTask.h"
#include "GameplayTagContainer.h"
#include "ScriptableTask_SendGameplayEvent.generated.h"

/** Sends a Gameplay Event to the target actor using the Ability System. */
UCLASS(DisplayName = "Send Gameplay Event", meta = (TaskCategory = "Gameplay|Abilities"))
class SCRIPTABLEFRAMEWORKGAS_API UScriptableTask_SendGameplayEvent : public UScriptableTask
{
	GENERATED_BODY()

public:
	/** The actor that will receive the gameplay event. */
	UPROPERTY(EditAnywhere, Category = "Event")
	TObjectPtr<AActor> TargetActor;

	/** The tag of the event to send. */
	UPROPERTY(EditAnywhere, Category = "Event")
	FGameplayTag EventTag;

	/** The magnitude to pass in the event data. */
	UPROPERTY(EditAnywhere, Category = "Event Data")
	float EventMagnitude = 0.0f;

	/** Optional Instigator to pass in the event data. */
	UPROPERTY(EditAnywhere, Category = "Event Data")
	TObjectPtr<AActor> Instigator;

	/** Optional Target to pass in the event data (If left empty, defaults to TargetActor). */
	UPROPERTY(EditAnywhere, Category = "Event Data")
	TObjectPtr<AActor> Target;

	/** Optional Object to pass in the event data (useful for Weapons, Projectiles, or DataAssets). */
	UPROPERTY(EditAnywhere, Category = "Event Data")
	TObjectPtr<UObject> OptionalObject;

	/** Second Optional Object to pass in the event data. */
	UPROPERTY(EditAnywhere, Category = "Event Data")
	TObjectPtr<UObject> OptionalObject2;

protected:
	virtual void BeginTask() override;

#if WITH_EDITOR
	virtual FText GetDisplayTitle() const override;
#endif
};