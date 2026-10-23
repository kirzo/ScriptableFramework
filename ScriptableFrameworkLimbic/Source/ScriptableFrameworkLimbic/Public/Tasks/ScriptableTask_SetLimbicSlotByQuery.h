// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "Core/KzDatabase.h"
#include "GameplayTagContainer.h"
#include "LimbicSlotLibrary.h"
#include "ScriptableTasks/ScriptableTask.h"
#include "StructUtils/PropertyBag.h"
#include "ScriptableTask_SetLimbicSlotByQuery.generated.h"

class ULimbicPuppeteer;

/**
 * Plays in a Limbic slot of the target's meshes the puppeteer a query picks: the source's database answers, such as a table's own hold pose, and the
 * target's overrides it only with a more specific row. Both's tags are the context. The tables hold soft Limbic Puppeteer references.
 */
UCLASS(DisplayName = "Set Limbic Slot (By Query)", meta = (TaskCategory = "Animation|Limbic"))
class SCRIPTABLEFRAMEWORKLIMBIC_API UScriptableTask_SetLimbicSlotByQuery : public UScriptableTask
{
	GENERATED_BODY()

public:
	/** Actor whose skeletal meshes take the play: only those running Limbic with the slot do. */
	UPROPERTY(EditAnywhere, Category = "Slot", meta = (ScriptableContext))
	TObjectPtr<AActor> TargetActor;

	/** The object the play is about, whose database answers first, such as the table being carried. None leaves the target's alone. */
	UPROPERTY(EditAnywhere, Category = "Slot")
	TObjectPtr<AActor> SourceActor;

	UPROPERTY(EditAnywhere, Category = "Slot", meta = (Categories = "Limbic.Slot"))
	FGameplayTag Slot;

	/** What to ask, such as RequireTags Pose.Hold. */
	UPROPERTY(EditAnywhere, Category = "Slot")
	FKzDatabaseQuery Query;

	UPROPERTY(EditAnywhere, Category = "Slot", meta = (ClampMin = "0", Units = "Seconds"))
	float BlendTime = 0.25f;

	/**
	 * Keeps the task running instead of finishing, and stops the play it started, by its handle, when the task is stopped: the pose then lasts
	 * exactly as long as the action that holds it, such as one an interaction cancels when it ends. Blocks the tasks after it in a sequence.
	 */
	UPROPERTY(EditAnywhere, Category = "Slot", meta = (NoBinding))
	bool bHoldUntilStopped = false;

	/** Values sent by name to whichever puppeteer plays; it takes those it has. */
	UPROPERTY(EditAnywhere, Category = "Slot")
	FInstancedPropertyBag Parameters;

#if WITH_EDITORONLY_DATA
	/** Pick a puppeteer to add its exposed parameters that Parameters lacks, at their defaults; then it clears itself. */
	UPROPERTY(EditAnywhere, Transient, Category = "Slot", meta = (NoBinding))
	TObjectPtr<ULimbicPuppeteer> AddParametersFrom;
#endif

	/** The play this task started, for Stop Limbic Slot to stop it alone. */
	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category = "Output")
	FLimbicSlotHandle Handle;

	virtual bool IsStoppable() const override { return bHoldUntilStopped; }

protected:
	virtual void BeginTask() override;
	virtual void StopTask() override;

#if WITH_EDITOR
public:
	virtual FText GetDisplayTitle() const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};