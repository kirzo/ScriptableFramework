// Copyright 2025 kirzo

#include "ScriptableConditions/ScriptableCondition.h"

#if WITH_EDITOR
FText UScriptableCondition::GetDescription() const
{
	return GetClass()->GetDisplayNameText();
}
#endif

bool UScriptableCondition::CheckCondition()
{
	ResolveBindings();
	const bool bResult = Evaluate();
	return IsNegated() ? !bResult : bResult;
}

