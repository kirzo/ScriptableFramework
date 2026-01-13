// Copyright 2026 kirzo

#include "ScriptableConditionCustomization.h"
#include "ScriptableConditions/ScriptableCondition.h"
#include "ScriptableConditions/ScriptableRequirementAsset.h"

#include "ScriptableFrameworkEditorStyle.h"

#define LOCTEXT_NAMESPACE "FScriptableConditionCustomization"

UClass* FScriptableConditionCustomization::GetWrapperClass() const
{
	return UScriptableCondition_Asset::StaticClass();
}

TSharedPtr<SHorizontalBox> FScriptableConditionCustomization::GetHeaderValueContent()
{
	TSharedPtr<SHorizontalBox> Box = FScriptableObjectCustomization::GetHeaderValueContent();
	if (!Box.IsValid()) Box = SNew(SHorizontalBox);

	// --- Negate Toggle ---
	Box->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 6, 0)
		[
			SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.OnClicked(this, &FScriptableConditionCustomization::OnNegateClicked)
				.ToolTipText(GetNegateTooltip())
				.ContentPadding(0)
				[
					SNew(SBorder)
						.Padding(FMargin(6, 1))
						.BorderImage(FScriptableFrameworkEditorStyle::Get().GetBrush("ScriptableFramework.Param.Background"))
						.BorderBackgroundColor(this, &FScriptableConditionCustomization::GetNegateColor)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text(this, &FScriptableConditionCustomization::GetNegateText)
								.TextStyle(FScriptableFrameworkEditorStyle::Get(), "ScriptableFramework.Param.Label")
								.ColorAndOpacity(FLinearColor::White)
						]
				]
		];

	return Box;
}

FText FScriptableConditionCustomization::GetDisplayTitle(UScriptableObject* Obj) const
{
	if (IsWrapperClass(Obj->GetClass()))
	{
		return FScriptableObjectCustomization::GetDisplayTitle(Obj);
	}

	if (UScriptableCondition* Condition = Cast<UScriptableCondition>(Obj))
	{
		return Condition->GetDescription();
	}

	return FScriptableObjectCustomization::GetDisplayTitle(Obj);
}

// -------------------------------------------------------------------
// Negate Logic
// -------------------------------------------------------------------

bool FScriptableConditionCustomization::GetNegateValue() const
{
	if (PropertyHandle.IsValid())
	{
		if (TSharedPtr<IPropertyHandle> Prop = PropertyHandle->GetChildHandle(TEXT("bNegate")))
		{
			bool bVal = false;
			Prop->GetValue(bVal);
			return bVal;
		}
	}
	return false;
}

FReply FScriptableConditionCustomization::OnNegateClicked()
{
	if (PropertyHandle.IsValid())
	{
		if (TSharedPtr<IPropertyHandle> Prop = PropertyHandle->GetChildHandle(TEXT("bNegate")))
		{
			bool bNewVal = !GetNegateValue();
			FScopedTransaction Transaction(LOCTEXT("ToggleNegate", "Toggle Negate Condition"));
			Prop->SetValue(bNewVal);
		}
	}
	return FReply::Handled();
}

FSlateColor FScriptableConditionCustomization::GetNegateColor() const
{
	// Red/Orange if Negated (Active), Dark Gray if Normal (Inactive)
	return GetNegateValue() ? FScriptableFrameworkEditorStyle::NegateColor : FScriptableFrameworkEditorStyle::InactiveColor;
}

FText FScriptableConditionCustomization::GetNegateText() const
{
	return LOCTEXT("NegateLabel", "NOT");
}

FText FScriptableConditionCustomization::GetNegateTooltip() const
{
	return LOCTEXT("NegateTooltip", "Negates the result of this condition.");
}

#undef LOCTEXT_NAMESPACE