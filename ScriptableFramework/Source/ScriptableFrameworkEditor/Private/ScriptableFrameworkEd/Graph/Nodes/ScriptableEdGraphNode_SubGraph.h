// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableFrameworkEd/Graph/ScriptableEdGraphNode_Native.h"
#include "ScriptableEdGraphNode_SubGraph.generated.h"

/** Ed-node for UScriptableNode_SubGraph: title color matches the UScriptableGraph asset's color. */
UCLASS()
class UScriptableEdGraphNode_SubGraph : public UScriptableEdGraphNode_Native
{
	GENERATED_BODY()

public:
	UScriptableEdGraphNode_SubGraph();

	//~ UEdGraphNode interface
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual bool ShowPaletteIconOnNode() const override { return true; }
	//~ End of UEdGraphNode interface

	//~ UScriptableEdGraphNode interface
	virtual bool ShouldShowPinLabel(FName PinName) const override;
	//~ End of UScriptableEdGraphNode interface
};
