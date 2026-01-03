// Copyright 2026 kirzo

#include "ScriptableFrameworkEd/Customization/ScriptableObjectCustomization.h"
#include "ScriptableObject.h"
#include "Bindings/ScriptablePropertyBindings.h"
#include "PropertyBindingPath.h"

#include "ScriptableFrameworkEditorHelpers.h"
#include "Widgets/SScriptableTypePicker.h"

#include "PropertyCustomizationHelpers.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "UObject/Field.h"
#include "IPropertyUtilities.h"
#include "IPropertyAccessEditor.h"
#include "Features/IModularFeatures.h"
#include "Styling/AppStyle.h"
#include "ScopedTransaction.h"
#include "EdGraphSchema_K2.h"
#include "StructUtils/InstancedStruct.h"

#define LOCTEXT_NAMESPACE "FScriptableObjectCustomization"

// ------------------------------------------------------------------------------------------------
// Helper Namespace for Binding Logic
// ------------------------------------------------------------------------------------------------
namespace ScriptableBindingHelpers
{
	FGuid GetScriptableObjectDataID(UScriptableObject* Owner)
	{
		if (Owner)
		{
			const uint32 Hash = GetTypeHash(Owner->GetPathName());
			return FGuid(Hash, Hash, Hash, Hash);
		}
		return FGuid();
	}

	void MakeStructPropertyPathFromPropertyHandle(UScriptableObject* ScriptableObject, TSharedPtr<const IPropertyHandle> InPropertyHandle, FPropertyBindingPath& OutPath)
	{
		OutPath.Reset();

		if (!ScriptableObject)
		{
			return;
		}

		TArray<FPropertyBindingPathSegment> PathSegments;
		TSharedPtr<const IPropertyHandle> CurrentPropertyHandle = InPropertyHandle;

		while (CurrentPropertyHandle.IsValid())
		{
			const FProperty* Property = CurrentPropertyHandle->GetProperty();
			if (Property)
			{
				// Determine if we went too far up the hierarchy (e.g. into the owning Actor)
				if (const UClass* PropertyOwnerClass = Cast<UClass>(Property->GetOwnerStruct()))
				{
					if (!ScriptableObject->GetClass()->IsChildOf(PropertyOwnerClass))
					{
						break;
					}
				}

				FPropertyBindingPathSegment& Segment = PathSegments.InsertDefaulted_GetRef(0);
				Segment.SetName(Property->GetFName());
				Segment.SetArrayIndex(CurrentPropertyHandle->GetIndexInArray());

				if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
				{
					if (ObjectProperty->HasAnyPropertyFlags(CPF_PersistentInstance | CPF_InstancedReference))
					{
						const UObject* Object = nullptr;
						if (CurrentPropertyHandle->GetValue(Object) == FPropertyAccess::Success && Object)
						{
							Segment.SetInstanceStruct(Object->GetClass());
						}
					}
				}
				else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					if (StructProperty->Struct == TBaseStructure<FInstancedStruct>::Get())
					{
						void* Address = nullptr;
						if (CurrentPropertyHandle->GetValueData(Address) == FPropertyAccess::Success && Address)
						{
							const FInstancedStruct& Struct = *static_cast<const FInstancedStruct*>(Address);
							Segment.SetInstanceStruct(Struct.GetScriptStruct());
						}
					}
				}

				if (Segment.GetArrayIndex() != INDEX_NONE)
				{
					TSharedPtr<const IPropertyHandle> ParentPropertyHandle = CurrentPropertyHandle->GetParentHandle();
					if (ParentPropertyHandle.IsValid())
					{
						const FProperty* ParentProperty = ParentPropertyHandle->GetProperty();
						if (ParentProperty && ParentProperty->IsA<FArrayProperty>() && Property->GetFName() == ParentProperty->GetFName())
						{
							CurrentPropertyHandle = ParentPropertyHandle;
						}
					}
				}
			}
			CurrentPropertyHandle = CurrentPropertyHandle->GetParentHandle();
		}

