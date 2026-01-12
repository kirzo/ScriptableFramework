// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableObjectAsset.h"
#include "ScriptableAction.h"
#include "ScriptableTask.h"
#include "ScriptableActionAsset.generated.h"

/** An asset that defines a reusable Action. */
UCLASS(BlueprintType, Const)
class SCRIPTABLEFRAMEWORK_API UScriptableActionAsset : public UScriptableObjectAsset
{
	GENERATED_BODY()

public:
	/** The action definition. Holds the Context variables and the list of Tasks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FScriptableAction Action;
};

UCLASS(EditInlineNew, BlueprintType, NotBlueprintable, meta = (DisplayName = "Run Asset", Hidden))
class UScriptableTask_RunAsset final : public UScriptableTask
{
	GENERATED_BODY()

public:
	/** The asset containing the Action definition (Context + Tasks) to run. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ScriptableTask)
	TObjectPtr<class UScriptableActionAsset> AssetToRun;

protected:
	/** * The runtime instance of the action.
	 * This holds the unique state (Context memory, Task instances) for this execution.
	 */
	UPROPERTY(Transient)
	FScriptableAction RuntimeAction;

public:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void ResetTask() override;
	virtual void BeginTask() override;
	virtual void FinishTask() override;

private:
	/** Creates a runtime copy of the Action defined in the Asset. */
	void InstantiateRuntimeAction();

	/** Cleans up the runtime action. */
	void TeardownRuntimeAction();
};