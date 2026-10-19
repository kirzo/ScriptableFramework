// Copyright 2026 kirzo

#include "ScriptableNodes/ScriptableNode_Branch.h"

const FName UScriptableNode_Branch::InInputName = TEXT("In");
const FName UScriptableNode_Branch::TrueOutputName = TEXT("True");
const FName UScriptableNode_Branch::FalseOutputName = TEXT("False");

TArray<FName> UScriptableNode_Branch::GetInputPins() const
{
	return { InInputName };
}

TArray<FName> UScriptableNode_Branch::GetDeclaredOutputPins() const
{
	return { TrueOutputName, FalseOutputName };
}

void UScriptableNode_Branch::OnRegister()
{
	Super::OnRegister();
	RegisterRequirement(Requirement);
}

void UScriptableNode_Branch::OnUnregister()
{
	Requirement.Unregister();
	Super::OnUnregister();
}

void UScriptableNode_Branch::ProcessInput(FName InputName)
{
	if (InputName != InInputName) return;

	const bool bResult = Requirement.Evaluate();
	const FName ChosenOutput = bResult ? TrueOutputName : FalseOutputName;

	// Arm only the chosen output (arming the other would just be marked inactive without firing).
	// The ActivateInput suppress scope hides the transient zero-pin state between consume and arm.
	MarkInputInactive(InInputName);
	MarkOutputActive(ChosenOutput);
	FireOutput(ChosenOutput);
}