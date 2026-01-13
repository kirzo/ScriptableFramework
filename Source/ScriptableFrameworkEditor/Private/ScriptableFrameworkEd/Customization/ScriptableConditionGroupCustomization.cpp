// Copyright 2026 kirzo

#include "ScriptableConditionGroupCustomization.h"
#include "ScriptableRequirementCustomization.h"

#include "ScriptableConditions/ScriptableCondition_Group.h"
#include "ScriptableConditions/ScriptableCondition.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

#define LOCTEXT_NAMESPACE "FScriptableConditionGroupCustomization"

void FScriptableConditionGroupCustomization::InitCustomization(TSharedRef<IPropertyHandle> InPropertyHandle, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	FScriptableConditionCustomization::InitCustomization(InPropertyHandle, CustomizationUtils);

	RequirementCustomization = MakeShared<FScriptableRequirementCustomization>();

	TSharedPtr<IPropertyHandle> ReqHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(UScriptableCondition_Group, Requirement));
	if (ReqHandle.IsValid())
	{
		RequirementCustomization->InitCustomization(ReqHandle.ToSharedRef(), CustomizationUtils);
	}
}

TSharedPtr<SHorizontalBox> FScriptableConditionGroupCustomization::GetHeaderNameContent()
{
	TSharedPtr<SHorizontalBox> NameBox = FScriptableConditionCustomization::GetHeaderNameContent();
	// Remove default name
	if (NameBox.IsValid() && NameBox->IsValidSlotIndex(1))
	{
		NameBox->RemoveSlot(NameBox->GetSlot(1).GetWidget());
	}

	NameBox->AddSlot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SInlineEditableTextBlock)
				.Text(this, &FScriptableConditionGroupCustomization::GetGroupName)
				.OnTextCommitted(this, &FScriptableConditionGroupCustomization::OnGroupNameCommitted)
				.OnVerifyTextChanged(this, &FScriptableConditionGroupCustomization::VerifyGroupNameChanged)
				.Font(IDetailLayoutBuilder::GetDetailFontBold())
				.ToolTipText(LOCTEXT("RenameTooltip", "Click to rename this group."))
				.ColorAndOpacity(ScriptableObject.IsValid() ? FSlateColor::UseForeground() : FSlateColor::UseSubduedForeground())
		];

	return NameBox;
}

TSharedPtr<SHorizontalBox> FScriptableConditionGroupCustomization::GetHeaderExtensionContent()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			RequirementCustomization->GetHeaderExtensionContent().ToSharedRef()
		]

		// Spacer
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(8, 0, 0, 0)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			FScriptableConditionCustomization::GetHeaderExtensionContent().ToSharedRef()
		];
}

void FScriptableConditionGroupCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	if (RequirementCustomization.IsValid())
	{
		TSharedPtr<IPropertyHandle> ReqHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(UScriptableCondition_Group, Requirement));
		if (ReqHandle.IsValid())
		{
			RequirementCustomization->CustomizeChildren(ReqHandle.ToSharedRef(), ChildBuilder, CustomizationUtils);
		}
	}
}

FText FScriptableConditionGroupCustomization::GetGroupName() const
{
	if (PropertyHandle.IsValid())
	{
		TSharedPtr<IPropertyHandle> NameHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(UScriptableCondition_Group, GroupName));
		if (NameHandle.IsValid())
		{
			FString Val;
			NameHandle->GetValue(Val);

			if (!Val.IsEmpty())
			{
				return FText::FromString(Val);
			}
		}
	}

	return LOCTEXT("DefaultGroupName", "Nested Group");
}

void FScriptableConditionGroupCustomization::OnGroupNameCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (PropertyHandle.IsValid())
	{
		TSharedPtr<IPropertyHandle> NameHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(UScriptableCondition_Group, GroupName));
		if (NameHandle.IsValid())
		{
			FString CurrentVal;
			NameHandle->GetValue(CurrentVal);

			FString NewString = NewText.ToString();

			if (NewString.Equals(LOCTEXT("DefaultGroupName", "Nested Group").ToString(), ESearchCase::IgnoreCase))
			{
				NewString.Empty();
			}

			if (!CurrentVal.Equals(NewString))
			{
				FScopedTransaction Transaction(LOCTEXT("RenameGroup", "Rename Group"));
				NameHandle->SetValue(NewString);
			}
		}
	}
}

bool FScriptableConditionGroupCustomization::VerifyGroupNameChanged(const FText& NewText, FText& OutErrorMessage)
{
	return true;
}

#undef LOCTEXT_NAMESPACE