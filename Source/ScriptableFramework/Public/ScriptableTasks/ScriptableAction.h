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
	 * Initializes the action and registers sub-tasks with the owner.
	 * Populates the BindingSourceMap with the children tasks.
	 */
	void Register(UObject* InOwner);

	/** Cleans up tasks, unregisters them, and clears the Binding Map. */
	void Unregister();

	void Reset();

	/** Starts the execution of the action. */
	void Begin();

	/** Finish the execution immediately. */
	void Finish();

	/** Returns true if the action is currently executing. */
	bool IsRunning() const { return bIsRunning; }

private:
	void BeginSubTask(UScriptableTask* Task);
	void OnSubTaskFinished(UScriptableTask* Task);

public:
	/** Static entry point to run an action. Handles registration and startup. */
	static void RunAction(UObject* Owner, FScriptableAction& Action);
};