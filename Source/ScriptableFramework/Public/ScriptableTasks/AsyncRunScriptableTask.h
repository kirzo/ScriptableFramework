// Copyright Kirzo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "ScriptableTask.h"
#include "AsyncRunScriptableTask.generated.h"

UCLASS(MinimalAPI)
class UAsyncRunScriptableTask : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

private:
	TWeakObjectPtr<UScriptableTask> ScriptableTask;

public:
	UFUNCTION(BlueprintCallable, Category = ScriptableTask, meta = (DefaultToSelf = "Owner", BlueprintInternalUseOnly = "true"))
	static SCRIPTABLEFRAMEWORK_API UAsyncRunScriptableTask* RunScriptableTask(UObject* Owner, UScriptableTask* Task);

	UPROPERTY(BlueprintAssignable)
	FScriptableTaskEventSignature OnFinish;

	void SetScriptableTask(UScriptableTask* Task);

	virtual void Activate() override;

	UFUNCTION()
	void OnTaskFinish(UScriptableTask* Task);
};