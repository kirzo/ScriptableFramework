// Copyright 2026 kirzo

#include "ScriptableFrameworkEd/Validation/ScriptableBindingsValidator.h"
#include "ScriptableFrameworkEd/Validation/ScriptableBindingsValidation.h"
#include "ScriptableNodes/ScriptableGraph.h"
#include "ScriptableFrameworkEditorHelpers.h"

FName UScriptableBindingsValidator::GetValidatorId_Implementation() const
{
	return TEXT("ScriptableBindings");
}

bool UScriptableBindingsValidator::CanValidate_Implementation(const UObject* Asset) const
{
	return Asset && Asset->IsA<UScriptableGraph>();
}

void UScriptableBindingsValidator::Validate_Implementation(const UObject* Asset, TArray<FKzValidationIssue>& OutIssues) const
{
	const int32 FirstIssue = OutIssues.Num();
	FScriptableBindingsValidation::ValidateBindings(Asset, OutIssues);
	ScriptableFrameworkEditor::DropUnreachableIssues(Cast<UScriptableGraph>(Asset), OutIssues, FirstIssue);
}
