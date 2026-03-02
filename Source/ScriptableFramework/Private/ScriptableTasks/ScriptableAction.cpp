// Copyright 2026 kirzo

#include "ScriptableTasks/ScriptableAction.h"
#include "ScriptableTasks/ScriptableTask.h"

FScriptableAction::FScriptableAction()
{
}

FScriptableAction::~FScriptableAction()
{
}

FScriptableAction FScriptableAction::Clone(UObject* NewOuter) const
{
	// 1. Shallow copy of the base properties (Context values, Mode, etc.)
	FScriptableAction ClonedAction = *this;

	// 2. Clear runtime state so the copy starts fresh
	ClonedAction.bIsRunning = false;
	ClonedAction.CurrentTaskIndex = 0;
	ClonedAction.OnActionBegin.Clear();
	ClonedAction.OnActionFinish.Clear();

	// 3. Deep copy the Tasks array to avoid mutating the Data Asset
	ClonedAction.Tasks.Empty(Tasks.Num());

	for (UScriptableTask* Task : Tasks)
	{
		if (Task)
		{
			// DuplicateObject creates a real memory copy with NewOuter as the owner
			UScriptableTask* ClonedTask = DuplicateObject<UScriptableTask>(Task, NewOuter);
			ClonedAction.Tasks.Add(ClonedTask);
		}
	}

	return ClonedAction;
}

void FScriptableAction::Run(UObject* InOwner)
{
	if (!InOwner) return;

	if (IsRunning())
	{
		Finish();
	}

	Register(InOwner);
	Begin();
}

void FScriptableAction::Register(UObject* InOwner)
{
	Super::Register(InOwner);

	// Filter invalid tasks
	for (int32 i = Tasks.Num() - 1; i >= 0; --i)
	{
		if (!Tasks[i])
		{
			Tasks.RemoveAt(i);
		}
	}

	for (UScriptableTask* Task : Tasks)
	{
		if (Task)
		{
			// Add to local map and inject THIS Context into the task
			AddBindingSource(Task);

			if (Task->IsEnabled())
			{
				Task->Register(Owner);
			}
		}
	}
}

void FScriptableAction::Unregister()
{
	for (UScriptableTask* Task : Tasks)
	{
		if (Task && Task->IsEnabled())
		{
			Task->Unregister();
		}
	}

	Super::Unregister();
}

void FScriptableAction::Reset()
{
	// Reset logic state
	bIsRunning = false;
	CurrentTaskIndex = 0;

	// Propagate hard reset to all tasks
	for (UScriptableTask* Task : Tasks)
	{
		if (Task)
		{
			Task->Reset();
		}
	}

	Unregister();
}

void FScriptableAction::Begin()
{
	if (Tasks.IsEmpty())
	{
		bIsRunning = false;
		return;
	}

	bIsRunning = true;
	CurrentTaskIndex = 0;

	if (Mode == EScriptableActionMode::Sequence)
	{
		BeginSubTask(Tasks[CurrentTaskIndex]);
	}
	else if (Mode == EScriptableActionMode::Parallel)
	{
		for (UScriptableTask* Task : Tasks)
		{
			BeginSubTask(Task);
		}
	}

	OnActionBegin.Broadcast();
}

void FScriptableAction::Finish()
{
	if (!bIsRunning) return;

	for (UScriptableTask* Task : Tasks)
	{
		if (Task)
		{
			Task->OnTaskFinishNative.RemoveAll(this);
			Task->Finish();
		}
	}

	bIsRunning = false;
	CurrentTaskIndex = 0;

	OnActionFinish.Broadcast();
}

void FScriptableAction::BeginSubTask(UScriptableTask* Task)
{
	if (!Task || !Task->IsEnabled())
	{
		OnSubTaskFinished(Task);
		return;
	}

	Task->OnTaskFinishNative.RemoveAll(this);
	Task->OnTaskFinishNative.AddRaw(this, &FScriptableAction::OnSubTaskFinished);
	Task->Begin();
}

void FScriptableAction::OnSubTaskFinished(UScriptableTask* Task)
{
	if (Task)
	{
		Task->OnTaskFinishNative.RemoveAll(this);
	}

	// In Parallel mode CurrentTaskIndex acts as a counter
	if (++CurrentTaskIndex >= Tasks.Num())
	{
		Finish();
	}
	else if (Mode == EScriptableActionMode::Sequence)
	{
		BeginSubTask(Tasks[CurrentTaskIndex]);
	}
}