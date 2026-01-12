// Copyright 2025 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableObject.h"
#include "ScriptableTask.generated.h"

SCRIPTABLEFRAMEWORK_API DECLARE_LOG_CATEGORY_EXTERN(LogScriptableTask, Log, All);

class UScriptableTask;
class UScriptableCondition;

DECLARE_MULTICAST_DELEGATE_OneParam(FScriptableTaskNativeDelegate, UScriptableTask* /*Task*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FScriptableTaskDelegate, UScriptableTask*, Task);

UENUM()
enum class EScriptableTaskStatus : uint8
{
	None, Begun, Finished
};

struct SCRIPTABLEFRAMEWORK_API FScriptableTaskEvents
{
	FScriptableTaskDelegate OnTaskBegin;
	FScriptableTaskDelegate OnTaskFinish;
};

/** Flow control settings for the task. */
USTRUCT(BlueprintType)
struct FScriptableTaskControl
{
	GENERATED_BODY()

	/** If true, the task will loop automatically upon finishing. */
	UPROPERTY(EditAnywhere, Category = "Control")
	uint8 bLoop : 1 = false;

	/** Number of loops. 0 means infinite. */
	UPROPERTY(EditAnywhere, Category = "Control", meta = (ClampMin = 0))
	int32 LoopCount = 0;

	/** If true, this task will execute only once during its lifecycle. */
	UPROPERTY(EditAnywhere, Category = "Control")
	uint8 bDoOnce : 1 = false;
};

UCLASS(Abstract, DefaultToInstanced, EditInlineNew, Blueprintable, BlueprintType, HideCategories = (Hidden), CollapseCategories)
class SCRIPTABLEFRAMEWORK_API UScriptableTask : public UScriptableObject
{
	GENERATED_BODY()

private:
	/** Current status of the task. */
	EScriptableTaskStatus Status = EScriptableTaskStatus::None;

	/** Advanced execution logic (Looping, DoOnce). */
	UPROPERTY(EditAnywhere, Category = Hidden, meta = (NoBinding))
	FScriptableTaskControl Control;

	/** Counter for the current loop iteration. */
	UPROPERTY(Transient)
	int32 CurrentLoopIndex = 0;

	/** Flag to track if a DoOnce task has already executed. */
	UPROPERTY(Transient)
	uint8 bDoOnceFinished : 1 = false;

public:
	EScriptableTaskStatus GetStatus() const { return Status; }

	/** Indicates that BeginTask has been called, but FinishTask has not yet */
	bool HasBegun() const { return Status == EScriptableTaskStatus::Begun; }
	/** Indicates that FinishTask has been called */
	bool HasFinished() const { return Status == EScriptableTaskStatus::Finished; }

	virtual bool IsReadyToTick() const override { return HasBegun(); }

	virtual void OnUnregister() override;

	UFUNCTION(BlueprintCallable, Category = ScriptableTask)
	void Reset();

	/**
	* Begins the execution of this task.
	* Requires task to be registered.
	*/
	UFUNCTION(BlueprintCallable, Category = ScriptableTask)
	void Begin();

	/**
	* Finish the execution of this task.
	*/
	UFUNCTION(BlueprintCallable, Category = ScriptableTask)
	void Finish();

	FScriptableTaskNativeDelegate OnTaskBeginNative;
	FScriptableTaskNativeDelegate OnTaskFinishNative;

	UPROPERTY(BlueprintAssignable)
	FScriptableTaskDelegate OnTaskBegin;

	UPROPERTY(BlueprintAssignable)
	FScriptableTaskDelegate OnTaskFinish;

private:
	virtual void ResetTask();
	virtual void BeginTask();
	virtual void FinishTask();

protected:
	/**
	* Blueprint implementable event for when the task resets.
	*/
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "Reset Task"))
	void ReceiveResetTask();

	/**
	* Blueprint implementable event for when the task begins.
	*/
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "Begin Task"))
	void ReceiveBeginTask();

	/**
	* Blueprint implementable event for when the task ends.
	*/
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "Finish Task"))
	void ReceiveFinishTask();
};