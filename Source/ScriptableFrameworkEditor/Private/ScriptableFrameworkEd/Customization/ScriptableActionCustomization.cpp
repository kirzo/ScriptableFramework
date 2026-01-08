// Copyright 2026 kirzo

#include "ScriptableActionCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"

#include "ScriptableTasks/ScriptableTask.h"
#include "ScriptableTasks/ScriptableAction.h"
#include "Widgets/SScriptableTypePicker.h"
#include "ScriptableFrameworkEditorStyle.h"
#include "ScriptableFrameworkEditorHelpers.h"

UE_DISABLE_OPTIMIZATION

#define LOCTEXT_NAMESPACE "FScriptableActionCustomization"

TSharedRef<IPropertyTypeCustomization> FScriptableActionCustomization::MakeInstance()
{
	return MakeShareable(new FScriptableActionCustomization);
}

void FScriptableActionCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PropertyUtilities = CustomizationUtils.GetPropertyUtilities();

	ActionHandle = PropertyHandle;
	TasksHandle = ActionHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FScriptableAction, Tasks));
	ModeHandle = ActionHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FScriptableAction, Mode));

	FName ClassCategory; FName PropCategory;
	ScriptableFrameworkEditor::GetScriptableCategory(UScriptableTask::StaticClass(), ClassCategory, PropCategory);

	SAssignNew(AddTaskComboButton, SScriptableTypePicker)
		.HasDownArrow(false)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ToolTipText(LOCTEXT("AddTaskTooltip", "Add new Task."))
		.BaseClass(UScriptableTask::StaticClass())
		.ClassCategoryMeta(ClassCategory)
		.FilterCategoryMeta(PropCategory)
		.Filter(ActionHandle->GetMetaData(PropCategory))
		.OnNodeTypePicked(SScriptableTypePicker::FOnNodeTypePicked::CreateSP(this, &FScriptableActionCustomization::OnTaskClassPicked))
		.Content()
		[
			SNew(SImage)
				.ColorAndOpacity(FSlateColor(FScriptableFrameworkEditorStyle::ScriptableTaskColor))
				.Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
		];

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ExtensionContent()
		[
			SNew(SHorizontalBox)
				// Mode Pill
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 6, 0)
				[
					SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
						.OnClicked(this, &FScriptableActionCustomization::OnModeClicked)
						.ToolTipText(LOCTEXT("ToggleModeTooltip", "Toggle between Sequence and Parallel execution."))
						.ContentPadding(0)
						[
							SNew(SBorder)
								.Padding(FMargin(8, 1))
								.BorderImage(FScriptableFrameworkEditorStyle::Get().GetBrush("ScriptableFramework.Param.Background"))
								.BorderBackgroundColor(this, &FScriptableActionCustomization::GetModeColor)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
										.Text(this, &FScriptableActionCustomization::GetModeText)
										.TextStyle(FScriptableFrameworkEditorStyle::Get(), "ScriptableFramework.Param.Label")
										.ColorAndOpacity(FLinearColor::White)
								]
						]
				]
			// Add Task Button
			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					AddTaskComboButton.ToSharedRef()
				]
		];
}

void FScriptableActionCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	if (!TasksHandle.IsValid()) return;

	// Use Array Builder to hide the default header and list items cleanly
	TSharedRef<FDetailArrayBuilder> ArrayBuilder = MakeShareable(new FDetailArrayBuilder(TasksHandle.ToSharedRef(), false, false, true));

	ArrayBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FScriptableActionCustomization::OnGenerateElement));

	ChildBuilder.AddCustomBuilder(ArrayBuilder);
}

FText FScriptableActionCustomization::GetModeText() const
{
	uint8 Value;
	if (ModeHandle.IsValid() && ModeHandle->GetValue(Value) == FPropertyAccess::Success)
	{
		const EScriptableActionMode Mode = (EScriptableActionMode)Value;
		switch (Mode)
		{
			case EScriptableActionMode::Sequence: return LOCTEXT("ModeSequence", "Sequence");
			case EScriptableActionMode::Parallel: return LOCTEXT("ModeParallel", "Parallel");
		}
	}
	return FText::GetEmpty();
}

