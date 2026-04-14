// Copyright 2026 kirzo

#include "Tasks/ScriptableTask_RunStateTree.h"
#include "Components/StateTreeComponent.h"
#include "StateTree.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"

void UScriptableTask_RunStateTree::BeginTask()
{
	if (IsValid(TargetActor) && StateTree.IsValid())
	{
		UStateTreeComponent* StateTreeComp = nullptr;

		// 1. Prioritize the Controller if the target is a Pawn.
		if (APawn* TargetPawn = Cast<APawn>(TargetActor))
		{
			if (AController* Controller = TargetPawn->GetController())
			{
				StateTreeComp = Controller->GetComponentByClass<UStateTreeComponent>();
			}
		}

		// 2. Fallback directly to the TargetActor.
		if (!StateTreeComp)
		{
			StateTreeComp = TargetActor->GetComponentByClass<UStateTreeComponent>();
		}

		// 3. Safely assign the tree and trigger execution.
		if (StateTreeComp)
		{
			if (StateTreeComp->IsRunning())
			{
				if (bForceRestart)
				{
					// We must stop the logic BEFORE assigning the reference, 
					// otherwise SetStateTreeReference will block it and log a warning.
					StateTreeComp->StopLogic(TEXT("ScriptableTask Override"));
					StateTreeComp->SetStateTreeReference(StateTree);
					StateTreeComp->StartLogic();
				}
				// If it's running and we shouldn't force restart, we do nothing.
			}
			else
			{
				// Tree is stopped, safe to assign and start.
				StateTreeComp->SetStateTreeReference(StateTree);
				StateTreeComp->StartLogic();
			}
		}
	}

	Finish();
}

#if WITH_EDITOR
FText UScriptableTask_RunStateTree::GetDisplayTitle() const
{
	FString TargetName;
	if (!GetBindingDisplayText(GET_MEMBER_NAME_CHECKED(UScriptableTask_RunStateTree, TargetActor), TargetName))
	{
		TargetName = TargetActor ? TargetActor->GetActorLabel() : TEXT("None");
	}

	if (StateTree.IsValid())
	{
		const FString TreeName = StateTree.GetStateTree() ? StateTree.GetStateTree()->GetName() : TEXT("Valid Tree");
		return FText::Format(INVTEXT("Run StateTree [{0}] on {1}"), FText::FromString(TreeName), FText::FromString(TargetName));
	}

	return INVTEXT("Run StateTree");
}
#endif