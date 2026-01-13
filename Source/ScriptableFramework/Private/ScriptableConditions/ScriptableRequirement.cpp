// Copyright 2026 kirzo

#include "ScriptableConditions/ScriptableRequirement.h"
#include "ScriptableConditions/ScriptableCondition.h"
#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"

UE_DISABLE_OPTIMIZATION

void FScriptableRequirement::Register(UObject* InOwner)
{
	if (bIsRegistered)
	{
		return;
	}

	Owner = InOwner;
	BindingSourceMap.Reset(); // Clean slate

	// Filter invalid conditions
	for (int32 i = Conditions.Num() - 1; i >= 0; --i)
	{
		if (!Conditions[i])
		{
			Conditions.RemoveAt(i);
		}
	}

	for (UScriptableCondition* Condition : Conditions)
	{
		if (Condition)
		{
			// Add to local map and inject THIS Context into the condition
			AddBindingSource(Condition);

			if (Condition->IsEnabled())
			{
				Condition->Register(Owner);
			}
		}
	}

	bIsRegistered = true;
}

void FScriptableRequirement::Unregister()
{
	if (!bIsRegistered)
	{
		return;
	}

	for (UScriptableCondition* Condition : Conditions)
	{
		if (Condition && Condition->IsEnabled())
		{
			Condition->Unregister();
		}
	}

	BindingSourceMap.Empty();
	Owner = nullptr;
	bIsRegistered = false;
}

bool FScriptableRequirement::Evaluate() const
{
	bool bResult = true;

	if (Conditions.IsEmpty())
	{
		// AND: Empty = True, OR: Empty = False
		bResult = (Mode == EScriptableRequirementMode::And);
	}
	else
	{
		auto EvalPredicate = [](UScriptableCondition* Condition)
		{
			return Condition ? Condition->CheckCondition() : false;
		};

		if (Mode == EScriptableRequirementMode::And)
		{
			bResult = Algo::AllOf(Conditions, EvalPredicate);
		}
		else
		{
			bResult = Algo::AnyOf(Conditions, EvalPredicate);
		}
	}

	return bNegate ? !bResult : bResult;
}

UScriptableObject* FScriptableRequirement::FindBindingSource(const FGuid& InID) const
{
	if (const TObjectPtr<UScriptableObject>* Found = BindingSourceMap.Find(InID))
	{
		return Found->Get();
	}
	return nullptr;
}

void FScriptableRequirement::AddBindingSource(UScriptableObject* InSource)
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

bool FScriptableRequirement::EvaluateRequirement(UObject* Owner, FScriptableRequirement& Action)
{
	if (!Owner) return false;

	Action.Register(Owner);
	const bool bResult = Action.Evaluate();
	Action.Unregister();
	return bResult;
}

UE_ENABLE_OPTIMIZATION