// Copyright 2026 kirzo

#include "ScriptableFrameworkEd/Graph/Nodes/ScriptableEdGraphNode_GoTo.h"
#include "ScriptableNodes/ScriptableNode_GoTo.h"
#include "ScriptableFrameworkEditorStyle.h"

#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"

UScriptableEdGraphNode_GoTo::UScriptableEdGraphNode_GoTo()
{
	RuntimeNodeClass = UScriptableNode_GoTo::StaticClass();
}

FText UScriptableEdGraphNode_GoTo::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	// Use the runtime node's dynamic title ("Go to X") rather than the generic class name.
	if (RuntimeNode)
	{
		return RuntimeNode->GetDisplayTitle();
	}
	return NSLOCTEXT("ScriptableEdGraphNode_GoTo", "DefaultTitle", "Go To");
}

FLinearColor UScriptableEdGraphNode_GoTo::GetNodeTitleColor() const
{
	return FScriptableFrameworkEditorStyle::FlowColor;
}

FSlateIcon UScriptableEdGraphNode_GoTo::GetIconAndTint(FLinearColor& OutColor) const
{
	OutColor = FLinearColor::White;
	// Targets an event, so reuse the event icon.
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Event_16x");
}
