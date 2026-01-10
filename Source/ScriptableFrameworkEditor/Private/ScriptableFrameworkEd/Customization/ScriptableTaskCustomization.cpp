// Copyright 2026 kirzo

#include "ScriptableTaskCustomization.h"
#include "ScriptableTasks/ScriptableTask.h"
#include "ScriptableFrameworkEditorStyle.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "IPropertyUtilities.h"

UE_DISABLE_OPTIMIZATION

#define LOCTEXT_NAMESPACE "FScriptableTaskCustomization"

TSharedPtr<IPropertyHandle> FScriptableTaskCustomization::GetControlProperty(FName PropertyName) const
{
	if (PropertyHandle.IsValid())
	{
		// Access: Task -> Control -> Property
		return PropertyHandle->GetChildHandle("Control")->GetChildHandle(PropertyName);
	}
	return nullptr;
}

TSharedPtr<SHorizontalBox> FScriptableTaskCustomization::GetHeaderValueContent()
{
	TSharedPtr<SHorizontalBox> Box = SNew(SHorizontalBox);// Super::GetHeaderValueContent();
	if (!Box.IsValid()) Box = SNew(SHorizontalBox);

	// --- DoOnce Toggle ---
	Box->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 6, 0)
		[
			SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.OnClicked(this, &FScriptableTaskCustomization::OnDoOnceClicked)
				.ToolTipText(GetDoOnceTooltip())
				.ContentPadding(0)
				[
					SNew(SBorder)
						.Padding(FMargin(8, 1))
						.BorderImage(FScriptableFrameworkEditorStyle::Get().GetBrush("ScriptableFramework.Param.Background"))
						.BorderBackgroundColor(this, &FScriptableTaskCustomization::GetDoOnceColor)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text(this, &FScriptableTaskCustomization::GetDoOnceText)
								.TextStyle(FScriptableFrameworkEditorStyle::Get(), "ScriptableFramework.Param.Label")
								.ColorAndOpacity(FLinearColor::White)
						]
				]
		];

	// --- Loop Toggle ---
	Box->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 6, 0)
		[
			SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.OnClicked(this, &FScriptableTaskCustomization::OnLoopClicked)
				.ToolTipText(GetLoopTooltip())
				.ContentPadding(0)
				[
					SNew(SBorder)
						.Padding(FMargin(8, 1))
						.BorderImage(FScriptableFrameworkEditorStyle::Get().GetBrush("ScriptableFramework.Param.Background"))
						.BorderBackgroundColor(this, &FScriptableTaskCustomization::GetLoopColor)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text(this, &FScriptableTaskCustomization::GetLoopText)
								.TextStyle(FScriptableFrameworkEditorStyle::Get(), "ScriptableFramework.Param.Label")
								.ColorAndOpacity(FLinearColor::White)
						]
				]
		];

	if (GetLoopValue())
	{
		TSharedPtr<IPropertyHandle> CountProp = GetControlProperty("LoopCount");
		Box->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 6, 0)
			[
				CountProp->CreatePropertyValueWidget()
			];
	}

	return Box;
}

// -------------------------------------------------------------------
// DoOnce Logic
// -------------------------------------------------------------------

bool FScriptableTaskCustomization::GetDoOnceValue() const
{
	if (TSharedPtr<IPropertyHandle> Prop = GetControlProperty("bDoOnce"))
	{
		bool bVal = false;
		Prop->GetValue(bVal);
		return bVal;
	}
	return false;
}

FReply FScriptableTaskCustomization::OnDoOnceClicked()
{
	if (TSharedPtr<IPropertyHandle> Prop = GetControlProperty("bDoOnce"))
	{
		bool bNewVal = !GetDoOnceValue();
		FScopedTransaction Transaction(LOCTEXT("ToggleDoOnce", "Toggle DoOnce"));
		Prop->SetValue(bNewVal);
	}
	return FReply::Handled();
}

FSlateColor FScriptableTaskCustomization::GetDoOnceColor() const
{
	// Orange if Active, Dark Gray/Subdued if Inactive
	return GetDoOnceValue() ? FLinearColor(0.8f, 0.4f, 0.1f) : FLinearColor(0.2f, 0.2f, 0.2f, 0.5f);
}

FText FScriptableTaskCustomization::GetDoOnceText() const
{
	return LOCTEXT("DoOnceLabel", "Once");
}

FText FScriptableTaskCustomization::GetDoOnceTooltip() const
{
	return LOCTEXT("DoOnceTooltip", "Execute this task only once.");
}

// -------------------------------------------------------------------
// Loop Logic
// -------------------------------------------------------------------

bool FScriptableTaskCustomization::GetLoopValue() const
{
	if (TSharedPtr<IPropertyHandle> Prop = GetControlProperty("bLoop"))
	{
		bool bVal = false;
		Prop->GetValue(bVal);
		return bVal;
	}
	return false;
}

FReply FScriptableTaskCustomization::OnLoopClicked()
{
	if (TSharedPtr<IPropertyHandle> Prop = GetControlProperty("bLoop"))
	{
		bool bNewVal = !GetLoopValue();
		FScopedTransaction Transaction(LOCTEXT("ToggleLoop", "Toggle Task Loop"));
		Prop->SetValue(bNewVal);
		if (PropertyUtilities) PropertyUtilities->ForceRefresh();
	}
	return FReply::Handled();
}

FSlateColor FScriptableTaskCustomization::GetLoopColor() const
{
	// Blue if Active, Dark Gray/Subdued if Inactive
	return GetLoopValue() ? FLinearColor(0.1f, 0.4f, 0.8f) : FLinearColor(0.2f, 0.2f, 0.2f, 0.5f);
}

FText FScriptableTaskCustomization::GetLoopText() const
{
	return LOCTEXT("LoopLabel", "Loop");
}

FText FScriptableTaskCustomization::GetLoopTooltip() const
{
	return LOCTEXT("LoopTooltip", "Toggle Looping. \nWhen enabled, this task will repeat upon completion.");
}

#undef LOCTEXT_NAMESPACE

UE_ENABLE_OPTIMIZATION