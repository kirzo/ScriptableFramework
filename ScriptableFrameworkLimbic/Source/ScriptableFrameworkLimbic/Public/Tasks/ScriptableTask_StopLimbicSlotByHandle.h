// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "LimbicSlotLibrary.h"
#include "ScriptableTasks/ScriptableTask.h"
#include "ScriptableTask_StopLimbicSlotByHandle.generated.h"

/** Blends out the play a Set Limbic Slot started, if it still plays: whatever took its place stays. */
UCLASS(DisplayName = "Stop Limbic Slot (Handle)", meta = (TaskCategory = "Animation|Limbic"))
class SCRIPTABLEFRAMEWORKLIMBIC_API UScriptableTask_StopLimbicSlotByHandle : public UScriptableTask
{
	GENERATED_BODY()

public:
	/** Bind it to the Handle output of a Set Limbic Slot. */
	UPROPERTY(EditAnywhere, Category = "Slot")
	FLimbicSlotHandle Handle;

	UPROPERTY(EditAnywhere, Category = "Slot", meta = (ClampMin = "0", Units = "Seconds"))
	float BlendTime = 0.25f;

	virtual bool IsStoppable() const override { return false; }

protected:
	virtual void BeginTask() override;

#if WITH_EDITOR
public:
	virtual void ValidateObject(TArray<FKzValidationIssue>& OutIssues) const override;
#endif
};