// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ScriptableTasks/ScriptableTask.h"
#include "ScriptableTask_StopLimbicSlot.generated.h"

class AActor;

/** Blends out whatever plays in a Limbic slot of the target's meshes. */
UCLASS(DisplayName = "Stop Limbic Slot", meta = (TaskCategory = "Animation|Limbic"))
class SCRIPTABLEFRAMEWORKLIMBIC_API UScriptableTask_StopLimbicSlot : public UScriptableTask
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Slot", meta = (ScriptableContext))
	TObjectPtr<AActor> TargetActor;

	UPROPERTY(EditAnywhere, Category = "Slot", meta = (Categories = "Limbic.Slot"))
	FGameplayTag Slot;

	UPROPERTY(EditAnywhere, Category = "Slot", meta = (ClampMin = "0", Units = "Seconds"))
	float BlendTime = 0.25f;

	virtual bool IsStoppable() const override { return false; }

protected:
	virtual void BeginTask() override;

#if WITH_EDITOR
public:
	virtual FText GetDisplayTitle() const override;
#endif
};