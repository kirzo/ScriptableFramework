// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "ScriptableTasks/ScriptableAction.h"
#include "AsyncRunScriptableAction.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAsyncScriptableActionEvent);

UCLASS(MinimalAPI)
class UAsyncRunScriptableAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Runs a Scriptable Action.
	 * @param Action Ref to the struct.
	 */
	UFUNCTION(BlueprintCallable, Category = ScriptableAction, meta = (DefaultToSelf = "Owner", BlueprintInternalUseOnly = "true"))
	static UAsyncRunScriptableAction* RunScriptableAction(UObject* Owner, UPARAM(ref) FScriptableAction& Action);

	UPROPERTY(BlueprintAssignable)
	FAsyncScriptableActionEvent OnFinish;

	virtual void Activate() override;
	virtual void SetReadyToDestroy() override;

private:
	void HandleActionFinished();

protected:
	UPROPERTY(Transient)
	TObjectPtr<UObject> ActionOwner;

	/** Raw pointer to the external struct passed by reference. */
	FScriptableAction* TargetAction = nullptr;
};