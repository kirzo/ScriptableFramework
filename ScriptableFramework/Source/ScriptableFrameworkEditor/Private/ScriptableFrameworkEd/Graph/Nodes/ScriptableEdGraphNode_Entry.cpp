// Copyright 2026 kirzo

#include "ScriptableFrameworkEd/Graph/Nodes/ScriptableEdGraphNode_Entry.h"
#include "ScriptableNodes/ScriptableNode_Entry.h"
#include "ScriptableFrameworkEditorStyle.h"

UScriptableEdGraphNode_Entry::UScriptableEdGraphNode_Entry()
{
	RuntimeNodeClass = UScriptableNode_Entry::StaticClass();
}

FText UScriptableEdGraphNode_Entry::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("ScriptableEdGraphNode_Entry", "EntryTitle", "Entry");
}

FLinearColor UScriptableEdGraphNode_Entry::GetNodeTitleColor() const
{
	return FScriptableFrameworkEditorStyle::GraphColor;
}

FSlateIcon UScriptableEdGraphNode_Entry::GetIconAndTint(FLinearColor& OutColor) const
{
	OutColor = FLinearColor::White;
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Event_16x");
}