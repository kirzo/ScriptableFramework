// Copyright 2026 kirzo

#include "ScriptableFrameworkEd/Graph/Nodes/ScriptableEdGraphNode_SubGraph.h"
#include "ScriptableNodes/ScriptableNode_SubGraph.h"
#include "ScriptableFrameworkEditorStyle.h"

UScriptableEdGraphNode_SubGraph::UScriptableEdGraphNode_SubGraph()
{
	RuntimeNodeClass = UScriptableNode_SubGraph::StaticClass();
}

FLinearColor UScriptableEdGraphNode_SubGraph::GetNodeTitleColor() const
{
	// Same teal as the UScriptableGraph asset entry — signals the node embeds a graph.
	return FScriptableFrameworkEditorStyle::ScriptableGraphColor;
}

FSlateIcon UScriptableEdGraphNode_SubGraph::GetIconAndTint(FLinearColor& OutColor) const
{
	OutColor = FLinearColor::White;
	return FSlateIcon(FScriptableFrameworkEditorStyle::Get().GetStyleSetName(), "ClassIcon.ScriptableGraph");
}

bool UScriptableEdGraphNode_SubGraph::ShouldShowPinLabel(FName PinName) const
{
	/** "In" is the implicit start pin and reads as noise alongside named event pins; Cancel and events keep their labels. */
	if (PinName == UScriptableNode_SubGraph::InInputName) return false;
	return Super::ShouldShowPinLabel(PinName);
}
