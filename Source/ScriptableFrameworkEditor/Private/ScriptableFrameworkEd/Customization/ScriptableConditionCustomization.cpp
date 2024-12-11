// Copyright Kirzo. All Rights Reserved.

#include "ScriptableConditionCustomization.h"

#define LOCTEXT_NAMESPACE "FScriptableConditionCustomization"

void FScriptableConditionCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TArray<UObject*> OuterObjects;
	StructPropertyHandle->GetOuterObjects(OuterObjects);

	if (OuterObjects.Num() > 1)
	{
		return;
	}

	FScriptableObjectCustomization::CustomizeHeader(StructPropertyHandle, HeaderRow, CustomizationUtils);

	HorizontalBox->InsertSlot(1)
		.AutoWidth()
		[
			SNew(SCheckBox)
				.ToolTipText(LOCTEXT("ConditionNegateTooltip", "Negate the condition."))
				.CheckedImage(FAppStyle::GetBrush("MessageLog.Warning"))
				.CheckedHoveredImage(FAppStyle::GetBrush("MessageLog.Warning"))
				.CheckedPressedImage(FAppStyle::GetBrush("MessageLog.Warning"))
				.ForegroundColor(FColor::Red)
				.IsChecked(this, &FScriptableConditionCustomization::GetNegateCheckBoxState)
				.OnCheckStateChanged(this, &FScriptableConditionCustomization::OnNegateCheckBoxChanged)
		];
}

void FScriptableConditionCustomization::GatherChildProperties(TSharedPtr<IPropertyHandle> ChildPropertyHandle)
{
	NegatePropertyHandle = ChildPropertyHandle->GetChildHandle("bNegate");
}

ECheckBoxState FScriptableConditionCustomization::GetNegateCheckBoxState() const
{
	bool bEnabled = false;

	if (NegatePropertyHandle.IsValid())
	{
		NegatePropertyHandle->GetValue(bEnabled);
	}

	return bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FScriptableConditionCustomization::OnNegateCheckBoxChanged(ECheckBoxState NewCheckedState)
{
	const bool bEnabled = (NewCheckedState == ECheckBoxState::Checked);
	if (NegatePropertyHandle.IsValid())
	{
		NegatePropertyHandle->SetValue(bEnabled);
		NegatePropertyHandle->RequestRebuildChildren();
	}
}

#undef LOCTEXT_NAMESPACE