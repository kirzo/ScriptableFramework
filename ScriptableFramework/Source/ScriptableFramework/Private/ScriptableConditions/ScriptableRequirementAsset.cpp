// Copyright 2025 kirzo

#include "ScriptableConditions/ScriptableRequirementAsset.h"
#include "ScriptableConditions/ScriptableCondition.h"
#include "ScriptableConditions/ScriptableCondition_NestedRequirement.h"

void UScriptableCondition_Asset::OnRegister()
{
	Super::OnRegister();

	// Requirements register on every evaluation, so the duplicated group is cached and only rebuilt when the asset changes.
	if (!ConditionAsset.IsValid() || ConditionAsset.Get() != Asset)
	{
		Condition = nullptr;
		ConditionAsset = Asset.Get();

		if (Asset)
		{
			// A Group already encapsulates FScriptableRequirement evaluation + bindings, and inherits our context on Register.
			UScriptableCondition_NestedRequirement* Group = NewObject<UScriptableCondition_NestedRequirement>(this, NAME_None, RF_Transient);
			Group->Requirement.Mode = Asset->Requirement.Mode;
			Group->Requirement.bNegate = Asset->Requirement.bNegate;

			// Duplicate so registering never touches the conditions inside the asset (Outer/runtime data).
			Group->Requirement.Conditions.Reset(Asset->Requirement.Conditions.Num());
			for (const UScriptableCondition* SourceCond : Asset->Requirement.Conditions)
			{
				if (SourceCond)
				{
					Group->Requirement.Conditions.Add(DuplicateObject<UScriptableCondition>(SourceCond, Group));
				}
			}

			Condition = Group;

#if WITH_EDITOR
			// Editing the asset during PIE must reach the cached copy.
			if (!OnObjectPropertyChangedHandle.IsValid())
			{
				OnObjectPropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &UScriptableCondition_Asset::OnObjectPropertyChanged);
			}
#endif
		}
	}

	if (Condition)
	{
		// Hand our context, locals and bindings map to the Group, which hands them to its conditions.
		PropagateRuntimeData(Condition);
		Condition->Register(GetOwner());
	}
}

void UScriptableCondition_Asset::OnUnregister()
{
	if (Condition)
	{
		Condition->Unregister();
	}

	Super::OnUnregister();
}

bool UScriptableCondition_Asset::Evaluate_Implementation() const
{
	if (Condition)
	{
		return Condition->CheckCondition();
	}
	return false;
}

#if WITH_EDITOR
FText UScriptableCondition_Asset::GetDisplayTitle() const
{
	return Asset ? FText::FromString(Asset->GetName()) : INVTEXT("None");
}

void UScriptableCondition_Asset::OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event)
{
	// Covers edits on the asset itself and on its condition subobjects (undo/redo goes through PostEditChange too).
	const UScriptableRequirementAsset* Source = ConditionAsset.Get();
	if (Source && Object && (Object == Source || Object->IsIn(Source)))
	{
		ConditionAsset.Reset();
	}
}
#endif