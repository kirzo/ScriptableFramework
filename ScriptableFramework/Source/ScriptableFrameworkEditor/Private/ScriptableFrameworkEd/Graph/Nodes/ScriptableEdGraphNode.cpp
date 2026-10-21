// Copyright 2026 kirzo

#include "ScriptableFrameworkEd/Graph/ScriptableEdGraphNode.h"
#include "ScriptableNodes/ScriptableNode.h"
#include "ScriptableNodes/ScriptableGraph.h"
#include "ScriptableFrameworkEditorStyle.h"
#include "ScriptableFrameworkEditorHelpers.h"
#include "EdGraph/EdGraph.h"

const FName UScriptableEdGraphNode::ScriptableExecPinCategory = TEXT("ScriptableExec");

void UScriptableEdGraphNode::AllocateDefaultPins()
{
	if (!RuntimeNode) return;

	for (const FName& InputName : RuntimeNode->GetInputPins())
	{
		CreatePin(EGPD_Input, ScriptableExecPinCategory, InputName);
	}

	for (const FName& OutputName : RuntimeNode->GetOutputPins())
	{
		CreatePin(EGPD_Output, ScriptableExecPinCategory, OutputName);
	}
}

FText UScriptableEdGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (RuntimeNode)
	{
		return RuntimeNode->GetClass()->GetDisplayNameText();
	}
	return NSLOCTEXT("ScriptableEdGraphNode", "DefaultTitle", "Scriptable Node");
}

FLinearColor UScriptableEdGraphNode::GetNodeTitleColor() const
{
	if (RuntimeNode)
	{
		const FString Category = RuntimeNode->GetClass()->GetMetaData(ScriptableFrameworkEditor::MD_NodeCategory);
		if (!Category.IsEmpty())
		{
			if (Category.StartsWith(TEXT("Condition"), ESearchCase::IgnoreCase)) return FScriptableFrameworkEditorStyle::RequirementColor;
			if (Category.StartsWith(TEXT("System"), ESearchCase::IgnoreCase)) return FScriptableFrameworkEditorStyle::FlowColor;
		}
	}

	return FScriptableFrameworkEditorStyle::TaskColor;
}

void UScriptableEdGraphNode::DestroyNode()
{
	// Capture the asset reference before Super::DestroyNode unhooks us from the graph.
	UEdGraph* OwningGraph = GetGraph();
	UScriptableGraph* GraphAsset = OwningGraph ? Cast<UScriptableGraph>(OwningGraph->GetOuter()) : nullptr;
	UScriptableNode* RuntimeNodeToRemove = RuntimeNode;

	Super::DestroyNode();

	// Entry node is auto-repaired by UScriptableGraph::EnsureEntryNode; never strip it from the asset.
	if (!GraphAsset || !RuntimeNodeToRemove) return;
	if (RuntimeNodeToRemove->GetBindingID() == GraphAsset->EntryNodeID) return;

	GraphAsset->Modify();

	// Remove the node's connections too. Super::DestroyNode only breaks the visual pin links; without
	// this the asset's Connections list keeps dangling entries that reference the now-missing node
	// (visible in validation) until PruneOrphanConnections clears them on the next load.
	const FGuid RemovedID = RuntimeNodeToRemove->GetBindingID();
	GraphAsset->Connections.RemoveAll([&RemovedID](const FScriptableGraphConnection& Conn)
		{
			return Conn.From.NodeID == RemovedID || Conn.To.NodeID == RemovedID;
		});

	GraphAsset->Nodes.Remove(RuntimeNodeToRemove);
}

void UScriptableEdGraphNode::ReconstructNode()
{
	TArray<UEdGraphPin*> OldPins = MoveTemp(Pins);
	Pins.Reset();

	AllocateDefaultPins();

	TArray<UEdGraphPin*> PinsToDestroy;
	for (UEdGraphPin* OldPin : OldPins)
	{
		if (!OldPin) continue;

		if (UEdGraphPin* NewPin = FindPin(OldPin->PinName, OldPin->Direction))
		{
			for (UEdGraphPin* Linked : OldPin->LinkedTo)
			{
				if (Linked) NewPin->MakeLinkTo(Linked);
			}

			// Sever the old pin from its peers manually to avoid triggering the Schema.
			// Calling BreakAllPinLinks here would corrupt Asset->Connections during reconstruction.
			for (UEdGraphPin* Linked : OldPin->LinkedTo)
			{
				if (Linked) Linked->LinkedTo.Remove(OldPin);
			}
			OldPin->LinkedTo.Empty();
			PinsToDestroy.Add(OldPin);
		}
		else if (!OldPin->LinkedTo.IsEmpty())
		{
			/** BP-style orphan: the runtime no longer exposes a pin with this name but the user still has
			 * wires attached. Keep the pin visible (rendered red, including its wires) so the user can
			 * either restore the missing event/output in the sub-asset or rewire/delete explicitly. */
			OldPin->bOrphanedPin = true;
			Pins.Add(OldPin);
		}
		else
		{
			PinsToDestroy.Add(OldPin);
		}
	}

	for (UEdGraphPin* OldPin : PinsToDestroy)
	{
		DestroyPin(OldPin);
	}

	ApplyPinVisibility();

	if (UEdGraph* OwningGraph = GetGraph())
	{
		OwningGraph->NotifyGraphChanged();
	}
}

void UScriptableEdGraphNode::ApplyPinVisibility()
{
	/** Orphan pins exist only to keep dangling wires visible; once they have no peers left they're noise. */
	TArray<UEdGraphPin*> OrphansToDestroy;
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin && Pin->bOrphanedPin && Pin->LinkedTo.IsEmpty())
		{
			OrphansToDestroy.Add(Pin);
		}
	}
	for (UEdGraphPin* Orphan : OrphansToDestroy)
	{
		Pins.Remove(Orphan);
		DestroyPin(Orphan);
	}

	for (UEdGraphPin* Pin : Pins)
	{
		if (!Pin) continue;
		Pin->bHidden = bHideUnconnectedPins && Pin->LinkedTo.IsEmpty();
	}

	if (!OrphansToDestroy.IsEmpty())
	{
		if (UEdGraph* OwningGraph = GetGraph()) OwningGraph->NotifyGraphChanged();
	}
}