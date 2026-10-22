// Copyright 2026 kirzo

#include "ScriptableFrameworkEd/Customization/ScriptableContainerCustomization.h"
#include "ScriptableFrameworkEditorHelpers.h"
#include "ScriptableFrameworkEditorStyle.h"
#include "Widgets/SScriptableDefinitionsEditor.h"
#include "Widgets/SScriptableTypePicker.h"

#include "ScriptableContainer.h"
#include "ScriptableObject.h"
#include "ScriptableObjectAsset.h"
#include "ScriptableRuntimeData.h"
#include "ScriptableNodes/ScriptableNode.h"
#include "ScriptableTasks/ScriptableTask_SetLocal.h"
#include "Bindings/ScriptablePropertyBindings.h"
#include "UObject/UnrealType.h"

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

#include "Engine/World.h"

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
		]
		// Context Button (Only visible if not wrapped)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.OnClicked(this, &FScriptableContainerCustomization::OnEditContextClicked)
				.ToolTipText(LOCTEXT("ContextBtnTooltip", "Edit the Context Definitions (Parameters) for this container."))
				.Visibility(this, &FScriptableContainerCustomization::GetContextButtonVisibility)
				.ContentPadding(FMargin(4.0f, 2.0f))
				[
					SNew(SBorder)
						.Padding(FMargin(6, 1))
						.BorderImage(FScriptableFrameworkEditorStyle::Get().GetBrush("ScriptableFramework.Param.Background"))
						.BorderBackgroundColor(FScriptableFrameworkEditorStyle::ContextColor)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("ContextBtnText", "Context"))
								.TextStyle(FScriptableFrameworkEditorStyle::Get(), "ScriptableFramework.Param.Label")
								.ColorAndOpacity(FLinearColor::White)
						]
				]
		]
		// Locals Button (same visibility rules as Context, opens a popup filtered to LocalsDefinitions)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.OnClicked(this, &FScriptableContainerCustomization::OnEditLocalsClicked)
				.ToolTipText(LOCTEXT("LocalsBtnTooltip", "Edit the Local Variables (mutable per-instance state) for this container."))
				.Visibility(this, &FScriptableContainerCustomization::GetLocalsButtonVisibility)
				.ContentPadding(FMargin(4.0f, 2.0f))
				[
					SNew(SBorder)
						.Padding(FMargin(6, 1))
						.BorderImage(FScriptableFrameworkEditorStyle::Get().GetBrush("ScriptableFramework.Param.Background"))
						.BorderBackgroundColor(FScriptableFrameworkEditorStyle::ContextColor)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("LocalsBtnText", "Locals"))
								.TextStyle(FScriptableFrameworkEditorStyle::Get(), "ScriptableFramework.Param.Label")
								.ColorAndOpacity(FLinearColor::White)
						]
				]
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
		.Padding(0, 0, 4, 0)
		[
			AddComboButton.ToSharedRef()
		]
	// Remove All Button
	+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &FScriptableContainerCustomization::OnClearClicked)
				.ToolTipText(GetRemoveButtonTooltip())
				.ContentPadding(FMargin(2.0f, 0.0f))
				[
					SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(FAppStyle::Get().GetBrush("Icons.Delete"))
				]
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

FReply FScriptableContainerCustomization::OnClearClicked()
{
	ClearElements();
	return FReply::Handled();
}

void FScriptableContainerCustomization::ClearElements()
{
	if (ListHandle.IsValid() && ListHandle->AsArray().IsValid())
	{
		uint32 NumElements = 0;
		ListHandle->AsArray()->GetNumElements(NumElements);

		// Only perform the transaction and empty the array if there are elements to remove
		if (NumElements > 0)
		{
			FScopedTransaction Transaction(LOCTEXT("ClearElements", "Clear Container Elements"));

			StructHandle->NotifyPreChange();

			ListHandle->AsArray()->EmptyArray();

			StructHandle->NotifyPostChange(EPropertyChangeType::ArrayClear);

			if (PropertyUtilities.IsValid())
			{
				PropertyUtilities->ForceRefresh();
			}
		}
	}
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

void FScriptableContainerCustomization::RedirectBindingsInContainer(FScriptableContainer* Container, const UScriptStruct* ConcreteStruct, const FGuid& ExpectedSourceID, FName OldName, FName NewName)
{
	if (!Container || !ConcreteStruct) return;

	const UScriptStruct* BaseContainerStruct = FScriptableContainer::StaticStruct();

	for (TFieldIterator<FArrayProperty> It(ConcreteStruct); It; ++It)
	{
		FArrayProperty* ArrayProp = *It;
		FObjectProperty* InnerObjProp = CastField<FObjectProperty>(ArrayProp->Inner);
		if (!InnerObjProp || !InnerObjProp->PropertyClass->IsChildOf(UScriptableObject::StaticClass())) continue;

		FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Container));
		for (int32 i = 0; i < Helper.Num(); ++i)
		{
			UObject* Obj = InnerObjProp->GetObjectPropertyValue(Helper.GetRawPtr(i));
			UScriptableObject* SO = Cast<UScriptableObject>(Obj);
			if (!SO) continue;

			FScriptablePropertyBindings& Bindings = SO->GetPropertyBindings();
			bool bAnyBindingChanged = false;
			for (FScriptablePropertyBinding& B : Bindings.Bindings)
			{
				if (B.SourceID != ExpectedSourceID) continue;
				TArrayView<FPropertyBindingPathSegment> Segments = B.SourcePath.GetMutableSegments();
				if (Segments.Num() > 0 && Segments[0].GetName() == OldName)
				{
					Segments[0].SetName(NewName);
					bAnyBindingChanged = true;
				}
			}
			if (bAnyBindingChanged) SO->Modify();

			// Direct VarName refs on Set Local. Locals rename only — Context rename never targets this field.
			if (ExpectedSourceID == ScriptableBindingSources::LocalsStructID)
			{
				if (UScriptableTask_SetLocal* SetLocal = Cast<UScriptableTask_SetLocal>(SO))
				{
					if (SetLocal->VarName == OldName)
					{
						SetLocal->Modify();
						SetLocal->VarName = NewName;
					}
				}
			}

			// Recurse into nested container struct members (e.g. NestedAction's Action / NestedRequirement's Requirement).
			// A nested unit that declares its own LocalsDefinitions is an isolated scope: skip it on Locals renames
			// to avoid rewriting bindings that resolve to the inner bag. Context renames always propagate.
			for (TFieldIterator<FStructProperty> StructIt(SO->GetClass()); StructIt; ++StructIt)
			{
				FStructProperty* StructProp = *StructIt;
				if (!StructProp->Struct->IsChildOf(BaseContainerStruct)) continue;

				FScriptableContainer* NestedContainer = StructProp->ContainerPtrToValuePtr<FScriptableContainer>(SO);
				if (!NestedContainer) continue;

				if (ExpectedSourceID == ScriptableBindingSources::LocalsStructID && NestedContainer->LocalsDefinitions.Num() > 0)
				{
					continue;
				}

				RedirectBindingsInContainer(NestedContainer, StructProp->Struct, ExpectedSourceID, OldName, NewName);
			}
		}
	}
}

