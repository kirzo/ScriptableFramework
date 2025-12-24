// Copyright 2025 kirzo

#include "ScriptableObjectCustomization.h"
#include "ScriptableObject.h"

#include "ScriptableFrameworkEditorHelpers.h"
#include "Widgets/SScriptableTypePicker.h"

#include "PropertyCustomizationHelpers.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "UObject/Field.h"
#include "IPropertyUtilities.h"

#define LOCTEXT_NAMESPACE "FScriptableObjectCustomization"

void FScriptableObjectCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TArray<UObject*> OuterObjects;
	InPropertyHandle->GetOuterObjects(OuterObjects);

	if (OuterObjects.Num() > 1)
	{
		return;
	}

	PropertyUtilities = CustomizationUtils.GetPropertyUtilities();
	PropertyHandle = InPropertyHandle;

	TSharedPtr<IPropertyHandle> ChildPropertyHandle = PropertyHandle->GetChildHandle(0);
	if (ChildPropertyHandle)
	{
		// Get the actual ScriptableObject
		TArray<void*> RawData;
		ChildPropertyHandle->AccessRawData(RawData);
		ScriptableObject = static_cast<UScriptableObject*>(RawData[0]);

		bIsBlueprintClass = IsValid(ScriptableObject) && ScriptableObject->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint);

		EnabledPropertyHandle = ChildPropertyHandle->GetChildHandle("bEnabled");
		GatherChildProperties(ChildPropertyHandle);
	}

	FName ClassCategory; FName PropCategory;
	ScriptableFrameworkEditor::GetScriptableCategory(GetBaseClass(), ClassCategory, PropCategory);

	HorizontalBox = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SCheckBox)
				.ToolTipText(LOCTEXT("ScriptableObjectEnabledTooltip", "Enable or disable the object."))
				.ForegroundColor(FColor::Green)
				.IsChecked(this, &FScriptableObjectCustomization::GetEnabledCheckBoxState)
				.OnCheckStateChanged(this, &FScriptableObjectCustomization::OnEnabledCheckBoxChanged)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(0.5f)
		[
			SNew(SScriptableTypePicker)
				.ClassCategoryMeta(ClassCategory)
				.FilterCategoryMeta(PropCategory)
				.Filter(PropertyHandle->GetMetaData(PropCategory))
				.BaseClass(GetBaseClass())
				.OnNodeTypePicked(SScriptableTypePicker::FOnNodeTypePicked::CreateSP(this, &FScriptableObjectCustomization::OnNodePicked))
				[
					SNew(STextBlock)
						.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
						.Text_Lambda([this]()
					{
						return ScriptableObject ? ScriptableObject->GetClass()->GetDisplayNameText() : FText::FromName(NAME_None);
					})
				]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeUseSelectedButton(FSimpleDelegate::CreateSP(this, &FScriptableObjectCustomization::OnUseSelected))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeClearButton(FSimpleDelegate::CreateSP(this, &FScriptableObjectCustomization::OnClear))
		];

	if (bIsBlueprintClass)
	{
		const FText BrowseText = FText::Format(FText::FromString("Browse to '{0}' in Content Browser"), ScriptableObject->GetClass()->GetDisplayNameText());
		HorizontalBox->InsertSlot(3)
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateSP(this, &FScriptableObjectCustomization::OnBrowseTo), BrowseText)
			];

		const FText EditText = FText::Format(FText::FromString("Edit '{0}'"), ScriptableObject->GetClass()->GetDisplayNameText());
		HorizontalBox->InsertSlot(4)
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				PropertyCustomizationHelpers::MakeEditButton(FSimpleDelegate::CreateSP(this, &FScriptableObjectCustomization::OnEdit), EditText)
			];
	}

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			HorizontalBox.ToSharedRef()
		];
}

void FScriptableObjectCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PropertyUtilities = CustomizationUtils.GetPropertyUtilities();

	uint32 NumberOfChild;
	if (InPropertyHandle->GetNumChildren(NumberOfChild) == FPropertyAccess::Success)
	{
		for (uint32 Index = 0; Index < NumberOfChild; ++Index)
		{
			TSharedRef<IPropertyHandle> CategoryPropertyHandle = InPropertyHandle->GetChildHandle(Index).ToSharedRef();

			// Don't add category rows. Only iterate over the category
			uint32 NumberOfChildrenInCategory;
			CategoryPropertyHandle->GetNumChildren(NumberOfChildrenInCategory);
			for (uint32 ChildrenInCategoryIndex = 0; ChildrenInCategoryIndex < NumberOfChildrenInCategory; ++ChildrenInCategoryIndex)
			{
				TSharedRef<IPropertyHandle> SubPropertyHandle = CategoryPropertyHandle->GetChildHandle(ChildrenInCategoryIndex).ToSharedRef();

				if (ScriptableFrameworkEditor::IsPropertyVisible(SubPropertyHandle))
				{
					ChildBuilder.AddProperty(SubPropertyHandle);
				}
			}
		}
	}
}

UClass* FScriptableObjectCustomization::GetBaseClass() const
{
	UClass* MyClass = nullptr;
	if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(PropertyHandle->GetProperty()))
	{
		MyClass = ObjectProperty->PropertyClass;
	}
	return MyClass;
}

