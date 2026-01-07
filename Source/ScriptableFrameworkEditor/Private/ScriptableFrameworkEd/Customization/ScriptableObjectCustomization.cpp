// Copyright 2026 kirzo

#include "ScriptableFrameworkEd/Customization/ScriptableObjectCustomization.h"
#include "ScriptableObject.h"
#include "Bindings/ScriptablePropertyBindings.h"
#include "PropertyBindingPath.h"
#include "ScriptableFrameworkEditorHelpers.h"
#include "ScriptableFrameworkEditorStyle.h"

#include "Widgets/SScriptableTypePicker.h"
#include "PropertyCustomizationHelpers.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "IPropertyAccessEditor.h"
#include "IPropertyUtilities.h"
#include "Features/IModularFeatures.h"
#include "Styling/AppStyle.h"
#include "ScopedTransaction.h"
#include "EdGraphSchema_K2.h"
#include "StructUtils/InstancedStruct.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/SecureHash.h"

#define LOCTEXT_NAMESPACE "FScriptableObjectCustomization"

UE_DISABLE_OPTIMIZATION

// ------------------------------------------------------------------------------------------------
// Helper Namespace for UI & Widgets (Local Implementation)
// ------------------------------------------------------------------------------------------------
namespace ScriptableBindingUI
{
	/** @return text describing the pin type, matches SPinTypeSelector. */
	FText GetPinTypeText(const FEdGraphPinType& PinType)
	{
		const FName PinSubCategory = PinType.PinSubCategory;
		const UObject* PinSubCategoryObject = PinType.PinSubCategoryObject.Get();
		if (PinSubCategory != UEdGraphSchema_K2::PSC_Bitmask && PinSubCategoryObject)
		{
			if (const UField* Field = Cast<const UField>(PinSubCategoryObject))
			{
				return Field->GetDisplayNameText();
			}
			return FText::FromString(PinSubCategoryObject->GetName());
		}

		return UEdGraphSchema_K2::GetCategoryText(PinType.PinCategory, NAME_None, true);
	}

	/** Caches binding data to avoid recalculating text/color every frame. */
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

