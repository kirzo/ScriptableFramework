// Copyright 2026 kirzo

#include "ScriptableTasks/ScriptableAction.h"
#include "ScriptableTasks/ScriptableTask.h"

UE_DISABLE_OPTIMIZATION

FScriptableAction::FScriptableAction()
{
}

FScriptableAction::~FScriptableAction()
{
}

void FScriptableAction::Register(UObject* InOwner)
{
	Owner = InOwner;
	BindingSourceMap.Reset(); // Clean slate

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

	BindingSourceMap.Empty();
	Owner = nullptr;
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

UScriptableObject* FScriptableAction::FindBindingSource(const FGuid& InID) const
{
	if (const TObjectPtr<UScriptableObject>* Found = BindingSourceMap.Find(InID))
	{
		return Found->Get();
	}
	return nullptr;
}

void FScriptableAction::AddBindingSource(UScriptableObject* InSource)
{
	if (InSource)
	{
		InSource->InitRuntimeData(&Context, &BindingSourceMap);

		FGuid ID = InSource->GetBindingID();
		if (ID.IsValid())
		{
			BindingSourceMap.Add(ID, InSource);
		}
	}
}

void FScriptableAction::RunAction(UObject* Owner, FScriptableAction& Action)
{
	if (!Owner) return;

	if (Action.IsRunning())
	{
		Action.Finish();
	}

	Action.Register(Owner);
	Action.Begin();
}

UE_ENABLE_OPTIMIZATION