FSlateColor FScriptableActionCustomization::GetModeColor() const
{
	uint8 Value;
	if (ModeHandle.IsValid() && ModeHandle->GetValue(Value) == FPropertyAccess::Success)
	{
		const EScriptableActionMode Mode = (EScriptableActionMode)Value;
		// Green for Sequence, Blue/Purple for Parallel
		return Mode == EScriptableActionMode::Sequence ? FLinearColor(0.02f, 0.4f, 0.1f) : FLinearColor(0.1f, 0.2f, 0.4f);
	}
	return FSlateColor::UseForeground();
}

FReply FScriptableActionCustomization::OnModeClicked()
{
	if (ModeHandle.IsValid())
	{
		uint8 Value;
		if (ModeHandle->GetValue(Value) == FPropertyAccess::Success)
		{
			const EScriptableActionMode CurrentMode = (EScriptableActionMode)Value;
			const EScriptableActionMode NewMode = (CurrentMode == EScriptableActionMode::Sequence) ? EScriptableActionMode::Parallel : EScriptableActionMode::Sequence;

			FScopedTransaction Transaction(LOCTEXT("ToggleActionMode", "Toggle Action Mode"));
			ModeHandle->SetValue((uint8)NewMode);
		}
	}
	return FReply::Handled();
}

void FScriptableActionCustomization::OnTaskClassPicked(const UStruct* InStruct, const FAssetData& AssetData)
{
	if (AddTaskComboButton.IsValid())
	{
		AddTaskComboButton->SetIsOpen(false);
	}

	bool bNeedsRefresh = false;

	if (AssetData.IsValid())
	{
		static const FName TaskAssetClassName(TEXT("ScriptableTaskAsset"));

		// If the selected asset is a ScriptableTaskAsset, we need to wrap it.
		if (AssetData.AssetClassPath.GetAssetName() == TaskAssetClassName)
		{
			// Find the wrapper class (ScriptableTask_RunAsset)
			UClass* RunAssetClass = FindObject<UClass>(nullptr, TEXT("/Script/ScriptableFramework.ScriptableTask_RunAsset"));

			if (RunAssetClass)
			{
				// 1. Add the wrapper task to the array
				TSharedPtr<IPropertyHandle> ElementHandle = AddTask(RunAssetClass);
				bNeedsRefresh = true;

				// 2. Assign the selected asset to the 'AssetToRun' property of the wrapper
				ScriptableFrameworkEditor::SetWrapperAssetProperty(ElementHandle, AssetData.GetAsset());
			}
		}
	}
	else if (const UClass* PickedClass = Cast<UClass>(InStruct))
	{
		AddTask(PickedClass);
		bNeedsRefresh = true;
	}

	if (bNeedsRefresh && PropertyUtilities.IsValid())
	{
		PropertyUtilities->ForceRefresh();
	}
}

TSharedPtr<IPropertyHandle> FScriptableActionCustomization::AddTask(const UClass* TaskClass)
{
	if (!TasksHandle.IsValid() || !TaskClass) return nullptr;

	FScopedTransaction Transaction(LOCTEXT("AddTask", "Add Scriptable Task"));

	ActionHandle->NotifyPreChange();
	TasksHandle->NotifyPreChange();

	// 1. Add Entry to Array (returns handle to the new null entry)
	uint32 NumChildren = 0;
	TasksHandle->GetNumChildren(NumChildren);
	TasksHandle->AsArray()->AddItem();

	TSharedPtr<IPropertyHandle> NewElementHandle = TasksHandle->GetChildHandle(NumChildren);

	// 2. Instantiate the Object
	if (NewElementHandle.IsValid())
	{
		// This helper creates the NewObject using the correct Outer (the actor/asset) and assigns it.
		// It handles EditInlineNew logic automatically.
		PropertyCustomizationHelpers::CreateNewInstanceOfEditInlineObjectClass(
			NewElementHandle.ToSharedRef(),
			const_cast<UClass*>(TaskClass),
			EPropertyValueSetFlags::DefaultFlags
		);
	}

	TasksHandle->NotifyPostChange(EPropertyChangeType::ArrayAdd);
	ActionHandle->NotifyPostChange(EPropertyChangeType::ValueSet);

	return NewElementHandle;
}

void FScriptableActionCustomization::OnGenerateElement(TSharedRef<IPropertyHandle> ElementHandle, int32 Index, IDetailChildrenBuilder& Builder)
{
	IDetailPropertyRow& Row = Builder.AddProperty(ElementHandle);
	Row.ShowPropertyButtons(false);
}

#undef LOCTEXT_NAMESPACE

UE_ENABLE_OPTIMIZATION