		if (PathSegments.Num() > 0)
		{
			FGuid OwnerID = GetScriptableObjectDataID(ScriptableObject);
			OutPath = FPropertyBindingPath(OwnerID, PathSegments);
		}
	}

	void MakeStructPropertyPathFromBindingChain(const FGuid& StructID, const TArray<FBindingChainElement>& InBindingChain, FPropertyBindingPath& OutPath)
	{
		OutPath.Reset();
		OutPath.SetStructID(StructID);

		for (const FBindingChainElement& Element : InBindingChain)
		{
			if (const FProperty* Property = Element.Field.Get<FProperty>())
			{
				OutPath.AddPathSegment(Property->GetFName(), Element.ArrayIndex);
			}
		}
	}

	struct FCachedBindingData : public TSharedFromThis<FCachedBindingData>
	{
		TWeakObjectPtr<UScriptableObject> WeakScriptableObject;
		FPropertyBindingPath TargetPath;
		TSharedPtr<const IPropertyHandle> PropertyHandle;
		TArray<FBindableStructDesc> AccessibleStructs;

		FText Text;
		FText TooltipText;
		FLinearColor Color = FLinearColor::White;
		const FSlateBrush* Image = nullptr;
		bool bIsDataCached = false;

		FCachedBindingData(UScriptableObject* InObject, const FPropertyBindingPath& InTargetPath, const TSharedPtr<const IPropertyHandle>& InHandle, const TArray<FBindableStructDesc>& InStructs)
			: WeakScriptableObject(InObject)
			, TargetPath(InTargetPath)
			, PropertyHandle(InHandle)
			, AccessibleStructs(InStructs)
		{
		}

		void UpdateData()
		{
			static const FName PropertyIcon(TEXT("Kismet.Tabs.Variables"));
			Text = FText::GetEmpty();
			TooltipText = FText::GetEmpty();
			Color = FLinearColor::White;
			Image = nullptr;

			if (!PropertyHandle.IsValid()) return;
			UScriptableObject* ScriptableObject = WeakScriptableObject.Get();
			if (!ScriptableObject) return;

			const FScriptablePropertyBindings& Bindings = ScriptableObject->GetPropertyBindings();

			const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
			FEdGraphPinType PinType;
			Schema->ConvertPropertyToPinType(PropertyHandle->GetProperty(), PinType);

			if (Bindings.HasPropertyBinding(TargetPath))
			{
				const FPropertyBindingPath* SourcePath = Bindings.GetPropertyBinding(TargetPath);
				if (SourcePath)
				{
					const FBindableStructDesc* SourceDesc = AccessibleStructs.FindByPredicate([&](const FBindableStructDesc& Desc) { return Desc.ID == SourcePath->GetStructID(); });

					FString DisplayString = SourceDesc ? SourceDesc->Name.ToString() : TEXT("Unknown");
					if (!SourcePath->IsPathEmpty())
					{
						DisplayString += TEXT(".") + SourcePath->ToString();
					}

					Text = FText::FromString(DisplayString);
					TooltipText = FText::Format(LOCTEXT("BindingTooltip", "Bound to {0}"), Text);
					Image = FAppStyle::GetBrush(PropertyIcon);
					Color = Schema->GetPinTypeColor(PinType);
				}
			}
			else
			{
				Text = LOCTEXT("Bind", "Bind");
				TooltipText = LOCTEXT("BindActionTooltip", "Bind this property to a value from the Context.");
				Image = FAppStyle::GetBrush(PropertyIcon);
				Color = FLinearColor::White;
			}

			bIsDataCached = true;
		}

		void AddBinding(const TArray<FBindingChainElement>& InBindingChain)
		{
			if (InBindingChain.IsEmpty()) return;

			UScriptableObject* ScriptableObject = WeakScriptableObject.Get();
			if (!ScriptableObject) return;

			const int32 ContextIndex = InBindingChain[0].ArrayIndex;
			if (!AccessibleStructs.IsValidIndex(ContextIndex)) return;

			FScopedTransaction Transaction(LOCTEXT("AddBinding", "Add Property Binding"));
			ScriptableObject->Modify();

			const FBindableStructDesc& SelectedContext = AccessibleStructs[ContextIndex];

			TArray<FBindingChainElement> PropertyChain = InBindingChain;
			PropertyChain.RemoveAt(0);

			FPropertyBindingPath SourcePath;
			MakeStructPropertyPathFromBindingChain(SelectedContext.ID, PropertyChain, SourcePath);

			ScriptableObject->GetPropertyBindings().AddPropertyBinding(SourcePath, TargetPath);
			UpdateData();
		}

		void RemoveBinding()
		{
			UScriptableObject* ScriptableObject = WeakScriptableObject.Get();
			if (!ScriptableObject) return;

			FScopedTransaction Transaction(LOCTEXT("RemoveBinding", "Remove Property Binding"));
			ScriptableObject->Modify();

			ScriptableObject->GetPropertyBindings().RemovePropertyBindings(TargetPath);
			UpdateData();
		}

		bool CanRemoveBinding() const
		{
			if (UScriptableObject* ScriptableObject = WeakScriptableObject.Get())
			{
				return ScriptableObject->GetPropertyBindings().HasPropertyBinding(TargetPath);
			}
			return false;
		}

		FText GetText() { if (!bIsDataCached) UpdateData(); return Text; }
		FText GetTooltipText() { if (!bIsDataCached) UpdateData(); return TooltipText; }
		FLinearColor GetColor() { if (!bIsDataCached) UpdateData(); return Color; }
		const FSlateBrush* GetImage() { if (!bIsDataCached) UpdateData(); return Image; }
	};
} // End Namespace

