// Copyright 2026 kirzo

#include "ScriptableFrameworkEd/Graph/Widgets/SScriptableGraphNode_Base.h"
#include "ScriptableFrameworkEd/Graph/ScriptableEdGraphNode.h"
#include "ScriptableFrameworkEd/Debug/ScriptableDebugRegistry.h"
#include "ScriptableNodes/ScriptableGraph.h"
#include "ScriptableNodes/ScriptableGraphInstance.h"
#include "ScriptableNodes/ScriptableNode.h"

#include "Styling/AppStyle.h"

void SScriptableGraphNode_Base::Construct(const FArguments& InArgs, UEdGraphNode* InNode)
{
	GraphNode = InNode;
	SetCursor(EMouseCursor::CardinalCross);
	UpdateGraphNode();
}

void SScriptableGraphNode_Base::GetOverlayBrushes(bool bSelected, const FVector2f& WidgetSize, TArray<FOverlayBrushInfo>& Brushes) const
{
	SGraphNode::GetOverlayBrushes(bSelected, WidgetSize, Brushes);

	const UScriptableEdGraphNode* SfNode = Cast<UScriptableEdGraphNode>(GraphNode);
	if (!SfNode || !SfNode->GetRuntimeNode()) return;

	const UScriptableGraph* Asset = SfNode->GetTypedOuter<UScriptableGraph>();
	if (!Asset) return;
	const FGuid TargetId = SfNode->GetRuntimeNode()->GetBindingID();

	/** Breakpoint overlay (filled when enabled, hollow when disabled). Independent of the active halo. */
	if (const bool* EnabledPtr = Asset->Breakpoints.Find(TargetId))
	{
		FOverlayBrushInfo BreakpointInfo;
		BreakpointInfo.Brush = FAppStyle::GetBrush(*EnabledPtr
			? TEXT("Kismet.DebuggerOverlay.Breakpoint.EnabledAndValid")
			: TEXT("Kismet.DebuggerOverlay.Breakpoint.Disabled"));
		if (BreakpointInfo.Brush)
		{
			BreakpointInfo.OverlayOffset -= BreakpointInfo.Brush->ImageSize / 2.f;
			Brushes.Add(BreakpointInfo);
		}
	}

	/** Active-node halo rendered as a tinted box via OnPaint — the Kismet InstructionPointer brush is 72x72 and dominates the canvas when multiple nodes are active at once. */
}

FSlateColor SScriptableGraphNode_Base::GetNodeTitleColor() const
{
	// The engine's title spill texture greys and darkens the colour by about half, so the palette goes in brighter and more saturated
	constexpr float SaturationBoost = 1.3f;
	constexpr float ValueBoost = 1.15f;

	const FLinearColor Color = SGraphNode::GetNodeTitleColor().GetSpecifiedColor();
	FLinearColor HSV = Color.ToFColorSRGB().ReinterpretAsLinear().LinearRGBToHSV();
	HSV.G = FMath::Min(HSV.G * SaturationBoost, 1.0f);
	HSV.B = FMath::Min(HSV.B * ValueBoost, 1.0f);

	FLinearColor Boosted = FLinearColor::FromSRGBColor(HSV.HSVToLinearRGB().ToFColor(false));
	Boosted.A = Color.A;
	return Boosted;
}

int32 SScriptableGraphNode_Base::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	bool bActive = false;
	if (const UScriptableEdGraphNode* SfNode = Cast<UScriptableEdGraphNode>(GraphNode))
	{
		if (UScriptableNode* RuntimeNode = SfNode->GetRuntimeNode())
		{
			if (const UScriptableGraph* Asset = SfNode->GetTypedOuter<UScriptableGraph>())
			{
				if (const UScriptableGraphInstance* Debug = FScriptableDebugRegistry::GetDebugInstance(Asset))
				{
					if (Debug->IsRunning())
					{
						const FGuid TargetId = RuntimeNode->GetBindingID();
						for (UScriptableNode* ActiveNode : Debug->GetActiveNodes())
						{
							if (ActiveNode && ActiveNode->GetBindingID() == TargetId) { bActive = true; break; }
						}
					}
				}
			}
		}
	}

	if (bActive)
	{
		const FSlateBrush* HaloBrush = FAppStyle::GetBrush(TEXT("Graph.Node.ShadowSelected"));
		if (HaloBrush)
		{
			const FLinearColor HaloColor(1.0f, 0.45f, 0.0f, 1.0f);
			const FVector2f Inflation(14.0f, 14.0f);
			const FVector2f LocalSize = AllottedGeometry.GetLocalSize();
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(LocalSize + Inflation * 2.f, FSlateLayoutTransform(-Inflation)),
				HaloBrush,
				ESlateDrawEffect::None,
				HaloColor);
		}
	}

	/** Shift the node body up one layer so it draws over the halo we just laid down. */
	return SGraphNode::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + (bActive ? 1 : 0), InWidgetStyle, bParentEnabled);
}