// Copyright 2026 kirzo

#include "ScriptableTasks/ScriptableTask.h"
#include "ScriptableTasks/ScriptableTaskAsset.h"
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
		ResetTask();
	}
}

void UScriptableTask::Begin()
{
	check(bRegistered);
	check(Status != EScriptableTaskStatus::Begun);

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

void UScriptableTask_Sequence::OnRegister()
{
	Super::OnRegister();

	Tasks.RemoveAll([](UScriptableTask* Task) { return !IsValid(Task) || !Task->IsEnabled(); });
	for (UScriptableTask* Task : Tasks)
	{
		Task->Register(GetOwner());
	}

	bCanEverTick = Algo::AnyOf(Tasks, [](UScriptableTask* Task) { return Task->CanEverTick(); });
}

void UScriptableTask_Sequence::OnUnregister()
{
	Super::OnUnregister();

	for (UScriptableTask* Task : Tasks)
	{
		Task->Unregister();
	}
}

void UScriptableTask_Sequence::ResetTask()
{
	for (UScriptableTask* Task : Tasks)
	{
		Task->OnTaskFinish.RemoveDynamic(this, &UScriptableTask_Sequence::OnSubTaskFinish);
		Task->Reset();
	}

	TaskIndex = 0;
}

void UScriptableTask_Sequence::BeginTask()
{
	if (Tasks.IsEmpty())
	{
		Finish();
		return;
	}
	else
	{
		BeginSubTask(Tasks[TaskIndex]);
	}
}

void UScriptableTask_Sequence::FinishTask()
{
	for (UScriptableTask* Task : Tasks)
	{
		Task->OnTaskFinish.RemoveDynamic(this, &UScriptableTask_Sequence::OnSubTaskFinish);
		Task->Finish();
	}
}

void UScriptableTask_Sequence::Tick(float DeltaTime)
{
	UScriptableTask* Task = Tasks[TaskIndex];
	if (Task->CanEverTick())
	{
		Task->Tick(DeltaTime);
	}
}

void UScriptableTask_Sequence::BeginSubTask(UScriptableTask* Task)
{
	Task->OnTaskFinish.AddDynamic(this, &UScriptableTask_Sequence::OnSubTaskFinish);
	Task->Begin();
}

void UScriptableTask_Sequence::OnSubTaskFinish(UScriptableTask* Task)
{
	Task->OnTaskFinish.RemoveDynamic(this, &UScriptableTask_Sequence::OnSubTaskFinish);

	TaskIndex++;
	if (TaskIndex >= Tasks.Num())
	{
		Finish();
	}
	else
	{
		BeginSubTask(Tasks[TaskIndex]);
	}
}

void UScriptableTask_ParallelSequence::OnRegister()
{
	Super::OnRegister();

	Tasks.RemoveAll([](UScriptableTask* Task) { return !IsValid(Task) || !Task->IsEnabled(); });
	for (UScriptableTask* Task : Tasks)
	{
		Task->Register(GetOwner());
	}

	bCanEverTick = Algo::AnyOf(Tasks, [](UScriptableTask* Task) { return Task->CanEverTick(); });
}

void UScriptableTask_ParallelSequence::OnUnregister()
{
	Super::OnUnregister();

	for (UScriptableTask* Task : Tasks)
	{
		Task->Unregister();
	}
}

void UScriptableTask_ParallelSequence::ResetTask()
{
	for (UScriptableTask* Task : Tasks)
	{
		Task->OnTaskFinish.RemoveDynamic(this, &UScriptableTask_ParallelSequence::OnSubTaskFinish);
		Task->Reset();
	}

	TaskIndex = 0;
}

void UScriptableTask_ParallelSequence::BeginTask()
{
	if (Tasks.IsEmpty())
	{
		Finish();
		return;
	}

	for (UScriptableTask* Task : Tasks)
	{
		BeginSubTask(Task);
	}
}

void UScriptableTask_ParallelSequence::FinishTask()
{
	for (UScriptableTask* Task : Tasks)
	{
		Task->OnTaskFinish.RemoveDynamic(this, &UScriptableTask_ParallelSequence::OnSubTaskFinish);
		Task->Finish();
	}
}

void UScriptableTask_ParallelSequence::Tick(float DeltaTime)
{
	for (UScriptableTask* Task : Tasks)
	{
		if (!Task->HasFinished() && Task->CanEverTick())
		{
			Task->Tick(DeltaTime);
		}
	}
}

void UScriptableTask_ParallelSequence::BeginSubTask(UScriptableTask* Task)
{
	Task->OnTaskFinish.AddDynamic(this, &UScriptableTask_ParallelSequence::OnSubTaskFinish);
	Task->Begin();
}

void UScriptableTask_ParallelSequence::OnSubTaskFinish(UScriptableTask* Task)
{
	Task->OnTaskFinish.RemoveDynamic(this, &UScriptableTask_ParallelSequence::OnSubTaskFinish);

	TaskIndex++;
	if (TaskIndex >= Tasks.Num())
	{
		Finish();
	}
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

void UScriptableTask_Loop::OnRegister()
{
	Super::OnRegister();

	if (IsValid(Task) && Task->IsEnabled())
	{
		Task->Register(GetOwner());
		bCanEverTick = Task->CanEverTick();
	}
}

void UScriptableTask_Loop::OnUnregister()
{
	Super::OnUnregister();

	if (IsValid(Task))
	{
		Task->Unregister();
		Task = nullptr;
	}
}

void UScriptableTask_Loop::ResetTask()
{
	LoopCount = 0;

	if (IsValid(Task))
	{
		Task->OnTaskFinish.RemoveDynamic(this, &UScriptableTask_Loop::OnSubTaskFinish);
		Task->Reset();
	}
}

void UScriptableTask_Loop::BeginTask()
{
	if (IsValid(Task) && Task->IsEnabled())
	{
		Task->OnTaskFinish.AddDynamic(this, &UScriptableTask_Loop::OnSubTaskFinish);
		Task->Begin();
	}
	else
	{
		Finish();
	}
}

void UScriptableTask_Loop::FinishTask()
{
	if (IsValid(Task))
	{
		Task->OnTaskFinish.RemoveDynamic(this, &UScriptableTask_Loop::OnSubTaskFinish);
		Task->Finish();
	}
}

void UScriptableTask_Loop::Tick(float DeltaTime)
{
	if (Task->CanEverTick())
	{
		Task->Tick(DeltaTime);
	}
}

void UScriptableTask_Loop::OnSubTaskFinish(UScriptableTask* SubTask)
{
	LoopCount++;

	if (NumLoops <= 0 || LoopCount < NumLoops)
	{
		SubTask->Reset();
		SubTask->Begin();
	}
	else
	{
		SubTask->OnTaskFinish.RemoveDynamic(this, &UScriptableTask_Loop::OnSubTaskFinish);
		Finish();
	}
}

void UScriptableTask_DoOnce::OnRegister()
{
	Super::OnRegister();

	if (IsValid(Task) && Task->IsEnabled())
	{
		Task->Register(GetOwner());
		bCanEverTick = Task->CanEverTick();
	}
}

void UScriptableTask_DoOnce::OnUnregister()
{
	Super::OnUnregister();

	if (IsValid(Task))
	{
		Task->Unregister();
		Task = nullptr;
	}
}

void UScriptableTask_DoOnce::BeginTask()
{
	if (IsValid(Task) && Task->IsEnabled())
	{
		Task->OnTaskFinish.AddDynamic(this, &UScriptableTask_DoOnce::OnSubTaskFinish);
		Task->Begin();
	}
	else
	{
		Finish();
	}
}

void UScriptableTask_DoOnce::FinishTask()
{
	if (IsValid(Task))
	{
		Task->OnTaskFinish.RemoveDynamic(this, &UScriptableTask_DoOnce::OnSubTaskFinish);
		Task->Finish();
		Task->Unregister();
		Task = nullptr;

		bCanEverTick = false;
	}
}

void UScriptableTask_DoOnce::Tick(float DeltaTime)
{
	if (Task->CanEverTick())
	{
		Task->Tick(DeltaTime);
	}
}

void UScriptableTask_DoOnce::OnSubTaskFinish(UScriptableTask* SubTask)
{
	SubTask->OnTaskFinish.RemoveDynamic(this, &UScriptableTask_DoOnce::OnSubTaskFinish);
	Finish();
}

void UScriptableTask_Condition::OnRegister()
{
	Super::OnRegister();

	if (IsValid(Task) && Task->IsEnabled())
	{
		Task->Register(GetOwner());
		bCanEverTick = Task->CanEverTick();
	}
}

void UScriptableTask_Condition::OnUnregister()
{
	Super::OnUnregister();

	if (IsValid(Task))
	{
		Task->Unregister();
		Task = nullptr;
	}
}

void UScriptableTask_Condition::BeginTask()
{
	if (IsValid(Task) && Task->IsEnabled() && UScriptableCondition::EvaluateCondition(GetOwner(), Condition))
	{
		Task->OnTaskFinish.AddDynamic(this, &UScriptableTask_Condition::OnSubTaskFinish);
		Task->Begin();
	}
	else
	{
		Finish();
	}
}

void UScriptableTask_Condition::FinishTask()
{
	if (IsValid(Task))
	{
		Task->OnTaskFinish.RemoveDynamic(this, &UScriptableTask_Condition::OnSubTaskFinish);
		Task->Finish();
		Task->Unregister();
		Task = nullptr;

		bCanEverTick = false;
	}
}

void UScriptableTask_Condition::Tick(float DeltaTime)
{
	if (Task->CanEverTick())
	{
		Task->Tick(DeltaTime);
	}
}

void UScriptableTask_Condition::OnSubTaskFinish(UScriptableTask* SubTask)
{
	SubTask->OnTaskFinish.RemoveDynamic(this, &UScriptableTask_Condition::OnSubTaskFinish);
	Finish();
}

void UScriptableTask_RunAsset::OnRegister()
{
	Super::OnRegister();

	if (!IsValid(Task))
	{
		CreateTaskInstance();
	}
}

void UScriptableTask_RunAsset::OnUnregister()
{
	Super::OnUnregister();

	ClearTaskInstance();
}

void UScriptableTask_RunAsset::ResetTask()
{
	if (IsValid(Task))
	{
		Task->OnTaskFinish.RemoveDynamic(this, &UScriptableTask_RunAsset::OnSubTaskFinish);
		Task->Reset();
	}
}

void UScriptableTask_RunAsset::BeginTask()
{
	if (IsValid(Task))
	{
		Task->OnTaskFinish.AddDynamic(this, &UScriptableTask_RunAsset::OnSubTaskFinish);
		Task->Begin();
	}
	else
	{
		Finish();
	}
}

void UScriptableTask_RunAsset::FinishTask()
{
	if (IsValid(Task))
	{
		Task->OnTaskFinish.RemoveDynamic(this, &UScriptableTask_RunAsset::OnSubTaskFinish);
		Task->Finish();
		Task->Unregister();
		Task = nullptr;

		bCanEverTick = false;
	}
}

void UScriptableTask_RunAsset::Tick(float DeltaTime)
{
	if (Task->CanEverTick())
	{
		Task->Tick(DeltaTime);
	}
}

void UScriptableTask_RunAsset::OnSubTaskFinish(UScriptableTask* SubTask)
{
	SubTask->OnTaskFinish.RemoveDynamic(this, &UScriptableTask_RunAsset::OnSubTaskFinish);
	Finish();
}

void UScriptableTask_RunAsset::CreateTaskInstance()
{
	ClearTaskInstance();

	if (IsValid(AssetToRun) && IsValid(AssetToRun->Task))
	{
		// Duplicate the template from the asset to create a unique instance for this execution context.
		// We use 'this' as the outer to keep the hierarchy clean.
		Task = DuplicateObject<UScriptableTask>(AssetToRun->Task, this);

		if (IsValid(Task))
		{
			Task->Register(GetOwner());
			bCanEverTick = Task->CanEverTick();
		}
	}
}

void UScriptableTask_RunAsset::ClearTaskInstance()
{
	if (IsValid(Task))
	{
		Task->Unregister();
		Task = nullptr;
	}
	bCanEverTick = false;
}

UE_ENABLE_OPTIMIZATION