// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableTasks/ScriptableTask.h"
#include "StateTreeReference.h"
#include "ScriptableTask_RunStateTree.generated.h"

/**
 * Task that finds a StateTree Component on the TargetActor (or its Controller),
 * assigns a new StateTree, and starts its execution.
 */
UCLASS(DisplayName = "Run StateTree", meta = (TaskCategory = "AI|StateTree"))
class SCRIPTABLEFRAMEWORKAI_API UScriptableTask_RunStateTree : public UScriptableTask
{
	GENERATED_BODY()

public:
	/** The actor whose StateTree we want to run. */
	UPROPERTY(EditAnywhere, Category = "StateTree", meta = (ScriptableContext))
	TObjectPtr<AActor> TargetActor;

	/** The StateTree asset and its parameters to assign and execute. */
	UPROPERTY(EditAnywhere, Category = "StateTree")
	FStateTreeReference StateTree;

	/** If true, the task will stop and restart the component if it is already running a tree. */
	UPROPERTY(EditAnywhere, Category = "StateTree")
	bool bForceRestart = true;

protected:
	virtual void PreResolveBindings() override;
	virtual void BeginTask() override;

#if WITH_EDITOR
	virtual FText GetDisplayTitle() const override;
#endif
};