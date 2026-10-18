// Copyright 2026 kirzo

#include "ScriptableConditions/ScriptableCondition_NestedRequirement.h"

void UScriptableCondition_NestedRequirement::OnRegister()
{
	Super::OnRegister();

	// Hand the parent scope's context to the inner conditions by pointer: Requirement.Register passes the
	// wrapper's owner, not the wrapper, so AddBindingSource cannot fall back to our ContextRef on its own.
	Requirement.SetInheritedContext(GetContext());

	// Inherit the parent scope's Locals when this nested unit doesn't declare its own. Cannot copy
	// because SetLocal-style writes have to propagate back to the parent's bag; point GetLocals at the
	// external bag instead. When the nested unit DOES declare its own LocalsDefinitions it stays isolated.
	if (Requirement.LocalsDefinitions.Num() == 0)
	{
		Requirement.SetInheritedLocals(GetMutableLocals());
	}

	Requirement.Register(GetOwner());
}

void UScriptableCondition_NestedRequirement::OnUnregister()
{
	Super::OnUnregister();

	Requirement.Unregister();
}

bool UScriptableCondition_NestedRequirement::Evaluate_Implementation() const
{
	return Requirement.Evaluate();
}
