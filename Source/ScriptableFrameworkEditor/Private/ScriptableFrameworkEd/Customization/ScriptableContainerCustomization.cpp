// Copyright 2026 kirzo

#include "ScriptableFrameworkEd/Customization/ScriptableContainerCustomization.h"
#include "ScriptableFrameworkEditorHelpers.h"
#include "ScriptableFrameworkEditorStyle.h"
#include "Widgets/SScriptableTypePicker.h"

#include "ScriptableObjectAsset.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "FScriptableContainerCustomization"

void FScriptableContainerCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	InitCustomization(PropertyHandle, CustomizationUtils);

	HeaderRow
		.NameContent()
		[
			GetHeaderNameContent().ToSharedRef()
		]
		.ValueContent()
		[
			GetHeaderValueContent().ToSharedRef()
		]
		.ExtensionContent()
		[
			GetHeaderExtensionContent().ToSharedRef()
		];
}

void FScriptableContainerCustomization::InitCustomization(TSharedRef<IPropertyHandle> InStructHandle, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	StructHandle = InStructHandle;
	PropertyUtilities = CustomizationUtils.GetPropertyUtilities();

	ListHandle = StructHandle->GetChildHandle(GetListPropertyName());
	ModeHandle = StructHandle->GetChildHandle(GetModePropertyName());

	if (!AddComboButton.IsValid())
	{
		FName ClassCategory; FName PropCategory;
		ScriptableFrameworkEditor::GetScriptableCategory(GetBaseClass(), ClassCategory, PropCategory);

		SAssignNew(AddComboButton, SScriptableTypePicker)
			.HasDownArrow(false)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ToolTipText(GetAddButtonTooltip())
			.BaseClass(GetBaseClass())
			.ClassCategoryMeta(ClassCategory)
			.FilterCategoryMeta(PropCategory)
			.Filter(ScriptableFrameworkEditor::GetPropertyMetaData(StructHandle, PropCategory))
			.OnNodeTypePicked(SScriptableTypePicker::FOnNodeTypePicked::CreateSP(this, &FScriptableContainerCustomization::OnTypePicked))
			.Content()
			[
				SNew(SImage)
					.ColorAndOpacity(GetIconColor())
					.Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
			];
	}
}

TSharedPtr<SHorizontalBox> FScriptableContainerCustomization::GetHeaderNameContent()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			StructHandle->CreatePropertyNameWidget()
		];
}

TSharedPtr<SHorizontalBox> FScriptableContainerCustomization::GetHeaderValueContent()
{
	return SNew(SHorizontalBox);
}

TSharedPtr<SHorizontalBox> FScriptableContainerCustomization::GetHeaderExtensionContent()
{
	return SNew(SHorizontalBox)
		// Mode Pill
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 6, 0)
		[
			SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.OnClicked(this, &FScriptableContainerCustomization::OnModeClicked)
				.ToolTipText(this, &FScriptableContainerCustomization::GetModeTooltip)
				.ContentPadding(0)
				[
					SNew(SBorder)
						.Padding(FMargin(8, 1))
						.BorderImage(FScriptableFrameworkEditorStyle::Get().GetBrush("ScriptableFramework.Param.Background"))
						.BorderBackgroundColor(this, &FScriptableContainerCustomization::GetModeColor)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text(this, &FScriptableContainerCustomization::GetModeText)
								.TextStyle(FScriptableFrameworkEditorStyle::Get(), "ScriptableFramework.Param.Label")
								.ColorAndOpacity(FLinearColor::White)
						]
				]
		]
	// Add Button
	+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			AddComboButton.ToSharedRef()
		];
}

void FScriptableContainerCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	if (!ListHandle.IsValid()) return;

	TSharedRef<FDetailArrayBuilder> ArrayBuilder = MakeShareable(new FDetailArrayBuilder(ListHandle.ToSharedRef(), false, false, true));
	ArrayBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FScriptableContainerCustomization::OnGenerateElement));

	ChildBuilder.AddCustomBuilder(ArrayBuilder);
}

void FScriptableContainerCustomization::OnGenerateElement(TSharedRef<IPropertyHandle> ElementHandle, int32 Index, IDetailChildrenBuilder& Builder)
{
	IDetailPropertyRow& Row = Builder.AddProperty(ElementHandle);
	Row.ShowPropertyButtons(false);
}

FReply FScriptableContainerCustomization::OnModeClicked()
{
	if (ModeHandle.IsValid())
	{
		uint8 Value;
		if (ModeHandle->GetValue(Value) == FPropertyAccess::Success)
		{
			// Generic toggle: 0 -> 1, 1 -> 0
			// Assumes enums have 2 values (AND/OR, Sequence/Parallel)
			const uint8 NewValue = (Value == 0) ? 1 : 0;

			FScopedTransaction Transaction(LOCTEXT("ToggleMode", "Toggle Mode"));
			ModeHandle->SetValue(NewValue);
		}
	}
	return FReply::Handled();
}

void FScriptableContainerCustomization::OnTypePicked(const UStruct* InStruct, const FAssetData& AssetData)
{
	if (AddComboButton.IsValid())
	{
		AddComboButton->SetIsOpen(false);
	}

	bool bNeedsRefresh = false;

	if (AssetData.IsValid())
	{
		OnAssetPicked(AssetData);
		bNeedsRefresh = true;
	}
	else if (const UClass* PickedClass = Cast<UClass>(InStruct))
	{
		AddElement(PickedClass);
		bNeedsRefresh = true;
	}

	if (bNeedsRefresh && PropertyUtilities.IsValid())
	{
		PropertyUtilities->ForceRefresh();
	}
}

void FScriptableContainerCustomization::OnAssetPicked(const FAssetData& AssetData)
{
	if (AssetData.IsInstanceOf<UScriptableObjectAsset>())
	{
		UClass* WrapperClass = GetWrapperClass();
		if (WrapperClass)
		{
			TSharedPtr<IPropertyHandle> ElementHandle = AddElement(WrapperClass);
			ScriptableFrameworkEditor::SetWrapperAssetProperty(ElementHandle, AssetData.GetAsset());
		}
	}
}

TSharedPtr<IPropertyHandle> FScriptableContainerCustomization::AddElement(const UClass* ElementClass)
{
	if (!ListHandle.IsValid() || !ElementClass) return nullptr;

	FScopedTransaction Transaction(LOCTEXT("AddElement", "Add Element"));

	StructHandle->NotifyPreChange();
	ListHandle->NotifyPreChange();

	uint32 NumChildren = 0;
	ListHandle->GetNumChildren(NumChildren);
	ListHandle->AsArray()->AddItem();

	TSharedPtr<IPropertyHandle> NewElementHandle = ListHandle->GetChildHandle(NumChildren);

	if (NewElementHandle.IsValid())
	{
		PropertyCustomizationHelpers::CreateNewInstanceOfEditInlineObjectClass(
			NewElementHandle.ToSharedRef(),
			const_cast<UClass*>(ElementClass),
			EPropertyValueSetFlags::DefaultFlags
		);
	}

	ListHandle->NotifyPostChange(EPropertyChangeType::ArrayAdd);
	StructHandle->NotifyPostChange(EPropertyChangeType::ValueSet);

	return NewElementHandle;
}

#undef LOCTEXT_NAMESPACE