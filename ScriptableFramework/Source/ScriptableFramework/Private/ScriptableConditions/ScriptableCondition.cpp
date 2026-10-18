// Copyright 2025 kirzo

#include "ScriptableConditions/ScriptableCondition.h"

bool UScriptableCondition::CheckCondition()
{
	ResolveBindings();
	// A native class cannot override Evaluate in Blueprint, so skip the ProcessEvent thunk.
	const bool bResult = GetClass()->HasAnyClassFlags(CLASS_Native) ? Evaluate_Implementation() : Evaluate();
	return IsNegated() ? !bResult : bResult;
}