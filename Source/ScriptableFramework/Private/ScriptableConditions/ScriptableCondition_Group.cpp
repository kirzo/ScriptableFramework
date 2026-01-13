// Copyright 2026 kirzo

#include "ScriptableConditions/ScriptableCondition_Group.h"

#define LOCTEXT_NAMESPACE "ScriptableCondition_Group"

#if WITH_EDITOR
FText UScriptableCondition_Group::GetDescription() const
{
	if (!GroupName.IsEmpty())
	{
		return FText::FromString(GroupName);
	}
	return LOCTEXT("GroupDesc", "Nested Group");
}
#endif

void UScriptableCondition_Group::OnRegister()
{
	Super::OnRegister();

	// Initializes the Requirement's internal map 
	// and injects THIS Group's Context into its children.
	Requirement.Register(this);
}

void UScriptableCondition_Group::OnUnregister()
{
	Super::OnUnregister();

	Requirement.Unregister();
}

bool UScriptableCondition_Group::Evaluate_Implementation() const
{
	return Requirement.Evaluate();
}

#undef LOCTEXT_NAMESPACE