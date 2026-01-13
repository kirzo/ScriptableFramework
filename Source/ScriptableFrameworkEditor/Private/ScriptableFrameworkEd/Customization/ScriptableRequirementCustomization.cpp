// Copyright 2026 kirzo

#include "ScriptableRequirementCustomization.h"
#include "ScriptableConditions/ScriptableCondition.h"
#include "ScriptableConditions/ScriptableRequirement.h"
#include "ScriptableConditions/ScriptableRequirementAsset.h"
#include "ScriptableFrameworkEditorStyle.h"

#define LOCTEXT_NAMESPACE "FScriptableRequirementCustomization"

TSharedPtr<SHorizontalBox> FScriptableRequirementCustomization::GetHeaderValueContent()
{
	TSharedPtr<SHorizontalBox> Box = FScriptableContainerCustomization::GetHeaderValueContent();

	Box->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 6, 0)
		[
			SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.OnClicked(this, &FScriptableRequirementCustomization::OnNegateClicked)
				.ToolTipText(GetNegateTooltip())
				.ContentPadding(0)
				[
					SNew(SBorder)
						.Padding(FMargin(6, 1))
						.BorderImage(FScriptableFrameworkEditorStyle::Get().GetBrush("ScriptableFramework.Param.Background"))
						.BorderBackgroundColor(this, &FScriptableRequirementCustomization::GetNegateColor)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text(this, &FScriptableRequirementCustomization::GetNegateText)
								.TextStyle(FScriptableFrameworkEditorStyle::Get(), "ScriptableFramework.Param.Label")
								.ColorAndOpacity(FLinearColor::White)
						]
				]
		];

	return Box;
}

UClass* FScriptableRequirementCustomization::GetBaseClass() const
{
	return UScriptableCondition::StaticClass();
}

FName FScriptableRequirementCustomization::GetListPropertyName() const
{
	return GET_MEMBER_NAME_CHECKED(FScriptableRequirement, Conditions);
}

FName FScriptableRequirementCustomization::GetModePropertyName() const
{
	return GET_MEMBER_NAME_CHECKED(FScriptableRequirement, Mode);
}

FSlateColor FScriptableRequirementCustomization::GetIconColor() const
{
	return FScriptableFrameworkEditorStyle::ScriptableConditionColor;
}

FText FScriptableRequirementCustomization::GetAddButtonTooltip() const
{
	return LOCTEXT("AddConditionTooltip", "Add new Condition.");
}

UClass* FScriptableRequirementCustomization::GetWrapperClass() const
{
	return UScriptableCondition_Asset::StaticClass();
}

FText FScriptableRequirementCustomization::GetModeText() const
{
	uint8 Value;
	if (ModeHandle.IsValid() && ModeHandle->GetValue(Value) == FPropertyAccess::Success)
	{
		const EScriptableRequirementMode Mode = (EScriptableRequirementMode)Value;
		switch (Mode)
		{
			case EScriptableRequirementMode::And: return LOCTEXT("ModeAnd", "AND");
			case EScriptableRequirementMode::Or: return LOCTEXT("ModeOr", "OR");
		}
	}
	return FText::GetEmpty();
}

FSlateColor FScriptableRequirementCustomization::GetModeColor() const
{
	uint8 Value;
	if (ModeHandle.IsValid() && ModeHandle->GetValue(Value) == FPropertyAccess::Success)
	{
		const EScriptableRequirementMode Mode = (EScriptableRequirementMode)Value;
		// Green for AND, Dark Orange for OR
		return Mode == EScriptableRequirementMode::And ? FLinearColor(0.02f, 0.4f, 0.1f) : FLinearColor(0.6f, 0.3f, 0.0f);
	}
	return FSlateColor::UseForeground();
}

FText FScriptableRequirementCustomization::GetModeTooltip() const
{
	return LOCTEXT("ToggleModeTooltip", "Toggle between AND (All) and OR (Any).");
}

// --- Negate Logic ---

bool FScriptableRequirementCustomization::GetNegateValue() const
{
	if (StructHandle.IsValid())
	{
		if (TSharedPtr<IPropertyHandle> Prop = StructHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FScriptableRequirement, bNegate)))
		{
			bool bVal = false;
			Prop->GetValue(bVal);
			return bVal;
		}
	}
	return false;
}

FReply FScriptableRequirementCustomization::OnNegateClicked()
{
	if (StructHandle.IsValid())
	{
		if (TSharedPtr<IPropertyHandle> Prop = StructHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FScriptableRequirement, bNegate)))
		{
			bool bNewVal = !GetNegateValue();
			FScopedTransaction Transaction(LOCTEXT("ToggleNegate", "Toggle Requirement Negation"));
			Prop->SetValue(bNewVal);
		}
	}
	return FReply::Handled();
}

FSlateColor FScriptableRequirementCustomization::GetNegateColor() const
{
	return GetNegateValue() ? FScriptableFrameworkEditorStyle::NegateColor : FScriptableFrameworkEditorStyle::InactiveColor;
}

FText FScriptableRequirementCustomization::GetNegateText() const
{
	return LOCTEXT("NegateLabel", "NOT");
}

FText FScriptableRequirementCustomization::GetNegateTooltip() const
{
	return LOCTEXT("NegateTooltip", "Inverts the result of this requirement.");
}

#undef LOCTEXT_NAMESPACE