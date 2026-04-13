// Copyright 2026 kirzo

#include "Tasks/ScriptableTask_ManageGameplayTag.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"

void UScriptableTask_ManageGameplayTag::BeginTask()
{
	if (IsValid(TargetActor) && !Tags.IsEmpty())
	{
		ApplyTagOperation(TargetActor, Tags, Operation);
	}

	Finish();
}

void UScriptableTask_ManageGameplayTag::ResetTask()
{
	if (bRevertOnReset && IsValid(TargetActor) && !Tags.IsEmpty())
	{
		// Invert the operation for the rollback.
		EScriptableTagOperation InverseOp = (Operation == EScriptableTagOperation::AddTags)
			? EScriptableTagOperation::RemoveTags
			: EScriptableTagOperation::AddTags;

		ApplyTagOperation(TargetActor, Tags, InverseOp);
	}
}

void UScriptableTask_ManageGameplayTag::ApplyTagOperation(AActor* InTarget, const FGameplayTagContainer& InTags, EScriptableTagOperation InOperation)
{
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InTarget);
	if (!ASC) return;

	if (InOperation == EScriptableTagOperation::AddTags)
	{
		ASC->AddLooseGameplayTags(InTags);
	}
	else
	{
		ASC->RemoveLooseGameplayTags(InTags);
	}
}

#if WITH_EDITOR
FText UScriptableTask_ManageGameplayTag::GetDisplayTitle() const
{
	FString TargetName;
	if (!GetBindingDisplayText(GET_MEMBER_NAME_CHECKED(UScriptableTask_ManageGameplayTag, TargetActor), TargetName))
	{
		TargetName = TargetActor ? TargetActor->GetActorLabel() : TEXT("None");
	}

	FString OpStr = (Operation == EScriptableTagOperation::AddTags) ? TEXT("Add") : TEXT("Remove");

	if (!Tags.IsEmpty())
	{
		// Display the first tag and a counter if there are multiple, to keep the UI node clean.
		FString TagString = Tags.First().ToString();
		if (Tags.Num() > 1)
		{
			TagString += FString::Printf(TEXT(" (+%d)"), Tags.Num() - 1);
		}

		return FText::Format(INVTEXT("{0} Tags [{1}] on {2}"), FText::FromString(OpStr), FText::FromString(TagString), FText::FromString(TargetName));
	}

	return FText::Format(INVTEXT("{0} Gameplay Tags"), FText::FromString(OpStr));
}
#endif