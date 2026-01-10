// Copyright 2025 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableObject.h"
#include "ScriptableTask.generated.h"

SCRIPTABLEFRAMEWORK_API DECLARE_LOG_CATEGORY_EXTERN(LogScriptableTask, Log, All);

class UScriptableTask;
class UScriptableCondition;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnScriptableTaskFinishedNative, UScriptableTask* /*Task*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FScriptableTaskEventSignature, UScriptableTask*, Task);

UENUM()
enum class EScriptableTaskStatus : uint8
{
	None, Begun, Finished
};

struct SCRIPTABLEFRAMEWORK_API FScriptableTaskEvents
{
	FScriptableTaskEventSignature OnTaskBegin;
	FScriptableTaskEventSignature OnTaskFinish;
};

UCLASS(Abstract, DefaultToInstanced, EditInlineNew, Blueprintable, BlueprintType, HideCategories = (Hidden), CollapseCategories)
class SCRIPTABLEFRAMEWORK_API UScriptableTask : public UScriptableObject
{
	GENERATED_BODY()

private:
	/** Current status of the task. */
	EScriptableTaskStatus Status = EScriptableTaskStatus::None;

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

	FOnScriptableTaskFinishedNative OnTaskBeginNative;
	FOnScriptableTaskFinishedNative OnTaskFinishNative;

	UPROPERTY(BlueprintAssignable)
	FScriptableTaskEventSignature OnTaskBegin;

	UPROPERTY(BlueprintAssignable)
	FScriptableTaskEventSignature OnTaskFinish;

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

public:
	static UScriptableTask* RunTask(UObject* Owner, UScriptableTask* Task, FScriptableTaskEvents Events = FScriptableTaskEvents());
};

UCLASS(EditInlineNew, BlueprintType, NotBlueprintable, meta = (DisplayName = "Random", TaskCategory = "System", BlockSiblingBindings = "true"))
class UScriptableTask_Random final : public UScriptableTask
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = ScriptableTask, meta = (ShowOnlyInnerProperties))
	TArray<UScriptableTask*> Tasks;

	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void ResetTask() override;
	virtual void BeginTask() override;
	virtual void FinishTask() override;
	virtual void Tick(float DeltaTime) override;

private:
	UPROPERTY(Transient)
	UScriptableTask* SelectedTask = nullptr;

	void SelectTask();

	UFUNCTION()
	void OnSubTaskFinish(UScriptableTask* Task);
};

UCLASS(EditInlineNew, BlueprintType, NotBlueprintable, meta = (DisplayName = "Loop", TaskCategory = "System"))
class UScriptableTask_Loop final : public UScriptableTask
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = ScriptableTask, meta = (ClampMin = 0))
	int32 NumLoops = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = ScriptableTask, meta = (ShowOnlyInnerProperties))
	UScriptableTask* Task;

	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void ResetTask() override;
	virtual void BeginTask() override;
	virtual void FinishTask() override;
	virtual void Tick(float DeltaTime) override;

private:
	int32 LoopCount = 0;

	UFUNCTION()
	void OnSubTaskFinish(UScriptableTask* SubTask);
};

UCLASS(EditInlineNew, BlueprintType, NotBlueprintable, meta = (DisplayName = "Do Once", TaskCategory = "System"))
class UScriptableTask_DoOnce final : public UScriptableTask
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = ScriptableTask, meta = (ShowOnlyInnerProperties))
	UScriptableTask* Task;

	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginTask() override;
	virtual void FinishTask() override;
	virtual void Tick(float DeltaTime) override;

private:
	UFUNCTION()
	void OnSubTaskFinish(UScriptableTask* SubTask);
};

UCLASS(EditInlineNew, BlueprintType, NotBlueprintable, meta = (DisplayName = "Condition", TaskCategory = "System", BlockSiblingBindings = "true"))
class UScriptableTask_Condition final : public UScriptableTask
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = ScriptableTask, meta = (ShowOnlyInnerProperties))
	UScriptableCondition* Condition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = ScriptableTask, meta = (ShowOnlyInnerProperties))
	UScriptableTask* Task;

	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginTask() override;
	virtual void FinishTask() override;
	virtual void Tick(float DeltaTime) override;

private:
	UFUNCTION()
	void OnSubTaskFinish(UScriptableTask* SubTask);
};