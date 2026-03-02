// Copyright 2026 kirzo

#include "ScriptableTasks/AsyncRunScriptableAction.h"
#include "ScriptableTasks/ScriptableTask.h"

UAsyncRunScriptableAction* UAsyncRunScriptableAction::RunScriptableAction(UObject* Owner, FScriptableAction& Action)
{
	UAsyncRunScriptableAction* Node = NewObject<UAsyncRunScriptableAction>(Owner);

	Node->ActionOwner = Owner;
	Node->TargetAction = &Action;

	if (Owner)
	{
		Node->RegisterWithGameInstance(Owner);
	}

	return Node;
}

void UAsyncRunScriptableAction::Activate()
{
	Super::Activate();

	if (!ActionOwner || !TargetAction)
	{
		SetReadyToDestroy();
		return;
	}

	TargetAction->OnActionFinish.RemoveAll(this);
	TargetAction->OnActionFinish.AddUObject(this, &UAsyncRunScriptableAction::HandleActionFinished);

	TargetAction->Run(ActionOwner);
}

void UAsyncRunScriptableAction::HandleActionFinished()
{
	if (TargetAction)
	{
		TargetAction->OnActionFinish.RemoveAll(this);
		TargetAction->Reset();
	}

	OnFinish.Broadcast();
	SetReadyToDestroy();
}

void UAsyncRunScriptableAction::SetReadyToDestroy()
{
	TargetAction = nullptr;
	ActionOwner = nullptr;

	Super::SetReadyToDestroy();
}