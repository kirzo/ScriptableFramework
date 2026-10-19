// Copyright 2026 kirzo

#include "ScriptableNodes/ScriptableNode_Switch.h"

const FName UScriptableNode_Switch::InInputName = TEXT("In");
const FName UScriptableNode_Switch::DefaultOutputName = TEXT("Default");

FName UScriptableNode_Switch::MakeCaseOutputName(int32 CaseIndex)
{
	return FName(*FString::Printf(TEXT("Case %d"), CaseIndex));
}

TArray<FName> UScriptableNode_Switch::GetInputPins() const
{
	return { InInputName };
}

TArray<FName> UScriptableNode_Switch::GetDeclaredOutputPins() const
{
	// One output per case, in order, then the fixed Default last. Empty Cases -> just Default.
	TArray<FName> Outputs;
	Outputs.Reserve(Cases.Num() + 1);
	for (int32 Index = 0; Index < Cases.Num(); ++Index)
	{
		Outputs.Add(MakeCaseOutputName(Index));
	}
	Outputs.Add(DefaultOutputName);
	return Outputs;
}

void UScriptableNode_Switch::OnRegister()
{
	Super::OnRegister();
	for (FScriptableRequirement& Case : Cases)
	{
		RegisterRequirement(Case);
	}
}

void UScriptableNode_Switch::OnUnregister()
{
	for (FScriptableRequirement& Case : Cases)
	{
		Case.Unregister();
	}
	Super::OnUnregister();
}

void UScriptableNode_Switch::ProcessInput(FName InputName)
{
	if (InputName != InInputName) return;

	// First passing case wins; fall back to Default when none pass.
	FName ChosenOutput = DefaultOutputName;
	for (int32 Index = 0; Index < Cases.Num(); ++Index)
	{
		if (Cases[Index].Evaluate())
		{
			ChosenOutput = MakeCaseOutputName(Index);
			break;
		}
	}

	// Arm only the chosen output (like Branch): the ActivateInput suppress scope hides the transient
	// zero-pin state between consuming the input and arming the output.
	MarkInputInactive(InInputName);
	MarkOutputActive(ChosenOutput);
	FireOutput(ChosenOutput);
}
