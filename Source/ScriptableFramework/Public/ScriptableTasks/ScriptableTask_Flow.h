// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableTasks/ScriptableTask.h"
#include "ScriptableTask_Flow.generated.h"

/**
 * Latent task that waits for a specified amount of time before finishing.
 * Note: This task is primarily intended for 'Sequence' execution mode.
 * In 'Parallel' mode, all tasks start simultaneously, so this Wait will NOT block
 * other sibling tasks from executing (it will only delay the completion of the Action itself).
 */
UCLASS(DisplayName = "Wait", meta = (TaskCategory = "System|Flow"))
class SCRIPTABLEFRAMEWORK_API UScriptableTask_Wait : public UScriptableTask
{
	GENERATED_BODY()

public:
	/** The duration to wait in seconds. */
	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = 0))
	float Duration = 1.0f;

	/** Random deviation added to the duration (Duration +/- RandomDeviation). */
	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = 0))
	float RandomDeviation = 0.0f;

protected:
	virtual void BeginTask() override;

	UFUNCTION()
	void OnWaitFinished();

public:
#if WITH_EDITOR
	virtual FText GetDisplayTitle() const override;
#endif

private:
	FTimerHandle TimerHandle;
};