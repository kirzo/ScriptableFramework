// Copyright 2026 kirzo

#include "Tasks/ScriptableTask_ManageGameplayTag.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"

void UScriptableTask_ManageGameplayTag::BeginTask()
{
	if (IsValid(TargetActor) && Tag.IsValid())
	{
		ApplyTagOperation(TargetActor, Tag, Operation);
	}

	Finish();
}

void UScriptableTask_ManageGameplayTag::ResetTask()
{
	if (bRevertOnReset && IsValid(TargetActor) && Tag.IsValid())
	{
		// Invert the operation for the rollback
		EScriptableTagOperation InverseOp = (Operation == EScriptableTagOperation::AddTag)
			? EScriptableTagOperation::RemoveTag
			: EScriptableTagOperation::AddTag;

		ApplyTagOperation(TargetActor, Tag, InverseOp);
	}
}

void UScriptableTask_ManageGameplayTag::ApplyTagOperation(AActor* InTarget, const FGameplayTag& InTag, EScriptableTagOperation InOperation)
{
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InTarget);
	if (!ASC) return;

	if (InOperation == EScriptableTagOperation::AddTag)
	{
		ASC->AddLooseGameplayTag(InTag);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(InTag);
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

	FString OpStr = (Operation == EScriptableTagOperation::AddTag) ? TEXT("Add") : TEXT("Remove");

	if (Tag.IsValid())
	{
		return FText::Format(INVTEXT("{0} Tag [{1}] on {2}"), FText::FromString(OpStr), FText::FromName(Tag.GetTagName()), FText::FromString(TargetName));
	}

	return FText::Format(INVTEXT("{0} Gameplay Tag"), FText::FromString(OpStr));
}
#endif