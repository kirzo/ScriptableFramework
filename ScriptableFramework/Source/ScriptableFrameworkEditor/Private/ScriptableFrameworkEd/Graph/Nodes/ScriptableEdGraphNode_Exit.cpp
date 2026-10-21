// Copyright 2026 kirzo

#include "ScriptableFrameworkEd/Graph/Nodes/ScriptableEdGraphNode_Exit.h"
#include "ScriptableNodes/ScriptableNode_Exit.h"
#include "ScriptableFrameworkEditorStyle.h"

#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"

UScriptableEdGraphNode_Exit::UScriptableEdGraphNode_Exit()
{
	RuntimeNodeClass = UScriptableNode_Exit::StaticClass();
}

FLinearColor UScriptableEdGraphNode_Exit::GetNodeTitleColor() const
{
	return FScriptableFrameworkEditorStyle::ExitColor;
}

FSlateIcon UScriptableEdGraphNode_Exit::GetIconAndTint(FLinearColor& OutColor) const
{
	OutColor = FLinearColor::White;
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Conduit_16x");
}
