// Copyright Kirzo. All Rights Reserved.

#include "ScriptableTasks/AsyncRunScriptableTask.h"

UAsyncRunScriptableTask* UAsyncRunScriptableTask::RunScriptableTask(UObject* Owner, UScriptableTask* Task)
{
	if (!Task)
	{
		UE_LOG(LogScriptableTask, Warning, TEXT("RunTask: Invalid Task."));
		return nullptr;
	}

	if (!Task->IsEnabled())
	{
		UE_LOG(LogScriptableTask, Warning, TEXT("RunTask: Disabled Task '%s'."), *GetNameSafe(Task));
		return nullptr;
	}

	UScriptableTask* NewTask = DuplicateObject<UScriptableTask>(Task, Owner);

	UAsyncRunScriptableTask* AsyncTask = NewObject<UAsyncRunScriptableTask>(NewTask);
	AsyncTask->SetScriptableTask(NewTask);
	return AsyncTask;
}

void UAsyncRunScriptableTask::SetScriptableTask(UScriptableTask* Task)
{
	ScriptableTask = Task;
}

void UAsyncRunScriptableTask::Activate()
{
	Super::Activate();

	ScriptableTask->Register(ScriptableTask->GetOuter());
	ScriptableTask->OnTaskFinish.AddDynamic(this, &UAsyncRunScriptableTask::OnTaskFinish);
	ScriptableTask->Begin();
}

void UAsyncRunScriptableTask::OnTaskFinish(UScriptableTask* Task)
{
	OnFinish.Broadcast(ScriptableTask.Get());
	ScriptableTask->MarkAsGarbage();
	ScriptableTask.Reset();
	SetReadyToDestroy();
}