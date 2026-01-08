// Copyright 2026 kirzo

#include "ScriptableTasks/ScriptableAction.h"
#include "ScriptableTasks/ScriptableTask.h"

FScriptableAction::FScriptableAction()
{
}

FScriptableAction::~FScriptableAction()
{
}

void FScriptableAction::Register(UObject* InOwner)
{
	Owner = InOwner;
	BindingSourceMap.Reset(); // Ensure clean slate

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
			// Register the task into our local Binding Map.
			AddBindingSource(Task);

			// Initialize the task with the owner context
			Task->Register(Owner);
		}
	}
}

void FScriptableAction::Unregister()
{
	Stop();

	for (UScriptableTask* Task : Tasks)
	{
		if (Task)
		{
			Task->Unregister();
		}
	}

	BindingSourceMap.Empty();
	Owner = nullptr;
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
		ExecuteNextSequenceTask();
	}
	else if (Mode == EScriptableActionMode::Parallel)
	{
		for (UScriptableTask* Task : Tasks)
		{
			if (Task)
			{
				Task->Begin();
			}
		}
	}
}

void FScriptableAction::Stop()
{
	if (!bIsRunning) return;

	for (UScriptableTask* Task : Tasks)
	{
		if (Task && Task->HasBegun() && !Task->HasFinished())
		{
			Task->Finish();
		}
	}

	bIsRunning = false;
	CurrentTaskIndex = 0;
}

void FScriptableAction::Tick(float DeltaTime)
{
	if (!bIsRunning) return;

	bool bAllFinished = true;

	// Polling logic for execution
	for (UScriptableTask* Task : Tasks)
	{
		if (Task)
		{
			const bool bShouldTick = (Mode == EScriptableActionMode::Parallel) || (Mode == EScriptableActionMode::Sequence && Tasks.IndexOfByKey(Task) == CurrentTaskIndex);

			if (bShouldTick)
			{
				if (Task->HasBegun() && !Task->HasFinished())
				{
					if (Task->CanEverTick())
					{
						Task->Tick(DeltaTime);
					}

					if (Task->HasFinished())
					{
						OnSubTaskFinished(Task);
					}
					else
					{
						bAllFinished = false;
					}
				}
				else if (Mode == EScriptableActionMode::Parallel && !Task->HasBegun())
				{
					bAllFinished = false;
				}
			}
		}
	}

	if (bAllFinished && Mode == EScriptableActionMode::Parallel)
	{
		bIsRunning = false;
	}
}

void FScriptableAction::ExecuteNextSequenceTask()
{
	if (Tasks.IsValidIndex(CurrentTaskIndex))
	{
		UScriptableTask* Task = Tasks[CurrentTaskIndex];
		if (Task)
		{
			Task->Reset();
			Task->Begin();
		}
		else
		{
			OnSubTaskFinished(nullptr);
		}
	}
	else
	{
		bIsRunning = false;
	}
}

void FScriptableAction::OnSubTaskFinished(UScriptableTask* Task)
{
	if (Mode == EScriptableActionMode::Sequence)
	{
		CurrentTaskIndex++;
		ExecuteNextSequenceTask();
	}
	else if (Mode == EScriptableActionMode::Parallel)
	{
		bool bAnyRunning = false;
		for (UScriptableTask* SubTask : Tasks)
		{
			if (SubTask && !SubTask->HasFinished())
			{
				bAnyRunning = true;
				break;
			}
		}

		if (!bAnyRunning)
		{
			bIsRunning = false;
		}
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
		InSource->InitRuntimeData(&Context);

		FGuid ID = InSource->GetBindingID();
		if (ID.IsValid())
		{
			BindingSourceMap.Add(ID, InSource);
		}
	}
}