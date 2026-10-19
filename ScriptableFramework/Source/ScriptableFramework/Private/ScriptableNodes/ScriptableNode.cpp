// Copyright 2026 kirzo

#include "ScriptableNodes/ScriptableNode.h"
#include "ScriptableNodes/ScriptableGraph.h"
#include "ScriptableNodes/ScriptableGraphInstance.h"
#include "ScriptableConditions/ScriptableRequirement.h"
#include "HAL/IConsoleManager.h"

const FName UScriptableNode::StopInputName = TEXT("Stop");
const FName UScriptableNode::StoppedOutputName = TEXT("Stopped");

#if WITH_EDITOR
UScriptableNode::FOnNodeActivatedEditor UScriptableNode::OnNodeActivatedEditor;
#endif

namespace
{
	/** -1 = use per-node/graph setting. 0=Off, 1=Log, 2=Verbose. Lets the user force trace verbosity mid-PIE. */
	int32 GScriptableTraceLevelOverride = -1;
	FAutoConsoleVariableRef CVarScriptableTraceLevel(
		TEXT("scriptable.TraceLevel"),
		GScriptableTraceLevelOverride,
		TEXT("Global override for UScriptableNode trace verbosity. -1=Use asset settings, 0=Off, 1=Log, 2=Verbose. Max-verbosity wins."),
		ECVF_Default);
}

const UScriptableGraph* UScriptableNode::FindOwningAsset() const
{
	if (const UScriptableGraph* Asset = GetTypedOuter<UScriptableGraph>()) return Asset;
	if (const UScriptableGraphInstance* Inst = GetTypedOuter<UScriptableGraphInstance>()) return Inst->GetAsset();
	return nullptr;
}

FString UScriptableNode::GetTraceLabel() const
{
	FString Label = GetClass()->GetName();
	Label.RemoveFromStart(TEXT("ScriptableNode_"));
	return Label;
}

EScriptableTraceLevel UScriptableNode::GetEffectiveTraceLevel() const
{
	uint8 Best = static_cast<uint8>(TraceLevel);

	if (GScriptableTraceLevelOverride >= 0)
	{
		Best = FMath::Max(Best, static_cast<uint8>(FMath::Clamp(GScriptableTraceLevelOverride, 0, 2)));
	}

	if (const UScriptableGraph* Asset = FindOwningAsset())
	{
		Best = FMath::Max(Best, static_cast<uint8>(Asset->DefaultNodeTraceLevel));
	}

	return static_cast<EScriptableTraceLevel>(Best);
}

TArray<FName> UScriptableNode::GetOutputPins() const
{
	TArray<FName> Outputs = GetDeclaredOutputPins();

	// Auto-append the Stopped output when the node accepts Stop.
	const TArray<FName> Inputs = GetInputPins();
	if (Inputs.Contains(StopInputName) && !Outputs.Contains(StoppedOutputName))
	{
		Outputs.Add(StoppedOutputName);
	}

	return Outputs;
}

void UScriptableNode::ActivateInput(FName InputName)
{
	const EScriptableTraceLevel Effective = GetEffectiveTraceLevel();
	if (Effective != EScriptableTraceLevel::Off)
	{
		const UScriptableGraph* Asset = FindOwningAsset();
		const FString AssetName = Asset ? Asset->GetName() : TEXT("<unknown>");
		UE_LOG(LogScriptableObject, Log, TEXT("[%s.%s] activate input '%s'"), *AssetName, *GetTraceLabel(), *InputName.ToString());
	}

#if WITH_EDITOR
	OnNodeActivatedEditor.Broadcast(this);
#endif

	ActiveInputPins.Add(InputName);

	if (CanProcessInput(InputName))
	{
		// Treat the ProcessInput call as one atomic transition. Any pin churn inside (consuming
		// the input, arming outputs, even firing them on sync tasks) must not be observable to
		// the runner as a "node went inactive" event partway through; only the post-state matters.
		++InactiveNotificationsSuppressed;
		ProcessInput(InputName);
		--InactiveNotificationsSuppressed;

		NotifyIfInactive();
	}
}

void UScriptableNode::MarkInputInactive(FName InputName)
{
	ActiveInputPins.Remove(InputName);
	NotifyIfInactive();
}

void UScriptableNode::MarkOutputActive(FName OutputName)
{
	ActiveOutputPins.Add(OutputName);
}

void UScriptableNode::MarkOutputInactive(FName OutputName)
{
	ActiveOutputPins.Remove(OutputName);
	NotifyIfInactive();
}

void UScriptableNode::FireOutput(FName OutputName)
{
	if (GetEffectiveTraceLevel() == EScriptableTraceLevel::Verbose)
	{
		const UScriptableGraph* Asset = FindOwningAsset();
		const FString AssetName = Asset ? Asset->GetName() : TEXT("<unknown>");
		UE_LOG(LogScriptableObject, Log, TEXT("[%s.%s] fire output '%s'"), *AssetName, *GetTraceLabel(), *OutputName.ToString());
	}

	// Broadcast first so the runner can enqueue downstream activations while this output is still "active".
	OnPinFiredNative.Broadcast(this, OutputName);

	// Then mark inactive and check for completion.
	ActiveOutputPins.Remove(OutputName);
	NotifyIfInactive();
}

void UScriptableNode::DeactivateAllOutputs()
{
	if (ActiveOutputPins.IsEmpty()) return;

	ActiveOutputPins.Reset();
	NotifyIfInactive();
}

void UScriptableNode::RegisterRequirement(FScriptableRequirement& Requirement)
{
	// The requirement registers against the graph owner, not this node, so the graph scope is handed over explicitly.
	Requirement.SetInheritedContext(GetContext());
	if (Requirement.LocalsDefinitions.Num() == 0)
	{
		Requirement.SetInheritedLocals(GetMutableLocals());
	}

	Requirement.Register(GetOwner());

	if (const TMap<FGuid, TObjectPtr<UScriptableObject>>* GraphSources = GetBindingsMap())
	{
		Requirement.AppendBindingSources(*GraphSources);
	}
}

void UScriptableNode::NotifyIfInactive()
{
	if (InactiveNotificationsSuppressed > 0) return;

	if (!IsNodeActive())
	{
		OnNodeInactiveNative.Broadcast(this);
	}
}