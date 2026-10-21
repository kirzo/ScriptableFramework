// Copyright 2026 kirzo

#include "ScriptableFrameworkEd/Graph/Nodes/ScriptableEdGraphNode_Switch.h"
#include "ScriptableNodes/ScriptableNode_Switch.h"
#include "ScriptableFrameworkEditorStyle.h"

#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"

UScriptableEdGraphNode_Switch::UScriptableEdGraphNode_Switch()
{
	RuntimeNodeClass = UScriptableNode_Switch::StaticClass();
}

FLinearColor UScriptableEdGraphNode_Switch::GetNodeTitleColor() const
{
	return FScriptableFrameworkEditorStyle::RequirementColor;
}

FSlateIcon UScriptableEdGraphNode_Switch::GetIconAndTint(FLinearColor& OutColor) const
{
	OutColor = FLinearColor::White;
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Switch_16x");
}
