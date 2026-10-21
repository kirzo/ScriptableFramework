// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "SGraphNode.h"

/**
 * Shared SGraphNode base for scriptable nodes. Adds the BP-style breakpoint overlay icon
 * (Kismet.DebuggerOverlay.Breakpoint.Enabled/Disabled) when the owning UScriptableGraph asset
 * has the node in its Breakpoints map. The factory uses it as fallback for ed-nodes without a
 * custom slate widget; custom slate widgets inherit from it to get the same overlay for free.
 *
 * External modules that ship their own SGraphNode for a UScriptableEdGraphNode subclass should
 * derive from this class to get the breakpoint visual. Skipping it loses only the overlay —
 * the schema menu, F9 shortcut, storage and PIE-pause flow all keep working regardless.
 */
class SCRIPTABLEFRAMEWORKEDITOR_API SScriptableGraphNode_Base : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SScriptableGraphNode_Base) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphNode* InNode);

	//~ SGraphNode interface
	virtual void GetOverlayBrushes(bool bSelected, const FVector2f& WidgetSize, TArray<FOverlayBrushInfo>& Brushes) const override;
	virtual FSlateColor GetNodeTitleColor() const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	//~ End of SGraphNode interface
};
