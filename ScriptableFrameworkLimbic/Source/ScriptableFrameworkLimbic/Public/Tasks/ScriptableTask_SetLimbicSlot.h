// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "Core/KzDatabase.h"
#include "GameplayTagContainer.h"
#include "LimbicSlotLibrary.h"
#include "ScriptableTasks/ScriptableTask.h"
#include "StructUtils/PropertyBag.h"
#include "ScriptableTask_SetLimbicSlot.generated.h"

class ULimbicPuppeteer;

/** Plays a puppeteer in a Limbic slot of the target's meshes until something replaces or stops it, such as the pose of a held object. */
UCLASS(DisplayName = "Set Limbic Slot", meta = (TaskCategory = "Animation|Limbic"))
class SCRIPTABLEFRAMEWORKLIMBIC_API UScriptableTask_SetLimbicSlot : public UScriptableTask
{
	GENERATED_BODY()

public:
	/** Actor whose skeletal meshes take the play: only those running Limbic with the slot do. */
	UPROPERTY(EditAnywhere, Category = "Slot", meta = (ScriptableContext))
	TObjectPtr<AActor> TargetActor;

	UPROPERTY(EditAnywhere, Category = "Slot", meta = (Categories = "Limbic.Slot"))
	FGameplayTag Slot;

	/** Puppeteer held in the slot, unless the target's database answers PuppeteerQuery with its own. Parameters mirror its exposed parameters. */
	UPROPERTY(EditAnywhere, Category = "Slot", meta = (NoBinding))
	TObjectPtr<ULimbicPuppeteer> Puppeteer;

	/** Asks the target's database for its puppeteer, such as RequireTags Pose.Hold.Table; the target's state picks the variant. Empty asks nothing. Parameters go by name to whichever plays. */
	UPROPERTY(EditAnywhere, Category = "Slot")
	FKzDatabaseQuery PuppeteerQuery;

	UPROPERTY(EditAnywhere, Category = "Slot", meta = (ClampMin = "0", Units = "Seconds"))
	float BlendTime = 0.25f;

	/**
	 * Keeps the task running instead of finishing, and stops the play it started, by its handle, when the task is stopped: the pose then lasts
	 * exactly as long as the action that holds it, such as one an interaction cancels when it ends. Blocks the tasks after it in a sequence.
	 */
	UPROPERTY(EditAnywhere, Category = "Slot", meta = (NoBinding))
	bool bHoldUntilStopped = false;

	/** Values for the puppeteer's exposed parameters. Only bound ones and those changed from the puppeteer's default are sent; the rest keep what the puppeteer the slot plays in gives them. */
	UPROPERTY(EditAnywhere, Category = "Slot", meta = (FixedLayout))
	FInstancedPropertyBag Parameters;

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
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;

private:
	/** Rebuilds Parameters to mirror the puppeteer's exposed parameters: new ones take its default, the rest keep their value. */
	void SyncParameters();
#endif
};