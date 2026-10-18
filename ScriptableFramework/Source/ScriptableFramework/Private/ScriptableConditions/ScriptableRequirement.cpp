// Copyright 2026 kirzo

#include "ScriptableConditions/ScriptableRequirement.h"
#include "ScriptableConditions/ScriptableCondition.h"
#include "ScriptableContext.h"
#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

void FScriptableRequirement::Register(UObject* InOwner)
{
	if (bIsRegistered)
	{
		return;
	}

	Super::Register(InOwner);

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

	bIsRegistered = false;
	Super::Unregister();
}

FScriptableRequirement FScriptableRequirement::Clone(UObject* NewOuter) const
{
	// Shallow copy carries the persisted fields (Mode, bNegate, context/locals definitions) and the condition
	// pointers; the deep copy below re-owns the Instanced conditions under NewOuter.
	FScriptableRequirement Cloned = *this;
	Cloned.bIsRegistered = false;

	// Rebuild the transient context so the copy doesn't share the template's runtime bag.
	Cloned.ConstructContext();

	// Deep-copy the Instanced conditions under NewOuter. A plain struct copy only duplicated the pointers, which stay
	// owned by this requirement's outer (e.g. an editor-only graph node stripped on cook) — so the copy would lose them.
	Cloned.Conditions.Empty(Conditions.Num());
	for (const TObjectPtr<UScriptableCondition>& Condition : Conditions)
	{
		if (Condition)
		{
			Cloned.Conditions.Add(DuplicateObject<UScriptableCondition>(Condition, NewOuter));
		}
	}

	return Cloned;
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

bool FScriptableRequirement::EvaluateRequirement(UObject* Owner, const FScriptableRequirement& Requirement)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FScriptableRequirement::EvaluateRequirement);

	if (!Owner) return false;

	FScriptableRequirement& MutableReq = const_cast<FScriptableRequirement&>(Requirement);

	MutableReq.Register(Owner);
	const bool bResult = MutableReq.Evaluate();
	MutableReq.Unregister();

	return bResult;
}

bool FScriptableRequirement::EvaluateRequirement(UObject* Owner, const FScriptableRequirement& Requirement, const FScriptableContext& InContext)
{
	if (!Owner) return false;

	FScriptableRequirement& MutableReq = const_cast<FScriptableRequirement&>(Requirement);

	MutableReq.SetContext(InContext);

	return EvaluateRequirement(Owner, Requirement);
}