// Copyright 2026 kirzo

#include "ScriptableTasks/ScriptableTask.h"
#include "ScriptableTasks/ScriptableActionAsset.h"
#include "ScriptableConditions/ScriptableCondition.h"

#include "Algo/AnyOf.h"

UE_DISABLE_OPTIMIZATION

DEFINE_LOG_CATEGORY(LogScriptableTask);

void UScriptableTask::OnUnregister()
{
	Super::OnUnregister();

	OnTaskBeginNative.Clear();
	OnTaskFinishNative.Clear();
	OnTaskBegin.Clear();
	OnTaskFinish.Clear();
}

void UScriptableTask::Reset()
{
	if (HasFinished())
	{
		Status = EScriptableTaskStatus::None;
		CurrentLoopIndex = 0;
		ResetTask();
	}
}

void UScriptableTask::Begin()
{
	check(bRegistered);

	if (Control.bDoOnce && bDoOnceFinished)
	{
		// We treat it as if it started and immediately finished successfully.
		// This ensures the Action sequence proceeds to the next task.
		OnTaskFinishNative.Broadcast(this);
		OnTaskFinish.Broadcast(this);
		return;
	}

	check(Status != EScriptableTaskStatus::Begun);

	CurrentLoopIndex = 0;

	ResolveBindings();

	Status = EScriptableTaskStatus::Begun;
	RegisterTickFunctions(true);
	BeginTask();

	OnTaskBeginNative.Broadcast(this);
	OnTaskBegin.Broadcast(this);
}

void UScriptableTask::Finish()
{
	if (HasBegun() && !HasFinished() && IsEnabled())
	{
		if (Control.bLoop)
		{
			CurrentLoopIndex++;

			// 0 means Infinite, otherwise check strictly against count
			if (Control.LoopCount <= 0 || CurrentLoopIndex < Control.LoopCount)
			{
				// Restart the task logic without changing Status or broadcasting Finish.
				// Note: We don't call Begin() to avoid resetting CurrentLoopIndex.
				// We call the virtual implementation directly.
				BeginTask();
				return; // Task is NOT finished yet.
			}
		}

		// Mark as finished for future runs.
		if (Control.bDoOnce)
		{
			bDoOnceFinished = true;
		}

		Status = EScriptableTaskStatus::Finished;
		RegisterTickFunctions(false);
		FinishTask();

		OnTaskFinishNative.Broadcast(this);
		OnTaskFinish.Broadcast(this);
	}
}

void UScriptableTask::ResetTask()
{
	ReceiveResetTask();
}

void UScriptableTask::BeginTask()
{
	ReceiveBeginTask();
}

void UScriptableTask::FinishTask()
{
	ReceiveFinishTask();
}

UScriptableTask* UScriptableTask::RunTask(UObject* Owner, UScriptableTask* Task, FScriptableTaskEvents Events)
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
	NewTask->OnTaskBegin = Events.OnTaskBegin;
	NewTask->OnTaskFinish = Events.OnTaskFinish;

	NewTask->Register(Owner);
	NewTask->Begin();

	if (NewTask->HasFinished())
	{
		NewTask->MarkAsGarbage();
		return nullptr;
	}

	return NewTask;
}

void UScriptableTask_Random::OnRegister()
{
	Super::OnRegister();

	Tasks.RemoveAll([](UScriptableTask* Task) { return !IsValid(Task) || !Task->IsEnabled(); });
	SelectTask();
}

void UScriptableTask_Random::OnUnregister()
{
	Super::OnUnregister();

	if (IsValid(SelectedTask))
	{
		SelectedTask->Unregister();
		SelectedTask = nullptr;
	}
}

void UScriptableTask_Random::ResetTask()
{
	if (IsValid(SelectedTask))
	{
		SelectedTask->Unregister();
		SelectedTask = nullptr;
	}
	
	SelectTask();
}

void UScriptableTask_Random::BeginTask()
{
	if (IsValid(SelectedTask))
	{
		SelectedTask->OnTaskFinish.AddDynamic(this, &UScriptableTask_Random::OnSubTaskFinish);
		SelectedTask->Begin();
	}
	else
	{
		Finish();
	}
}

void UScriptableTask_Random::FinishTask()
{
	if (IsValid(SelectedTask))
	{
		SelectedTask->OnTaskFinish.RemoveDynamic(this, &UScriptableTask_Random::OnSubTaskFinish);
		SelectedTask->Finish();
		SelectedTask = nullptr;
	}
}

void UScriptableTask_Random::Tick(float DeltaTime)
{
	if (SelectedTask->CanEverTick())
	{
		SelectedTask->Tick(DeltaTime);
	}
}

void UScriptableTask_Random::SelectTask()
{
	if (!Tasks.IsEmpty())
	{
		SelectedTask = Tasks[FMath::RandRange(0, Tasks.Num() - 1)];
		SelectedTask->Register(GetOwner());
		bCanEverTick = SelectedTask->CanEverTick();
	}
}

void UScriptableTask_Random::OnSubTaskFinish(UScriptableTask* Task)
{
	Task->OnTaskFinish.RemoveDynamic(this, &UScriptableTask_Random::OnSubTaskFinish);
	Finish();
}

void UScriptableTask_RunAsset::OnRegister()
{
	Super::OnRegister();

	if (!RuntimeAction.IsRunning())
	{
		InstantiateRuntimeAction();
	}
}

void UScriptableTask_RunAsset::OnUnregister()
{
	Super::OnUnregister();
	TeardownRuntimeAction();
}

void UScriptableTask_RunAsset::ResetTask()
{
	if (RuntimeAction.IsRunning())
	{
		RuntimeAction.Finish();
	}

	InstantiateRuntimeAction();
}

void UScriptableTask_RunAsset::BeginTask()
{
	if (AssetToRun && !RuntimeAction.Tasks.IsEmpty())
	{
		RuntimeAction.Begin();
	}
	else
	{
		Finish();
	}
}

void UScriptableTask_RunAsset::FinishTask()
{
	// Ensure the inner action is stopped properly
	RuntimeAction.Finish();
	RuntimeAction.Unregister();
}

void UScriptableTask_RunAsset::InstantiateRuntimeAction()
{
	TeardownRuntimeAction();

	if (AssetToRun)
	{
		// Copy the Struct
		RuntimeAction = AssetToRun->Action;

		// Deep Copy Tasks
		// The 'Tasks' array currently points to the Asset's archetype objects.
		for (int32 i = 0; i < RuntimeAction.Tasks.Num(); ++i)
		{
			UScriptableTask* TemplateTask = RuntimeAction.Tasks[i];
			if (TemplateTask)
			{
				UScriptableTask* NewTaskInstance = DuplicateObject<UScriptableTask>(TemplateTask, this);
				RuntimeAction.Tasks[i] = NewTaskInstance;
			}
		}

		RuntimeAction.Register(GetOwner());
	}
}

void UScriptableTask_RunAsset::TeardownRuntimeAction()
{
	if (RuntimeAction.IsRunning())
	{
		RuntimeAction.Finish();
	}

	RuntimeAction.Unregister();

	// Explicitly empty tasks to drop references to the instanced objects
	RuntimeAction.Tasks.Empty();
}

UE_ENABLE_OPTIMIZATION