ECheckBoxState FScriptableObjectCustomization::GetEnabledCheckBoxState() const
{
	bool bEnabled = false;

	if (EnabledPropertyHandle.IsValid())
	{
		EnabledPropertyHandle->GetValue(bEnabled);
	}

	return bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FScriptableObjectCustomization::OnEnabledCheckBoxChanged(ECheckBoxState NewCheckedState)
{
	const bool bEnabled = (NewCheckedState == ECheckBoxState::Checked);
	if (EnabledPropertyHandle.IsValid())
	{
		EnabledPropertyHandle->SetValue(bEnabled);
		EnabledPropertyHandle->RequestRebuildChildren();
	}
}

void FScriptableObjectCustomization::OnNodePicked(const UStruct* InStruct, const FAssetData& InAssetData)
{
	if (InAssetData.IsValid())
	{
		OnAssetPicked(InAssetData);
	}
	else if (InStruct)
	{
		SetScriptableObjectType(InStruct);
	}
}

void FScriptableObjectCustomization::SetScriptableObjectType(const UStruct* NewType)
{
	if (NewType->IsChildOf(GetBaseClass()))
	{
		const UClass* ConstClass = Cast<UClass>(NewType);
		UClass* Class = const_cast<UClass*>(ConstClass);

		GEditor->BeginTransaction(LOCTEXT("SetScriptableObjectType", "Set Scriptable Object Type"));

		PropertyHandle->NotifyPreChange();

		PropertyCustomizationHelpers::CreateNewInstanceOfEditInlineObjectClass(PropertyHandle.ToSharedRef(), Class, EPropertyValueSetFlags::InteractiveChange);

		PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		PropertyHandle->NotifyFinishedChangingProperties();

		GEditor->EndTransaction();

		FSlateApplication::Get().DismissAllMenus();

		if (PropertyUtilities)
		{
			PropertyUtilities->ForceRefresh();
		}
	}
}

void FScriptableObjectCustomization::OnAssetPicked(const FAssetData& AssetData)
{
	UClass* WrapperClass = nullptr;
	FName PropertyName = NAME_None;

	if (AssetData.AssetClassPath.GetAssetName() == "ScriptableTaskAsset")
	{
		WrapperClass = FindObject<UClass>(nullptr, TEXT("/Script/ScriptableFramework.ScriptableTask_RunAsset"));
		PropertyName = FName("AssetToRun");
	}
	else if (AssetData.AssetClassPath.GetAssetName() == "ScriptableConditionAsset")
	{
		WrapperClass = FindObject<UClass>(nullptr, TEXT("/Script/ScriptableFramework.ScriptableCondition_Asset"));
		PropertyName = FName("AssetToEvaluate");
	}

	if (WrapperClass)
	{
		GEditor->BeginTransaction(LOCTEXT("SetScriptableAsset", "Set Scriptable Asset"));
		PropertyHandle->NotifyPreChange();

		PropertyCustomizationHelpers::CreateNewInstanceOfEditInlineObjectClass(PropertyHandle.ToSharedRef(), WrapperClass, EPropertyValueSetFlags::InteractiveChange);

		UObject* NewObj = nullptr;
		if (PropertyHandle->GetValue(NewObj) == FPropertyAccess::Success && NewObj)
		{
			if (FObjectProperty* AssetProp = CastField<FObjectProperty>(WrapperClass->FindPropertyByName(PropertyName)))
			{
				void* ValuePtr = AssetProp->ContainerPtrToValuePtr<void>(NewObj);
				AssetProp->SetObjectPropertyValue(ValuePtr, AssetData.GetAsset());
			}
		}

		PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		PropertyHandle->NotifyFinishedChangingProperties();
		GEditor->EndTransaction();

		FSlateApplication::Get().DismissAllMenus();
		if (PropertyUtilities) PropertyUtilities->ForceRefresh();
	}
}

void FScriptableObjectCustomization::OnUseSelected()
{
	TArray<FAssetData> SelectedAssets;
	GEditor->GetContentBrowserSelections(SelectedAssets);

	for (const FAssetData& AssetData : SelectedAssets)
	{
		UBlueprint* SelectedBlueprint = Cast<UBlueprint>(AssetData.GetAsset());

		if (SelectedBlueprint)
		{
			if (SelectedBlueprint->GeneratedClass && SelectedBlueprint->GeneratedClass->IsChildOf(GetBaseClass()))
			{
				SetScriptableObjectType(SelectedBlueprint->GeneratedClass);
				return;
			}
		}
	}
}

void FScriptableObjectCustomization::OnBrowseTo()
{
	if (ScriptableObject)
	{
		TArray<FAssetData> SyncAssets;
		SyncAssets.Add(FAssetData(ScriptableObject->GetClass()));
		GEditor->SyncBrowserToObjects(SyncAssets);
	}
}

void FScriptableObjectCustomization::OnEdit()
{
	if (UBlueprint* Blueprint = Cast<UBlueprint>(ScriptableObject->GetClass()->ClassGeneratedBy))
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Blueprint);
	}
}

void FScriptableObjectCustomization::OnClear()
{
	static const FString None("None");
	PropertyHandle->SetValueFromFormattedString(None);
}

#undef LOCTEXT_NAMESPACE