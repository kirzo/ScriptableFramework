// Copyright 2026 kirzo

#include "ScriptableFrameworkEd/Graph/Nodes/ScriptableEdGraphNode_Finish.h"
#include "ScriptableNodes/ScriptableNode_Finish.h"
#include "ScriptableFrameworkEditorStyle.h"

#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"

UScriptableEdGraphNode_Finish::UScriptableEdGraphNode_Finish()
{
	RuntimeNodeClass = UScriptableNode_Finish::StaticClass();
}

FText UScriptableEdGraphNode_Finish::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	// Runtime's dynamic "Finish (OutputName)" title.
	if (RuntimeNode)
	{
		return RuntimeNode->GetDisplayTitle();
	}
	return NSLOCTEXT("ScriptableEdGraphNode_Finish", "DefaultTitle", "Finish");
}

FLinearColor UScriptableEdGraphNode_Finish::GetNodeTitleColor() const
{
	// Same red as Exit — both belong to the "graph terminator" family.
	return FScriptableFrameworkEditorStyle::ExitColor;
}

FSlateIcon UScriptableEdGraphNode_Finish::GetIconAndTint(FLinearColor& OutColor) const
{
	OutColor = FLinearColor::White;
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Check");
}