		void Invalidate()
		{
			bIsDataCached = false;
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

			if (Bindings.HasPropertyBinding(TargetPath)) // Bound
			{
				const FPropertyBindingPath* SourcePath = Bindings.GetPropertyBinding(TargetPath);
				if (SourcePath)
				{
					const FBindableStructDesc* SourceDesc = AccessibleStructs.FindByPredicate([&](const FBindableStructDesc& Desc) { return Desc.ID == SourcePath->GetStructID(); });

					bool bIsBindingValid = false;

					// Default name if no valid source is found
					FString DisplayString = TEXT("Unknown");

					if (SourceDesc)
					{
						DisplayString = SourceDesc->Name.ToString();

						// Create a dummy view (only types) to validate the path
						FPropertyBindingDataView DummyView(SourceDesc->Struct, nullptr);

						// ResolveIndirections returns false if any path segment does not exist
						TArray<FPropertyBindingPathIndirection> Indirections;
						if (SourcePath->ResolveIndirectionsWithValue(DummyView, Indirections))
						{
							bIsBindingValid = true;
						}
					}

					if (!SourcePath->IsPathEmpty())
					{
						DisplayString += TEXT(".") + SourcePath->ToString();
					}

					Text = FText::FromString(DisplayString);

					if (bIsBindingValid)
					{
						TooltipText = FText::Format(LOCTEXT("BindingTooltip", "Bound to {0}"), Text);
						Image = FAppStyle::GetBrush(PropertyIcon);
						Color = Schema->GetPinTypeColor(PinType);
					}
					else
					{
						TooltipText = LOCTEXT("BindingErrorTooltip", "ERROR: The source property or task is missing.");
						Image = FAppStyle::GetBrush("Icons.Error");
						Color = FLinearColor::Red;
					}
				}
			}
			else // Unbound
			{
				Text = FText::GetEmpty();
				TooltipText = LOCTEXT("BindActionTooltip", "Bind this property to a value from the Context or a sibling node.");
				Image = FAppStyle::GetBrush(PropertyIcon);
				Color = Schema->GetPinTypeColor(PinType);
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
			ScriptableFrameworkEditor::MakeStructPropertyPathFromBindingChain(SelectedContext.ID, PropertyChain, SourcePath);

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

	/** Static helper to create the binding widget, shared between class methods and customization loop. */
	TSharedPtr<SWidget> CreateBindingWidget(TSharedPtr<IPropertyHandle> InPropertyHandle, TSharedPtr<FCachedBindingData> CachedData)
	{
		if (!IModularFeatures::Get().IsModularFeatureAvailable("PropertyAccessEditor"))
		{
			return SNullWidget::NullWidget;
		}

		TArray<FBindingContextStruct> Contexts;
		for (const FBindableStructDesc& Desc : CachedData->AccessibleStructs)
		{
			if (Desc.IsValid())
			{
				FBindingContextStruct& ContextStruct = Contexts.AddDefaulted_GetRef();
				ContextStruct.DisplayText = FText::FromName(Desc.Name);
				ContextStruct.Struct = const_cast<UStruct*>(Desc.Struct.Get());
			}
		}

		FPropertyBindingWidgetArgs Args;
		Args.Property = InPropertyHandle->GetProperty();
		Args.bAllowPropertyBindings = true;
		Args.bGeneratePureBindings = true;
		Args.bAllowStructMemberBindings = true;
		Args.bAllowUObjectFunctions = true;

		Args.OnCanBindToClass = FOnCanBindToClass::CreateLambda([](UClass* InClass) { return true; });

		Args.OnCanBindToContextStructWithIndex = FOnCanBindToContextStructWithIndex::CreateLambda([InPropertyHandle](const UStruct* InStruct, int32 Index)
		{
			if (const FStructProperty* StructProp = CastField<FStructProperty>(InPropertyHandle->GetProperty()))
			{
				return StructProp->Struct == InStruct;
			}
			return false;
		});

		Args.OnCanBindPropertyWithBindingChain = FOnCanBindPropertyWithBindingChain::CreateLambda([InPropertyHandle](FProperty* InProperty, TArrayView<const FBindingChainElement> BindingChain)
		{
			// 1. Check the property itself (Leaf)
			if (InProperty->HasMetaData(TEXT("NoBinding")))
			{
				return false;
			}

			// 2. Check the hierarchy chain (Parents)
			// If any parent property in the path is marked as NoBinding, we block this child too.
			for (const FBindingChainElement& Element : BindingChain)
			{
				if (const FProperty* ChainProp = Element.Field.Get<FProperty>())
				{
					if (ChainProp->HasMetaData(TEXT("NoBinding")))
					{
						return false;
					}
				}
			}

			// 3. Check Type Compatibility
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

	void ModifyRow(UScriptableObject* ScriptableObject, IDetailPropertyRow& Row, TSharedPtr<FCachedBindingData> CachedData)
	{
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		TSharedPtr<IPropertyHandle> PropertyHandle = Row.GetPropertyHandle();

		TSharedPtr<SWidget> NameWidget, ValueWidget;
		Row.GetDefaultWidgets(NameWidget, ValueWidget);

		FEdGraphPinType PinType;
		Schema->ConvertPropertyToPinType(PropertyHandle->GetProperty(), PinType);

		auto IsValueVisible = TAttribute<EVisibility>::Create([ScriptableObject, PropertyHandle]() -> EVisibility
		{
			if (!ScriptableObject) return EVisibility::Visible;
			FPropertyBindingPath TP;
			ScriptableFrameworkEditor::MakeStructPropertyPathFromPropertyHandle(ScriptableObject, PropertyHandle, TP);
			return ScriptableObject->GetPropertyBindings().HasPropertyBinding(TP) ? EVisibility::Collapsed : EVisibility::Visible;
		});

		const FSlateBrush* Icon = FBlueprintEditorUtils::GetIconFromPin(PinType, true);
		FText Text = GetPinTypeText(PinType);

		FText ToolTip;
		FLinearColor IconColor = Schema->GetPinTypeColor(PinType);
		FText Label;
		FText LabelToolTip;
		FSlateColor TextColor = FSlateColor::UseForeground();

		const bool bIsOutputProperty = ScriptableFrameworkEditor::IsPropertyBindableOutput(PropertyHandle->GetProperty());

		if (bIsOutputProperty)
		{
			Label = LOCTEXT("LabelOutput", "OUT");
			LabelToolTip = LOCTEXT("OutputToolTip", "This is Output property. The node will always set it's value, other nodes can bind to it.");
		}

		TSharedPtr<SWidget> ContentWidget = SNullWidget::NullWidget;
		if (bIsOutputProperty)
		{
			ContentWidget = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.0f, 0.0f)
				[
					SNew(SImage)
						.Image(Icon)
						.ColorAndOpacity(IconColor)
						.ToolTipText(ToolTip)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.ColorAndOpacity(TextColor)
						.Text(Text)
						.ToolTipText(ToolTip)
				];
		}
		else
		{
			ContentWidget = SNew(SBox)
				.Visibility(IsValueVisible)
				[
					ValueWidget.ToSharedRef()
				];
		}

		TSharedPtr<SWidget> BindingWidget = bIsOutputProperty ? SNullWidget::NullWidget : ScriptableBindingUI::CreateBindingWidget(PropertyHandle, CachedData);

		Row
			.CustomWidget(true)
			.NameContent()
			[
				SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						NameWidget.ToSharedRef()
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(4.0f, 0.0f)
					[
						SNew(SBorder)
							.Padding(FMargin(6.0f, 0.0f))
							.BorderImage(FScriptableFrameworkEditorStyle::Get().GetBrush("ScriptableFramework.Param.Background"))
							.Visibility(Label.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
							[
								SNew(STextBlock)
									.TextStyle(FScriptableFrameworkEditorStyle::Get(), "ScriptableFramework.Param.Label")
									.ColorAndOpacity(FStyleColors::Foreground)
									.Text(Label)
									.ToolTipText(LabelToolTip)
							]
					]

			]
			.ValueContent()
			[
				ContentWidget.ToSharedRef()
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
						PropertyHandle->CreateDefaultPropertyButtonWidgets()
					]
			];
	}
}

// ------------------------------------------------------------------------------------------------
// FScriptableObjectCustomization Implementation
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
		ScriptableObject = ScriptableFrameworkEditor::GetOuterScriptableObject(ChildPropertyHandle);

		bIsBlueprintClass = ScriptableObject.IsValid() && ScriptableObject->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint);

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
						if (UScriptableObject* Obj = ScriptableObject.Get())
						{
							static const FName CandidateProperties[] = { FName("AssetToRun"), FName("AssetToEvaluate") };

							for (const FName& PropName : CandidateProperties)
							{
								if (FObjectProperty* AssetProp = CastField<FObjectProperty>(Obj->GetClass()->FindPropertyByName(PropName)))
								{
									if (UObject* AssignedAsset = AssetProp->GetObjectPropertyValue_InContainer(Obj))
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

	// Pre-fetch contexts once for optimization
	TArray<FBindableStructDesc> AccessibleStructs;
	UScriptableObject* Obj = ScriptableObject.Get();
	if (Obj)
	{
		ScriptableFrameworkEditor::GetAccessibleStructs(Obj, AccessibleStructs);
	}

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

					if (Obj && IsPropertyExtendable(SubPropertyHandle))
					{
						// Create CachedData locally to capture it in the Reset Handler
						FPropertyBindingPath TargetPath;
						ScriptableFrameworkEditor::MakeStructPropertyPathFromPropertyHandle(Obj, SubPropertyHandle, TargetPath);

						TSharedPtr<ScriptableBindingUI::FCachedBindingData> CachedData =
							MakeShared<ScriptableBindingUI::FCachedBindingData>(Obj, TargetPath, SubPropertyHandle, AccessibleStructs);

						// --- RESET HANDLER ---
						// This ensures the yellow reset arrow appears if bound, and handles unbinding upon click.
						FIsResetToDefaultVisible IsResetVisible = FIsResetToDefaultVisible::CreateLambda([this, SubPropertyHandle](TSharedPtr<IPropertyHandle> InHandle)
						{
							if (!ScriptableObject.IsValid()) return false;

							FPropertyBindingPath TP;
							ScriptableFrameworkEditor::MakeStructPropertyPathFromPropertyHandle(ScriptableObject.Get(), SubPropertyHandle, TP);
							if (ScriptableObject->GetPropertyBindings().HasPropertyBinding(TP))
							{
								return true;
							}
							return InHandle->DiffersFromDefault();
						});

						FResetToDefaultHandler ResetHandler = FResetToDefaultHandler::CreateLambda([this, SubPropertyHandle, CachedData](TSharedPtr<IPropertyHandle> InHandle)
						{
							if (!ScriptableObject.IsValid()) return;

							FScopedTransaction Transaction(LOCTEXT("ResetToDefault", "Reset Property to Default"));
							ScriptableObject->Modify();

							FPropertyBindingPath TP;
							ScriptableFrameworkEditor::MakeStructPropertyPathFromPropertyHandle(ScriptableObject.Get(), SubPropertyHandle, TP);

							if (ScriptableObject->GetPropertyBindings().HasPropertyBinding(TP))
							{
								ScriptableObject->GetPropertyBindings().RemovePropertyBindings(TP);

								// Force refresh the binding widget immediately
								if (CachedData) CachedData->Invalidate();
							}

							InHandle->ResetToDefault();
						});

						Row.OverrideResetToDefault(FResetToDefaultOverride::Create(IsResetVisible, ResetHandler));
						// ---------------------

						ScriptableBindingUI::ModifyRow(Obj, Row, CachedData);
					}
				}
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------
// General Customization Logic
// ------------------------------------------------------------------------------------------------

bool FScriptableObjectCustomization::IsPropertyExtendable(TSharedPtr<IPropertyHandle> InPropertyHandle) const
{
	const FProperty* Property = InPropertyHandle->GetProperty();
	if (!Property) return false;

	// 1. Filter out internal framework properties
	if (Property->HasMetaData(TEXT("NoBinding")))
	{
		return false;
	}

	// 2. Filter out system flags (Config, Deprecated, etc.)
	if (Property->HasAnyPropertyFlags(CPF_PersistentInstance | CPF_EditorOnly | CPF_Config | CPF_Deprecated))
	{
		return false;
	}

	// 3. Filter out Container types (Arrays, Sets, Maps).
	// The custom binding logic wraps the property widget in a custom row, which breaks 
	// the native header logic of containers (causing duplicate 'Add' buttons or preventing expansion).
	// We let Unreal handle these natively by returning false.
	if (Property->IsA<FArrayProperty>() || Property->IsA<FSetProperty>() || Property->IsA<FMapProperty>())
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

	TArray<FBindableStructDesc> AccessibleStructs;
	ScriptableFrameworkEditor::GetAccessibleStructs(InScriptableObject, AccessibleStructs);

	FPropertyBindingPath TargetPath;
	ScriptableFrameworkEditor::MakeStructPropertyPathFromPropertyHandle(InScriptableObject, InPropertyHandle, TargetPath);

	TSharedPtr<ScriptableBindingUI::FCachedBindingData> CachedData =
		MakeShared<ScriptableBindingUI::FCachedBindingData>(InScriptableObject, TargetPath, InPropertyHandle, AccessibleStructs);

	return ScriptableBindingUI::CreateBindingWidget(InPropertyHandle, CachedData);
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
	if (ScriptableObject.IsValid())
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

UE_ENABLE_OPTIMIZATION

#undef LOCTEXT_NAMESPACE