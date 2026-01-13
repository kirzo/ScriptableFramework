// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/PropertyBag.h"
#include "Utils/PropertyBagHelpers.h"
#include "Bindings/ScriptablePropertyBindings.h"
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
struct SCRIPTABLEFRAMEWORK_API FScriptableAction
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

	/** Shared memory (Blackboard) for this action execution. */
	UPROPERTY(EditAnywhere, Category = "Config")
	FInstancedPropertyBag Context;

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

	/** Owner of this action execution. */
	UPROPERTY(Transient)
	TObjectPtr<UObject> Owner = nullptr;

	/** Runtime lookup map for Sibling bindings (Guid -> Object Instance). */
	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UScriptableObject>> BindingSourceMap;

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

	// --- Context & Binding Accessors ---

	FInstancedPropertyBag& GetContext() { return Context; }
	const FInstancedPropertyBag& GetContext() const { return Context; }

	bool HasContextProperty(const FName& Name) const
	{
		return Context.FindPropertyDescByName(Name) != nullptr;
	}

	void ResetContext()
	{
		Context.Reset();
	}

	template <typename T>
	void AddContextProperty(const FName& Name)
	{
		ScriptablePropertyBag::Add<T>(Context, Name);
	}

	template <typename T>
	void SetContextProperty(const FName& Name, const T& Value)
	{
		ScriptablePropertyBag::Set(Context, Name, Value);
	}

	template <typename T>
	T GetContextProperty(const FName& Name) const
	{
		auto Result = ScriptablePropertyBag::Get<T>(Context, Name);
		return Result.HasValue() ? Result.GetValue() : T();
	}

	/** Finds a registered object by its persistent ID (used by Property Bindings). */
	UScriptableObject* FindBindingSource(const FGuid& InID) const;

private:
	/** Populates the map and initializes the child with this context. */
	void AddBindingSource(UScriptableObject* InSource);

	void BeginSubTask(UScriptableTask* Task);
	void OnSubTaskFinished(UScriptableTask* Task);

public:
	/** Static entry point to run an action. Handles registration and startup. */
	static void RunAction(UObject* Owner, FScriptableAction& Action);
};