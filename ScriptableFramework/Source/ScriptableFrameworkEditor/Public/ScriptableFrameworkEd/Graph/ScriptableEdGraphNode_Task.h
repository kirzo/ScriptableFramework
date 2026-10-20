// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableFrameworkEd/Graph/ScriptableEdGraphNode.h"
#include "ScriptableEdGraphNode_Task.generated.h"

/** Visual node wrapping a UScriptableNode_Task runtime instance. */
UCLASS()
class SCRIPTABLEFRAMEWORKEDITOR_API UScriptableEdGraphNode_Task : public UScriptableEdGraphNode
{
	GENERATED_BODY()

public:
	UScriptableEdGraphNode_Task();

	//~ UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual bool ShowPaletteIconOnNode() const override { return true; }
	//~ End of UEdGraphNode interface

	virtual bool ShouldShowPinLabel(FName PinName) const override;
};