// ------------------------------------------------------------------------------------------------
// FScriptableObjectCustomization
// ------------------------------------------------------------------------------------------------

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
						if (ScriptableObject)
						{
							static const FName CandidateProperties[] = { FName("AssetToRun"), FName("AssetToEvaluate") };

							for (const FName& PropName : CandidateProperties)
							{
								if (FObjectProperty* AssetProp = CastField<FObjectProperty>(ScriptableObject->GetClass()->FindPropertyByName(PropName)))
								{
									if (UObject* AssignedAsset = AssetProp->GetObjectPropertyValue_InContainer(ScriptableObject))
									{
										return FText::FromString(AssignedAsset->GetName());
									}

									return ScriptableObject->GetClass()->GetDisplayNameText();
								}
							}

							return ScriptableObject->GetClass()->GetDisplayNameText();
						}

						return FText::FromName(NAME_None);
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

			uint32 NumberOfChildrenInCategory;
			CategoryPropertyHandle->GetNumChildren(NumberOfChildrenInCategory);

			for (uint32 ChildrenInCategoryIndex = 0; ChildrenInCategoryIndex < NumberOfChildrenInCategory; ++ChildrenInCategoryIndex)
			{
				TSharedRef<IPropertyHandle> SubPropertyHandle = CategoryPropertyHandle->GetChildHandle(ChildrenInCategoryIndex).ToSharedRef();

				if (ScriptableFrameworkEditor::IsPropertyVisible(SubPropertyHandle))
				{
					IDetailPropertyRow& Row = ChildBuilder.AddProperty(SubPropertyHandle);

					// Check if we should add the Binding Widget
					if (ScriptableObject && IsPropertyExtendable(SubPropertyHandle))
					{
						TSharedPtr<SWidget> BindingWidget = GenerateBindingWidget(ScriptableObject, SubPropertyHandle);

						if (BindingWidget.IsValid() && BindingWidget != SNullWidget::NullWidget)
						{
							// Logic to disable the property value if it is bound
							auto IsPropertyEnabled = [this, SubPropertyHandle]() -> bool
							{
								if (!ScriptableObject) return true;

								// Re-calculate path to check against bindings
								// This is fast enough for Editor UI
								FPropertyBindingPath TargetPath;
								ScriptableBindingHelpers::MakeStructPropertyPathFromPropertyHandle(ScriptableObject, SubPropertyHandle, TargetPath);

								// If it HAS a binding, return FALSE (Disabled/Read-only)
								return !ScriptableObject->GetPropertyBindings().HasPropertyBinding(TargetPath);
							};

							Row.CustomWidget()
								.NameContent()
								[
									SubPropertyHandle->CreatePropertyNameWidget()
								]
								.ValueContent()
								[
									// Wrap the Value Widget in a Box to control its Enabled state
									SNew(SBox)
										.IsEnabled_Lambda(IsPropertyEnabled)
										[
											SubPropertyHandle->CreatePropertyValueWidget()
										]
								]
							.ExtensionContent()
								[
									SNew(SHorizontalBox)
										+ SHorizontalBox::Slot()
										.AutoWidth()
										.VAlign(VAlign_Center)
										.Padding(2.0f, 0.0f)
										[
											BindingWidget.ToSharedRef()
										]
										+ SHorizontalBox::Slot()
										.AutoWidth()
										.VAlign(VAlign_Center)
										[
											SubPropertyHandle->CreateDefaultPropertyButtonWidgets()
										]
								];
						}
					}
				}
			}
		}
	}
}

bool FScriptableObjectCustomization::IsPropertyExtendable(TSharedPtr<IPropertyHandle> InPropertyHandle) const
{
	const FProperty* Property = InPropertyHandle->GetProperty();
	if (!Property) return false;

	// Filter out internal/system properties
	if (Property->HasMetaData(TEXT("NoBinding")))
	{
		return false;
	}

	if (Property->HasAnyPropertyFlags(CPF_PersistentInstance | CPF_EditorOnly | CPF_Config | CPF_Deprecated))
	{
		return false;
	}

	return true;
}