EVisibility FScriptableContainerCustomization::GetLocalsButtonVisibility() const
{
	if (!bShowLocalsButton) return EVisibility::Collapsed;

	// Reuse the Context visibility logic: same rules apply (hide when wrapped / inside graph node / not in asset/template).
	const bool bSaved = bShowContextButton;
	const_cast<FScriptableContainerCustomization*>(this)->bShowContextButton = true;
	const EVisibility Result = GetContextButtonVisibility();
	const_cast<FScriptableContainerCustomization*>(this)->bShowContextButton = bSaved;
	return Result;
}

EVisibility FScriptableContainerCustomization::GetContextButtonVisibility() const
{
	// Explicit opt-out from the wrapping customization (e.g. NestedRequirement / NestedAction).
	if (!bShowContextButton)
	{
		return EVisibility::Collapsed;
	}

	// Hide button if wrapped or if multiple items are selected
	if (StructHandle->GetNumPerObjectValues() > 1)
	{
		return EVisibility::Collapsed;
	}

	TArray<UObject*> OuterObjects;
	StructHandle->GetOuterObjects(OuterObjects);

	for (UObject* OuterObj : OuterObjects)
	{
		bool bIsInsideAssetOrTemplate = false;
		bool bIsInsideGraphNode = false;
		UObject* CurrentOuter = OuterObj;

		// Traverse the ownership chain upwards until we hit the package
		while (CurrentOuter && !CurrentOuter->IsA<UPackage>())
		{
			// A UWorld is technically an asset (.umap). We must intercept Levels and Worlds 
			// before the IsAsset() check to correctly identify scene instances.
			if (CurrentOuter->IsA<ULevel>() || CurrentOuter->IsA<UWorld>())
			{
				bIsInsideAssetOrTemplate = false;
				break;
			}

			// A UScriptableNode anywhere in the chain means 
			// his container is nested inside a graph node
			if (CurrentOuter->IsA<UScriptableNode>())
			{
				bIsInsideGraphNode = true;
				break;
			}

			// If the current object or any of its parents is an Asset, CDO, or Archetype, we are safe
			if (CurrentOuter->IsTemplate() || CurrentOuter->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) || CurrentOuter->IsAsset())
			{
				bIsInsideAssetOrTemplate = true;
				break;
			}

			// Move up to the next parent
			CurrentOuter = CurrentOuter->GetOuter();
		}

		// If we hit a Level/World, or reached the root package without finding an Asset/CDO
		if (!bIsInsideAssetOrTemplate)
		{
			return EVisibility::Collapsed;
		}

		if (bIsInsideGraphNode)
		{
			return EVisibility::Collapsed;
		}
	}

	return EVisibility::Visible;
}

FReply FScriptableContainerCustomization::OnEditContextClicked()
{
	if (StructHandle.IsValid())
	{
		SScriptableDefinitionsEditor::Open(StructHandle.ToSharedRef(), SScriptableDefinitionsEditor::ETab::Context);
	}
	return FReply::Handled();
}

FReply FScriptableContainerCustomization::OnEditLocalsClicked()
{
	if (StructHandle.IsValid())
	{
		SScriptableDefinitionsEditor::Open(StructHandle.ToSharedRef(), SScriptableDefinitionsEditor::ETab::Locals);
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

	uint32 NumChildren = 0;
	ListHandle->GetNumChildren(NumChildren);
	ListHandle->AsArray()->AddItem();

	TSharedPtr<IPropertyHandle> NewElementHandle = ListHandle->GetChildHandle(NumChildren);

	if (NewElementHandle.IsValid())
	{
		StructHandle->NotifyPreChange();

		PropertyCustomizationHelpers::CreateNewInstanceOfEditInlineObjectClass(
			NewElementHandle.ToSharedRef(),
			const_cast<UClass*>(ElementClass),
			EPropertyValueSetFlags::DefaultFlags
		);

	  StructHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	}

	return NewElementHandle;
}

#undef LOCTEXT_NAMESPACE