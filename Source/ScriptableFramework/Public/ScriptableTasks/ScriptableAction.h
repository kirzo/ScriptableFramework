// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableContainer.h"
#include "ScriptableAction.generated.h"

class UScriptableObject;
class UScriptableTask;

DECLARE_MULTICAST_DELEGATE(FScriptableActionNativeDelegate);

/** Defines how the tasks within the action container are executed. */
UENUM(BlueprintType)
enum class EScriptableActionMode : uint8
{
	/** Tasks are executed one by one in order. Fails if any task fails. */
	Sequence,

	/** All tasks start at the same time. Finishes when all finish. */
	Parallel,
};

/**
 * A container struct that holds a list of tasks, defines their execution flow,
 * and acts as the "Root" execution context (holding shared data and bindings).
 */
USTRUCT(BlueprintType)
struct SCRIPTABLEFRAMEWORK_API FScriptableAction : public FScriptableContainer
{
	GENERATED_BODY()

	friend class UScriptableTask_RunAsset;

public:
	FScriptableAction();
	~FScriptableAction();

	// -------------------------------------------------------------------
	// Configuration
	// -------------------------------------------------------------------
public:
	/** Execution mode for the tasks (e.g., Sequence vs Parallel). */
	UPROPERTY(EditAnywhere, Category = "Config")
	EScriptableActionMode Mode = EScriptableActionMode::Parallel;

	/** The list of tasks to execute. */
	UPROPERTY(EditAnywhere, Instanced, Category = "Tasks")
	TArray<TObjectPtr<UScriptableTask>> Tasks;

	FScriptableActionNativeDelegate OnActionBegin;
	FScriptableActionNativeDelegate OnActionFinish;

private:
	/** The index of the currently running task (used in Sequence mode). */
	UPROPERTY(Transient)
	int32 CurrentTaskIndex = 0;

	/** True if the action is currently running. */
	UPROPERTY(Transient)
	bool bIsRunning = false;

	// -------------------------------------------------------------------
	// API
	// -------------------------------------------------------------------
public:
	/**
	 * Creates a deep copy of this action and all its sub-tasks.
	 * @param NewOuter The object that will own the newly instanced tasks (usually the component running the action).
	 * @return A fresh, un-registered copy of the action.
	 */
	FScriptableAction Clone(UObject* NewOuter) const;

	/**
	 * Starts the execution of the action.
	 * Registers tasks under the hood and begins the flow.
	 * @param Owner The object responsible for running this action (e.g., a Component).
	 */
	void Run(UObject* Owner);

	/**
	 * Reverts the action's effects and cleans up memory.
	 * Calls Reset on all tasks and unregisters them safely.
	 */
	void Reset();

	/** Returns true if the action is currently executing. */
	bool IsRunning() const { return bIsRunning; }

private:
	/**
	 * Initializes the action and registers sub-tasks with the owner.
	 * Populates the BindingSourceMap with the children tasks.
	 */
	void Register(UObject* InOwner);

	/** Cleans up tasks, unregisters them, and clears the Binding Map. */
	void Unregister();

	/** Starts the execution of the action. */
	void Begin();

	/** Finish the execution immediately. */
	void Finish();

	void BeginSubTask(UScriptableTask* Task);
	void OnSubTaskFinished(UScriptableTask* Task);
};