TSharedPtr<SWidget> FScriptableObjectCustomization::GenerateBindingWidget(UScriptableObject* InScriptableObject, TSharedPtr<IPropertyHandle> InPropertyHandle)
{
	if (!IModularFeatures::Get().IsModularFeatureAvailable("PropertyAccessEditor"))
	{
		return SNullWidget::NullWidget;
	}

	// Prepare Contexts
	TArray<FBindableStructDesc> AccessibleStructs;
	InScriptableObject->GetAccessibleStructs(InScriptableObject, AccessibleStructs);

	TArray<FBindingContextStruct> Contexts;
	for (const FBindableStructDesc& Desc : AccessibleStructs)
	{
		if (Desc.IsValid())
		{
			FBindingContextStruct& ContextStruct = Contexts.AddDefaulted_GetRef();
			ContextStruct.DisplayText = FText::FromName(Desc.Name);
			ContextStruct.Struct = const_cast<UStruct*>(Desc.Struct.Get());
		}
	}

	// Prepare Target Path
	FPropertyBindingPath TargetPath;
	ScriptableBindingHelpers::MakeStructPropertyPathFromPropertyHandle(InScriptableObject, InPropertyHandle, TargetPath);

	// Create UI Cache
	TSharedPtr<ScriptableBindingHelpers::FCachedBindingData> CachedData =
		MakeShared<ScriptableBindingHelpers::FCachedBindingData>(InScriptableObject, TargetPath, InPropertyHandle, AccessibleStructs);

	// Setup Args
	FPropertyBindingWidgetArgs Args;
	Args.Property = InPropertyHandle->GetProperty();
	Args.bAllowPropertyBindings = true;
	Args.bGeneratePureBindings = true;
	Args.bAllowStructMemberBindings = true;
	Args.bAllowUObjectFunctions = true;

	Args.OnCanBindToClass = FOnCanBindToClass::CreateLambda([](UClass* InClass) { return true; });

	Args.OnCanBindToContextStructWithIndex = FOnCanBindToContextStructWithIndex::CreateLambda([InPropertyHandle, AccessibleStructs](const UStruct* InStruct, int32 Index)
	{
		if (const FStructProperty* StructProp = CastField<FStructProperty>(InPropertyHandle->GetProperty()))
		{
			return StructProp->Struct == InStruct;
		}
		return false;
	});

	Args.OnCanBindPropertyWithBindingChain = FOnCanBindPropertyWithBindingChain::CreateLambda([InPropertyHandle](FProperty* InProperty, TArrayView<const FBindingChainElement> BindingChain)
	{
		return FScriptablePropertyBindings::ArePropertiesCompatible(InProperty, InPropertyHandle->GetProperty());
	});

	Args.OnCanAcceptPropertyOrChildrenWithBindingChain = FOnCanAcceptPropertyOrChildrenWithBindingChain::CreateLambda([](FProperty* InProperty, TArrayView<const FBindingChainElement>)
	{
		return true;
	});

	Args.OnAddBinding = FOnAddBinding::CreateLambda([CachedData](FName InPropertyName, const TArray<FBindingChainElement>& InBindingChain)
	{
		if (CachedData) CachedData->AddBinding(InBindingChain);
	});

	Args.OnRemoveBinding = FOnRemoveBinding::CreateLambda([CachedData](FName)
	{
		if (CachedData) CachedData->RemoveBinding();
	});

	Args.OnCanRemoveBinding = FOnCanRemoveBinding::CreateLambda([CachedData](FName)
	{
		return CachedData ? CachedData->CanRemoveBinding() : false;
	});

	Args.CurrentBindingText = MakeAttributeLambda([CachedData]() { return CachedData->GetText(); });
	Args.CurrentBindingToolTipText = MakeAttributeLambda([CachedData]() { return CachedData->GetTooltipText(); });
	Args.CurrentBindingColor = MakeAttributeLambda([CachedData]() { return CachedData->GetColor(); });
	Args.CurrentBindingImage = MakeAttributeLambda([CachedData]() { return CachedData->GetImage(); });
	Args.BindButtonStyle = &FAppStyle::Get().GetWidgetStyle<FButtonStyle>("HoverHintOnly");

	IPropertyAccessEditor& PropertyAccessEditor = IModularFeatures::Get().GetModularFeature<IPropertyAccessEditor>("PropertyAccessEditor");
	return PropertyAccessEditor.MakePropertyBindingWidget(Contexts, Args);
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