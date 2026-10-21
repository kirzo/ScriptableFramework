// Copyright 2026 kirzo

#include "ScriptableFrameworkEd/Graph/Nodes/ScriptableEdGraphNode_Branch.h"
#include "ScriptableNodes/ScriptableNode_Branch.h"
#include "ScriptableFrameworkEditorStyle.h"

#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"

UScriptableEdGraphNode_Branch::UScriptableEdGraphNode_Branch()
{
	RuntimeNodeClass = UScriptableNode_Branch::StaticClass();
}

FLinearColor UScriptableEdGraphNode_Branch::GetNodeTitleColor() const
{
	return FScriptableFrameworkEditorStyle::RequirementColor;
}

FSlateIcon UScriptableEdGraphNode_Branch::GetIconAndTint(FLinearColor& OutColor) const
{
	OutColor = FLinearColor::White;
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Branch_16x");
}