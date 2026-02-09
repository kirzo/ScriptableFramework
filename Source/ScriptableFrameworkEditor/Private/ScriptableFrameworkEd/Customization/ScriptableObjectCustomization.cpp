// Copyright 2026 kirzo

#include "ScriptableFrameworkEd/Customization/ScriptableObjectCustomization.h"
#include "ScriptableObject.h"
#include "Bindings/ScriptablePropertyBindings.h"
#include "PropertyBindingPath.h"
#include "PropertyBindingDataView.h"

#include "ScriptableTasks/ScriptableActionAsset.h"
#include "ScriptableConditions/ScriptableRequirementAsset.h"

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
#include "Misc/StringOutputDevice.h"

#include "HAL/PlatformApplicationMisc.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "FScriptableObjectCustomization"

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
		TArray<FPropertyBindingBindableStructDescriptor> AccessibleStructs;

		FText Text;
		FText TooltipText;
		FLinearColor Color = FLinearColor::White;
		const FSlateBrush* Image = nullptr;
		bool bIsDataCached = false;

		FCachedBindingData(UScriptableObject* InObject, const FPropertyBindingPath& InTargetPath, const TSharedPtr<const IPropertyHandle>& InHandle, const TArray<FPropertyBindingBindableStructDescriptor>& InStructs)
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
					const FPropertyBindingBindableStructDescriptor* SourceDesc = AccessibleStructs.FindByPredicate([&](const FPropertyBindingBindableStructDescriptor& Desc) { return Desc.ID == SourcePath->GetStructID(); });

					bool bIsPathValid = false;
					bool bIsTypeCompatible = false;

					FString DisplayString = TEXT("Unknown");

					if (SourceDesc)
					{
						DisplayString = SourceDesc->Name.ToString();

						// Create a dummy view (only types) to validate the path
						FPropertyBindingDataView DummyView(SourceDesc->Struct, nullptr);

						TArray<FPropertyBindingPathIndirection> Indirections;
						if (SourcePath->ResolveIndirectionsWithValue(DummyView, Indirections))
						{
							bIsPathValid = true;

							if (Indirections.Num() > 0)
							{
								const FProperty* SourceProp = Indirections.Last().GetProperty();
								const FProperty* TargetProp = PropertyHandle->GetProperty();

								if (ScriptableFrameworkEditor::ArePropertiesCompatible(SourceProp, TargetProp))
								{
									bIsTypeCompatible = true;
								}
							}
						}
					}

					if (!SourceDesc || !bIsPathValid)
					{
						TooltipText = LOCTEXT("BindingErrorTooltip", "ERROR: The source property is missing.");
						Image = FAppStyle::GetBrush("Icons.Error");
						Color = FLinearColor::Red;
					}
					else if (!bIsTypeCompatible)
					{
						TooltipText = LOCTEXT("BindingTypeMismatch", "ERROR: Type Mismatch (Source type changed).");
						Image = FAppStyle::GetBrush("Icons.Error");
						Color = FLinearColor::Red;
					}
					else
					{
						if (!SourcePath->IsPathEmpty())
						{
							DisplayString += TEXT(".") + SourcePath->ToString();
						}

						Text = FText::FromString(DisplayString);
						TooltipText = FText::Format(LOCTEXT("BindingTooltip", "Bound to {0}"), Text);
						Image = FAppStyle::GetBrush(PropertyIcon);
						Color = Schema->GetPinTypeColor(PinType);
					}

					if (Text.IsEmpty())
					{
						if (!SourcePath->IsPathEmpty()) DisplayString += TEXT(".") + SourcePath->ToString();
						Text = FText::FromString(DisplayString);
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

			const FPropertyBindingBindableStructDescriptor& SelectedContext = AccessibleStructs[ContextIndex];

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
		for (const FPropertyBindingBindableStructDescriptor& Desc : CachedData->AccessibleStructs)
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
			return ScriptableFrameworkEditor::ArePropertiesCompatible(InProperty, InPropertyHandle->GetProperty());
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

/** Custom Array Builder that allows binding the Array property itself (Header) */
class FScriptableArrayBuilder : public FDetailArrayBuilder
{
public:
	FScriptableArrayBuilder(TSharedRef<IPropertyHandle> InBaseProperty, UScriptableObject* InObject, const TArray<FPropertyBindingBindableStructDescriptor>& InStructs)
		: FDetailArrayBuilder(InBaseProperty, true, true, true)
		, ScriptableObject(InObject)
		, AccessibleStructs(InStructs)
	{
	}

	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override
	{
		// 1. Generate the standard header (Name + "X elements" + Add/Clear buttons)
		FDetailArrayBuilder::GenerateHeaderRowContent(NodeRow);

		// 2. Inject Binding Logic into the header row
		if (UScriptableObject* Obj = ScriptableObject.Get())
		{
			TSharedPtr<IPropertyHandle> Handle = GetPropertyHandle();

			// Check if we can/should bind this array
			// Note: We duplicate the check logic here or assume caller checked it.
			// Let's assume valid since we created this builder.

			FPropertyBindingPath TargetPath;
			ScriptableFrameworkEditor::MakeStructPropertyPathFromPropertyHandle(Obj, Handle, TargetPath);

			TSharedPtr<ScriptableBindingUI::FCachedBindingData> CachedData =
				MakeShared<ScriptableBindingUI::FCachedBindingData>(Obj, TargetPath, Handle, AccessibleStructs);

			// --- Visibility Logic ---
			// If the array itself is bound, we hide the standard controls (ValueContent)
			// because the array content is driven by the binding.
			TSharedPtr<SWidget> StandardValueWidget = NodeRow.ValueContent().Widget;

			auto IsValueVisible = TAttribute<EVisibility>::Create([Obj, Handle]() -> EVisibility
			{
				if (!Obj) return EVisibility::Visible;
				FPropertyBindingPath TP;
				ScriptableFrameworkEditor::MakeStructPropertyPathFromPropertyHandle(Obj, Handle, TP);
				return Obj->GetPropertyBindings().HasPropertyBinding(TP) ? EVisibility::Collapsed : EVisibility::Visible;
			});

			// Wrap the standard value widget
			NodeRow.ValueContent()
				[
					SNew(SBox)
						.Visibility(IsValueVisible)
						.VAlign(VAlign_Center)
						[
							StandardValueWidget.ToSharedRef()
						]
				];

			// --- Extension Logic (The Pill) ---
			TSharedPtr<SWidget> BindingWidget = ScriptableBindingUI::CreateBindingWidget(Handle, CachedData);

			NodeRow.ExtensionContent()
				[
					SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(2.0f, 0.0f)
						[
							BindingWidget.ToSharedRef()
						]
				];
		}
	}

private:
	TWeakObjectPtr<UScriptableObject> ScriptableObject;
	TArray<FPropertyBindingBindableStructDescriptor> AccessibleStructs;
};

// ------------------------------------------------------------------------------------------------
// FScriptableObjectCustomization Implementation
// ------------------------------------------------------------------------------------------------

FScriptableObjectCustomization::~FScriptableObjectCustomization()
{
	// Clean up the global delegate
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(OnObjectPropertyChangedHandle);
}

void FScriptableObjectCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	InitCustomization(InPropertyHandle, CustomizationUtils);

	if (ScriptableObject.IsValid())
	{
		HeaderRow.OverrideResetToDefault(FResetToDefaultOverride::Create(
			FIsResetToDefaultVisible::CreateSP(this, &FScriptableObjectCustomization::IsResetToDefaultVisible),
			FResetToDefaultHandler::CreateSP(this, &FScriptableObjectCustomization::OnResetToDefault)
		));
	}

	// Border to capture mouse clicks on the row (used for right click menu).
	HeaderRow
		.NameContent()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBorder)
				.BorderImage(FStyleDefaults::GetNoBrush())
				.Padding(0)
				.OnMouseButtonUp(this, &FScriptableObjectCustomization::OnRowMouseUp)
				[
					GetHeaderNameContent().ToSharedRef()
				]
		]
		.ValueContent()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBorder)
				.BorderImage(FStyleDefaults::GetNoBrush())
				.Padding(0)
				.OnMouseButtonUp(this, &FScriptableObjectCustomization::OnRowMouseUp)
				[
					GetHeaderValueContent().ToSharedRef()
				]
		]
		.ExtensionContent()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBorder)
				.BorderImage(FStyleDefaults::GetNoBrush())
				.Padding(0)
				.OnMouseButtonUp(this, &FScriptableObjectCustomization::OnRowMouseUp)
				[
					GetHeaderExtensionContent().ToSharedRef()
				]
		]
		.CopyAction(FUIAction(FExecuteAction::CreateSP(this, &FScriptableObjectCustomization::OnCopyNode)))
		.PasteAction(FUIAction(FExecuteAction::CreateSP(this, &FScriptableObjectCustomization::OnPasteNode)));
}

void FScriptableObjectCustomization::InitCustomization(TSharedRef<IPropertyHandle> InPropertyHandle, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PropertyHandle = InPropertyHandle;
	PropertyUtilities = CustomizationUtils.GetPropertyUtilities();

	UObject* Object = nullptr;
	PropertyHandle->GetValue(Object);
	UScriptableObject* ScriptableObj = Cast<UScriptableObject>(Object);
	ScriptableObject = ScriptableObj; // Cache weak pointer

	if (ScriptableObj)
	{
		NodeDescription = ScriptableObj->GetClass()->GetToolTipText();
		NodeTitle = GetDisplayTitle();

		if (IsWrapperClass(ScriptableObj->GetClass()))
		{
			NodeTitle = ScriptableObj->GetDisplayTitle();
		}
		else
		{
			ScriptableObj->GetClass()->GetDisplayNameText();
		}
	}
	else
	{
		NodeTitle = FText::FromString(TEXT("None"));
	}

	// Subscribe to global editor event
	OnObjectPropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &FScriptableObjectCustomization::OnObjectPropertyChanged);
}

TSharedPtr<SHorizontalBox> FScriptableObjectCustomization::GetHeaderNameContent()
{
	TSharedPtr<SHorizontalBox> NameBox = SNew(SHorizontalBox);

	// Checkbox
	NameBox->AddSlot()
		.AutoWidth().Padding(0, 0, 4, 0).VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
				.Visibility(ScriptableObject.IsValid() ? EVisibility::Visible : EVisibility::Collapsed)
				.IsChecked(this, &FScriptableObjectCustomization::OnGetEnabled)
				.OnCheckStateChanged(this, &FScriptableObjectCustomization::OnSetEnabled)
				.ToolTipText(LOCTEXT("Toggle", "Enable/Disable"))
		];

	// Title
	NameBox->AddSlot()
		.FillWidth(1.0f).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
				.Text(this, &FScriptableObjectCustomization::GetDisplayTitle)
				.Font(IDetailLayoutBuilder::GetDetailFontBold())
				.ToolTipText(NodeDescription)
				.ColorAndOpacity(ScriptableObject.IsValid() ? FSlateColor::UseForeground() : FSlateColor::UseSubduedForeground())
		];

	return NameBox;
}

TSharedPtr<SHorizontalBox> FScriptableObjectCustomization::GetHeaderValueContent()
{
	return SNew(SHorizontalBox);
}

TSharedPtr<SHorizontalBox> FScriptableObjectCustomization::GetHeaderExtensionContent()
{
	TSharedPtr<SHorizontalBox> ExtensionBox = SNew(SHorizontalBox);
	UScriptableObject* ScriptableObj = ScriptableObject.Get();

	FName ClassCategory; FName PropCategory;
	ScriptableFrameworkEditor::GetScriptableCategory(GetBaseClass(), ClassCategory, PropCategory);

	// Missmatch warning
	if (ScriptableObj && IsWrapperClass(ScriptableObj->GetClass()))
	{
		ExtensionBox->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 4, 0)
			[
				SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.Warning"))
					.ColorAndOpacity(FLinearColor::Yellow)
					.Visibility_Lambda([this]()
				{
					FText Dummy;
					return GetContextWarning(Dummy) ? EVisibility::Visible : EVisibility::Collapsed;
				})
					.ToolTipText_Lambda([this]()
				{
					FText Tooltip;
					GetContextWarning(Tooltip);
					return Tooltip;
				})
			];
	}

	// Picker
	if (!ScriptableObj)
	{
		ExtensionBox->AddSlot()
			.AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SScriptableTypePicker)
					.HasDownArrow(false)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ToolTipText(LOCTEXT("ChangeType", "Change Object Type"))
					.BaseClass(GetBaseClass())
					.ClassCategoryMeta(ClassCategory)
					.FilterCategoryMeta(PropCategory)
					.Filter(ScriptableFrameworkEditor::GetPropertyMetaData(PropertyHandle, PropCategory))
					.OnNodeTypePicked(SScriptableTypePicker::FOnNodeTypePicked::CreateSP(this, &FScriptableObjectCustomization::OnTypePicked))
					.Content()
					[
						SNew(SImage)
							.ColorAndOpacity(FSlateColor::UseForeground())
							.Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
					]
			];
	}

	// Use Selected
	ExtensionBox->AddSlot()
		.AutoWidth().VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeUseSelectedButton(FSimpleDelegate::CreateSP(this, &FScriptableObjectCustomization::OnUseSelected))
		];

	// Browse / Edit
	if (ScriptableObj)
	{
		const bool bIsBlueprint = ScriptableObj->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
		const bool bIsWrapper = IsWrapperClass(ScriptableObj->GetClass());

		if (bIsBlueprint || bIsWrapper)
		{
			FText BrowseTxt = FText::Format(LOCTEXT("Browse", "Browse to '{0}'"), NodeTitle);
			ExtensionBox->AddSlot()
				.AutoWidth().Padding(2, 0).VAlign(VAlign_Center)
				[
					PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateSP(this, &FScriptableObjectCustomization::OnBrowse), BrowseTxt)
				];

			FText EditTxt = FText::Format(LOCTEXT("Edit", "Edit '{0}'"), NodeTitle);
			ExtensionBox->AddSlot()
				.AutoWidth().Padding(2, 0).VAlign(VAlign_Center)
				[
					PropertyCustomizationHelpers::MakeEditButton(FSimpleDelegate::CreateSP(this, &FScriptableObjectCustomization::OnEdit), EditTxt)
				];
		}
	}

	// Options
	ExtensionBox->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(FMargin(4.f, 0.f, 0.f, 0.f))
		[
			SNew(SComboButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnGetMenuContent(this, &FScriptableObjectCustomization::GenerateOptionsMenu)
				.ToolTipText(LOCTEXT("OptionsTooltip", "Options")) 
				.HasDownArrow(false)
				.ContentPadding(FMargin(4.f, 2.f))
				.ButtonContent()
				[
					SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.ChevronDown"))
						.ColorAndOpacity(FSlateColor::UseForeground())
				]
		];

	return ExtensionBox;
}

void FScriptableObjectCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PropertyUtilities = CustomizationUtils.GetPropertyUtilities();

	// Pre-fetch contexts once for optimization
	TArray<FPropertyBindingBindableStructDescriptor> AccessibleStructs;
	UScriptableObject* Obj = ScriptableObject.Get();
	if (Obj)
	{
		ScriptableFrameworkEditor::GetAccessibleStructs(Obj, InPropertyHandle, AccessibleStructs);
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
					if (Obj && IsPropertyExtendable(SubPropertyHandle))
					{
						const FProperty* Prop = SubPropertyHandle->GetProperty();
						if (Prop->IsA<FArrayProperty>())
						{
							TSharedRef<FScriptableArrayBuilder> ArrayBuilder = MakeShared<FScriptableArrayBuilder>(SubPropertyHandle, Obj, AccessibleStructs);

							// Bind the delegate to generate elements (for children bindings)
							ArrayBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FScriptableObjectCustomization::GenerateArrayElement));

							ChildBuilder.AddCustomBuilder(ArrayBuilder);
						}
						else
						{
							IDetailPropertyRow& Row = ChildBuilder.AddProperty(SubPropertyHandle);
							BindPropertyRow(Row, SubPropertyHandle, Obj);
						}
					}
					else
					{
						ChildBuilder.AddProperty(SubPropertyHandle);
					}
				}
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------------------------------------

UClass* FScriptableObjectCustomization::GetBaseClass() const
{
	UClass* MyClass = nullptr;
	if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(PropertyHandle->GetProperty()))
	{
		MyClass = ObjectProperty->PropertyClass;
	}
	return MyClass;
}

bool FScriptableObjectCustomization::IsWrapperClass(const UClass* Class) const
{
	return Class ? Class->IsChildOf(GetWrapperClass()) : false;
}

UObject* FScriptableObjectCustomization::GetInnerAsset(UScriptableObject* Obj) const
{
	if (!Obj) return nullptr;

	static const FName NAME_Asset = TEXT("Asset");
	if (FObjectProperty* AssetProp = CastField<FObjectProperty>(Obj->GetClass()->FindPropertyByName(NAME_Asset)))
	{
		if (UObject* Asset = AssetProp->GetObjectPropertyValue_InContainer(Obj))
		{
			return Asset;
		}
	}
	return nullptr;
}

// ------------------------------------------------------------------------------------------------
// Logic Implementations (Member Functions)
// ------------------------------------------------------------------------------------------------

FText FScriptableObjectCustomization::GetDisplayTitle() const
{
	if (UScriptableObject* Obj = ScriptableObject.Get())
	{
		// Attempt to retrieve a dynamic description (e.g., "Health > 50") from the object instance.
		// This allows the header to update in real-time as properties change.
		FText DynamicDesc = Obj->GetDisplayTitle();
		if (!DynamicDesc.IsEmpty())
		{
			return DynamicDesc;
		}
	}

	// Fallback: Use the static title calculated at initialization
	return NodeTitle;
}

ECheckBoxState FScriptableObjectCustomization::OnGetEnabled() const
{
	if (UScriptableObject* Obj = ScriptableObject.Get())
	{
		return Obj->IsEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Unchecked;
}

void FScriptableObjectCustomization::OnSetEnabled(ECheckBoxState NewState)
{
	if (UScriptableObject* Obj = ScriptableObject.Get())
	{
		// We manipulate the child property handle for Undo/Redo support
		if (TSharedPtr<IPropertyHandle> EnabledHandle = PropertyHandle->GetChildHandle("bEnabled"))
		{
			EnabledHandle->SetValue(NewState == ECheckBoxState::Checked);
		}
	}
}

bool FScriptableObjectCustomization::IsResetToDefaultVisible(TSharedPtr<IPropertyHandle> Handle) const
{
	UObject* Obj = nullptr;
	return Handle.IsValid() && Handle->GetValue(Obj) == FPropertyAccess::Success && Obj != nullptr;
}

void FScriptableObjectCustomization::OnResetToDefault(TSharedPtr<IPropertyHandle> Handle)
{
	UObject* Obj = nullptr;
	if (Handle->GetValue(Obj) == FPropertyAccess::Success && Obj)
	{
		// Re-instantiate the same class (reset to defaults)
		InstantiateClass(Obj->GetClass());
	}
}

FReply FScriptableObjectCustomization::OnRowMouseUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		// Open the existing context menu at the mouse cursor position
		FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();

		FSlateApplication::Get().PushMenu(
			FSlateApplication::Get().GetInteractiveTopLevelWindows()[0],
			WidgetPath,
			GenerateOptionsMenu(),
			MouseEvent.GetScreenSpacePosition(),
			FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
		);

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

TSharedRef<SWidget> FScriptableObjectCustomization::GenerateOptionsMenu()
{
	FMenuBuilder MenuBuilder(/*ShouldCloseWindowAfterMenuSelection*/true, /*CommandList*/nullptr);

	UScriptableObject* ScriptableObj = ScriptableObject.Get();

	if (ScriptableObj)
	{
		MenuBuilder.BeginSection(FName("Type"), LOCTEXT("Type", "Type"));

		// Picker
		MenuBuilder.AddSubMenu(
			LOCTEXT("ReplaceWith", "Replace With"),
			LOCTEXT("ReplaceTooltip", "Replace current object with a new instance of another type"),
			FNewMenuDelegate::CreateSP(this, &FScriptableObjectCustomization::GeneratePickerMenu));

		MenuBuilder.EndSection();
	}

	MenuBuilder.BeginSection(FName("Edit"), LOCTEXT("Edit", "Edit"));

	// Copy
	MenuBuilder.AddMenuEntry(
		LOCTEXT("CopyItem", "Copy"),
		LOCTEXT("CopyItemTooltip", "Copy this item"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Copy"),
		FUIAction(FExecuteAction::CreateSP(this, &FScriptableObjectCustomization::OnCopyNode))
		);

	// Paste
	MenuBuilder.AddMenuEntry(
		LOCTEXT("PasteItem", "Paste"),
		LOCTEXT("PasteItemTooltip", "Paste into this item"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Paste"),
		FUIAction(FExecuteAction::CreateSP(this, &FScriptableObjectCustomization::OnPasteNode))
		);

	// Duplicate
	MenuBuilder.AddMenuEntry(
		LOCTEXT("DuplicateItem", "Duplicate"),
		LOCTEXT("DuplicateItemTooltip", "Duplicate this item"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Duplicate"),
		FUIAction(FExecuteAction::CreateSP(this, &FScriptableObjectCustomization::OnDuplicateNode))
	);

	// Delete
	if (PropertyHandle->GetIndexInArray() != INDEX_NONE)
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("Delete", "Remove"),
			LOCTEXT("DeleteTooltip", "Remove this item from the list"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Delete"),
			FUIAction(FExecuteAction::CreateSP(this, &FScriptableObjectCustomization::OnDelete))
		);
	}

	// Clear
	if (ScriptableObj)
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("Clear", "Clear"),
			LOCTEXT("ClearTooltip", "Reset this property to None"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.X"),
			FUIAction(FExecuteAction::CreateSP(this, &FScriptableObjectCustomization::OnClear))
		);
	}

	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void FScriptableObjectCustomization::GeneratePickerMenu(class FMenuBuilder& InMenuBuilder)
{
	FName ClassCategory; FName PropCategory;
	ScriptableFrameworkEditor::GetScriptableCategory(GetBaseClass(), ClassCategory, PropCategory);

	TSharedRef<SWidget> Widget = SNew(SBox)
		.Padding(2)
		[
			SNew(SScriptableTypeSelector)
				.BaseClass(GetBaseClass())
				.ClassCategoryMeta(ClassCategory)
				.FilterCategoryMeta(PropCategory)
				.Filter(ScriptableFrameworkEditor::GetPropertyMetaData(PropertyHandle, PropCategory))
				.OnNodeTypePicked(SScriptableTypePicker::FOnNodeTypePicked::CreateSP(this, &FScriptableObjectCustomization::OnTypePicked))
		];

	InMenuBuilder.AddWidget(Widget, FText::GetEmpty(), true);
}

void FScriptableObjectCustomization::OnCopyNode()
{
	UObject* Obj = nullptr;
	PropertyHandle->GetValue(Obj);

	if (Obj)
	{
		// Manually build the data string: (PropA=ValueA, PropB=ValueB)
		FString DataString;
		FStringOutputDevice Ar;
		Ar.Log(TEXT("(")); // Start

		const UObject* CDO = Obj->GetClass()->GetDefaultObject();
		bool bFirst = true;

		// Iterate over all class properties (including parents)
		for (TFieldIterator<FProperty> It(Obj->GetClass()); It; ++It)
		{
			FProperty* Prop = *It;

			// Ignore properties that shouldn't be copied (Transient, EditorOnly, etc.)
			if (Prop->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_EditorOnly | CPF_Deprecated))
			{
				continue;
			}

			// Get pointers to current and default values (for delta export)
			const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Obj);
			const void* DefaultValuePtr = Prop->ContainerPtrToValuePtr<void>(CDO);

			FString PropValue;
			// Convert property value to text
			if (Prop->ExportText_Direct(PropValue, ValuePtr, DefaultValuePtr, Obj, PPF_Copy | PPF_SimpleObjectText))
			{
				if (!PropValue.IsEmpty())
				{
					if (!bFirst) Ar.Log(TEXT(","));
					// Format: PropName=Value
					Ar.Logf(TEXT("%s=%s"), *Prop->GetName(), *PropValue);
					bFirst = false;
				}
			}
		}
		Ar.Log(TEXT(")")); // End
		DataString = Ar;

		// Package with Class path
		// Final format: ScriptableObject Class=/Path/To.Class Data=(...)
		FString CopyStr = FString::Printf(TEXT("ScriptableObject Class=%s Data=%s"), *Obj->GetClass()->GetPathName(), *DataString);
		FPlatformApplicationMisc::ClipboardCopy(*CopyStr);
	}
}

void FScriptableObjectCustomization::OnPasteNode()
{
	FString PastedText;
	FPlatformApplicationMisc::ClipboardPaste(PastedText);

	if (PastedText.IsEmpty() || !PastedText.StartsWith("ScriptableObject")) return;

	// Extract Class
	FString ClassPath;
	FParse::Value(*PastedText, TEXT("Class="), ClassPath);

	// Resolve Class
	UClass* ResolvedClass = LoadObject<UClass>(nullptr, *ClassPath);
	if (!ResolvedClass) return;

	FString DataString;
	const FString DataKey = TEXT("Data=");
	int32 DataIndex = PastedText.Find(DataKey);
	if (DataIndex != INDEX_NONE)
	{
		DataString = PastedText.Mid(DataIndex + DataKey.Len()).TrimStartAndEnd();
	}

	// Type validation
	UClass* BaseClass = GetBaseClass();
	if (BaseClass && !ResolvedClass->IsChildOf(BaseClass))
	{
		FNotificationInfo Info(FText::Format(LOCTEXT("PasteTypeMismatch", "Cannot paste '{0}' here. Expected '{1}'."), ResolvedClass->GetDisplayNameText(), BaseClass->GetDisplayNameText()));
		Info.ExpireDuration = 3.0f;
		Info.Image = FAppStyle::GetBrush("Icons.Error");
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	// Instantiate New Object
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);
	if (OuterObjects.IsEmpty()) return;

	UObject* NewInstance = NewObject<UObject>(OuterObjects[0], ResolvedClass, NAME_None, RF_Transactional | RF_Public);

	// Manual Import
	if (!DataString.IsEmpty() && NewInstance)
	{
		// Iterate over new object properties
		for (TFieldIterator<FProperty> It(ResolvedClass); It; ++It)
		{
			FProperty* Prop = *It;
			if (Prop->HasAnyPropertyFlags(CPF_EditorOnly | CPF_Deprecated)) continue;

			FString ValStr;
			FString SearchToken = Prop->GetName() + TEXT("=");
			int32 TokenIdx = DataString.Find(SearchToken);

			if (TokenIdx != INDEX_NONE)
			{
				// The value starts right after "PropName="
				int32 ValueStartIdx = TokenIdx + SearchToken.Len();
				int32 Cursor = ValueStartIdx;
				int32 ParenCount = 0;
				bool bInsideQuote = false;
				int32 DataLen = DataString.Len();

				// Scan forward to find the end of this value
				while (Cursor < DataLen)
				{
					TCHAR Char = DataString[Cursor];

					// Toggle quote state (ignoring escaped quotes)
					if (Char == '"' && (Cursor == 0 || DataString[Cursor - 1] != '\\'))
					{
						bInsideQuote = !bInsideQuote;
					}

					if (!bInsideQuote)
					{
						if (Char == '(')
						{
							ParenCount++;
						}
						else if (Char == ')')
						{
							// If ParenCount is already 0 and we hit ')', it means we reached 
							// the end of the entire DataString container (e.g., "...LastProp=Val)")
							if (ParenCount == 0)
							{
								break;
							}
							ParenCount--;
						}
						else if (Char == ',' && ParenCount == 0)
						{
							// We found a comma at the root level, meaning this property value ended
							break;
						}
					}

					Cursor++;
				}

				// Extract the exact substring for this property
				ValStr = DataString.Mid(ValueStartIdx, Cursor - ValueStartIdx);
			}

			if (!ValStr.IsEmpty())
			{
				void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(NewInstance);

				// Apply text to memory value
				Prop->ImportText_Direct(*ValStr, ValuePtr, NewInstance, PPF_Copy | PPF_SimpleObjectText);
			}
		}

		// The bindings were pasted with the OLD object's StructID. 
		// We must update the TargetPath of all bindings to match the NEW object's ID.
		if (UScriptableObject* ScriptableObj = Cast<UScriptableObject>(NewInstance))
		{
			const FGuid NewGuid = ScriptableObj->GetBindingID();
			FScriptablePropertyBindings& Bindings = ScriptableObj->GetPropertyBindings();

			// Iterate over all bindings and patch the TargetPath to point to this new instance
			for (FScriptablePropertyBinding& Binding : Bindings.Bindings)
			{
				Binding.TargetPath.SetStructID(NewGuid);
			}
		}

		// Access raw data (memory pointers)
		TArray<void*> RawData;
		PropertyHandle->AccessRawData(RawData);

		// Notify change (vital for Undo/Redo)
		FScopedTransaction Transaction(LOCTEXT("PasteNode", "Paste Scriptable Object"));
		PropertyHandle->NotifyPreChange();

		// Direct Injection
		// Assume RawData has the same num as selected objects (usually 1)
		for (void* DataPtr : RawData)
		{
			if (DataPtr)
			{
				// FObjectProperty stores a UObject*, so cast void* to UObject**
				UObject** ObjectPtr = static_cast<UObject**>(DataPtr);
				*ObjectPtr = NewInstance;
			}
		}

		PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);

		if (PropertyUtilities)
		{
			PropertyUtilities->ForceRefresh();
		}
	}
}

void FScriptableObjectCustomization::OnDuplicateNode() const
{
	const int32 Index = PropertyHandle->GetArrayIndex();
	if (const TSharedPtr<IPropertyHandle> ParentHandle = PropertyHandle->GetParentHandle())
	{
		if (const TSharedPtr<IPropertyHandleArray> ArrayHandle = ParentHandle->AsArray())
		{
			ArrayHandle->DuplicateItem(Index);
		}
	}
}

void FScriptableObjectCustomization::OnDelete()
{
	if (PropertyHandle.IsValid())
	{
		TSharedPtr<IPropertyHandle> ParentHandle = PropertyHandle->GetParentHandle();
		if (ParentHandle.IsValid() && ParentHandle->AsArray().IsValid())
		{
			ParentHandle->AsArray()->DeleteItem(PropertyHandle->GetIndexInArray());
		}
	}
}

void FScriptableObjectCustomization::OnClear()
{
	static const FString None("None");
	PropertyHandle->SetValueFromFormattedString(None);
}

void FScriptableObjectCustomization::OnBrowse()
{
	if (UScriptableObject* Obj = ScriptableObject.Get())
	{
		UObject* Target = Obj->GetClass()->ClassGeneratedBy; // Default: Blueprint

		if (IsWrapperClass(Obj->GetClass()))
		{
			if (UObject* Inner = GetInnerAsset(Obj))
			{
				Target = Inner;
			}
		}

		if (Target)
		{
			TArray<FAssetData> SyncAssets = { FAssetData(Target) };
			GEditor->SyncBrowserToObjects(SyncAssets);
		}
	}
}

void FScriptableObjectCustomization::OnEdit()
{
	if (UScriptableObject* Obj = ScriptableObject.Get())
	{
		UObject* Target = Obj->GetClass()->ClassGeneratedBy; // Default: Blueprint

		if (IsWrapperClass(Obj->GetClass()))
		{
			if (UObject* Inner = GetInnerAsset(Obj))
			{
				Target = Inner;
			}
		}

		if (Target)
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Target);
		}
	}
}

void FScriptableObjectCustomization::OnUseSelected()
{
	TArray<FAssetData> SelectedAssets;
	GEditor->GetContentBrowserSelections(SelectedAssets);

	for (const FAssetData& AssetData : SelectedAssets)
	{
		// 1. Check for Blueprint Class
		if (UBlueprint* SelectedBlueprint = Cast<UBlueprint>(AssetData.GetAsset()))
		{
			if (SelectedBlueprint->GeneratedClass && SelectedBlueprint->GeneratedClass->IsChildOf(GetBaseClass()))
			{
				InstantiateClass(SelectedBlueprint->GeneratedClass);
				return;
			}
		}
		// 2. Check for Task/Condition Asset (Wrappers)
		if (AssetData.IsInstanceOf<UScriptableObjectAsset>())
		{
			OnAssetPicked(AssetData);
			return;
		}
	}
}

bool FScriptableObjectCustomization::GetContextWarning(FText& OutTooltip) const
{
	UScriptableObject* Wrapper = ScriptableObject.Get();
	if (!Wrapper) return false;

	// Get the Inner Asset
	UScriptableObjectAsset* InnerAsset = Cast<UScriptableObjectAsset>(GetInnerAsset(Wrapper));
	if (!InnerAsset) return false;

	// If the asset doesn't require anything, everything is OK.
	const FInstancedPropertyBag* RequiredContext = InnerAsset->GetContext();
	if (!RequiredContext || RequiredContext->GetNumPropertiesInBag() == 0) return false;

	// Get Available Contexts (Outer Scope)
	TArray<FPropertyBindingBindableStructDescriptor> AvailableContexts;
	ScriptableFrameworkEditor::GetAccessibleStructs(Wrapper, PropertyHandle, AvailableContexts);

	TArray<FString> MissingParams;

	// Iterate Requirements
	const UPropertyBag* BagStruct = RequiredContext->GetPropertyBagStruct();
	for (TFieldIterator<FProperty> It(BagStruct); It; ++It)
	{
		const FProperty* ReqProp = *It;
		const FName ReqName = ReqProp->GetFName();
		bool bFound = false;

		// Search in all available bags (Global Context, Local Variables, etc.)
		for (const FPropertyBindingBindableStructDescriptor& ContextDesc : AvailableContexts)
		{
			if (const UStruct* Struct = ContextDesc.Struct.Get())
			{
				if (const FProperty* SourceProp = Struct->FindPropertyByName(ReqName))
				{
					// Found a variable with the same name.
					// Now check for type compatibility.
					if (ScriptableFrameworkEditor::ArePropertiesCompatible(SourceProp, ReqProp))
					{
						bFound = true;
						break;
					}
				}
			}
		}

		if (!bFound)
		{
			MissingParams.Add(ReqName.ToString());
		}
	}

	if (MissingParams.Num() > 0)
	{
		FString MissingStr = FString::Join(MissingParams, TEXT(", "));
		OutTooltip = FText::Format(LOCTEXT("ContextWarning", "Missing Context Parameters.\nThe asset expects these variables, but they are not found in the current context:\n- {0}"), FText::FromString(MissingStr));
		return true;
	}

	return false;
}

void FScriptableObjectCustomization::OnTypePicked(const UStruct* InStruct, const FAssetData& AssetData)
{
	if (AssetData.IsValid())
	{
		OnAssetPicked(AssetData);
	}
	else if (const UClass* PickedClass = Cast<UClass>(InStruct))
	{
		InstantiateClass(PickedClass);
	}
}

void FScriptableObjectCustomization::OnAssetPicked(const FAssetData& AssetData)
{
	if (AssetData.IsInstanceOf(UScriptableObjectAsset::StaticClass()))
	{
		UClass* WrapperClass = GetWrapperClass();
		if (WrapperClass)
		{
			InstantiateClass(WrapperClass, [&]()
			{
				ScriptableFrameworkEditor::SetWrapperAssetProperty(PropertyHandle, AssetData.GetAsset());
			});
		}
	}
}

void FScriptableObjectCustomization::InstantiateClass(const UClass* NewClass, TFunction<void()> OnInstanceCreated)
{
	if (NewClass->IsChildOf(GetBaseClass()))
	{
		FScopedTransaction Transaction(LOCTEXT("InstantiateClass", "Instantiate Class"));
		PropertyHandle->NotifyPreChange();

		PropertyCustomizationHelpers::CreateNewInstanceOfEditInlineObjectClass(
			PropertyHandle.ToSharedRef(),
			const_cast<UClass*>(NewClass),
			EPropertyValueSetFlags::InteractiveChange
		);

		if (OnInstanceCreated)
		{
			OnInstanceCreated();
		}

		PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);

		if (PropertyUtilities) PropertyUtilities->ForceRefresh();
	}
}

void FScriptableObjectCustomization::OnObjectPropertyChanged(UObject* InObject, FPropertyChangedEvent& InEvent)
{
	// Safety Checks
	if (!ScriptableObject.IsValid() || !PropertyUtilities.IsValid())
	{
		return;
	}

	// Filter: Only care if the Object that changed is the Asset owning this Action
	// (ScriptableObject->GetOuter() is the Asset)
	if (InObject == ScriptableObject->GetOuter())
	{
		const FName PropertyName = (InEvent.Property) ? InEvent.Property->GetFName() : NAME_None;
		const FName MemberName = (InEvent.MemberProperty) ? InEvent.MemberProperty->GetFName() : NAME_None;

		// Filter
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UScriptableObjectAsset, Context))
		{
			// This allows PostEditChangeProperty to fully complete (migrating the PropertyBag)
			// before we try to read it again in the UI.
			if (GEditor)
			{
				GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateSP(this, &FScriptableObjectCustomization::HandleForceRefresh));
			}
		}
	}
}

void FScriptableObjectCustomization::HandleForceRefresh()
{
	// Check validity again as customization might have been destroyed in the meantime
	if (PropertyUtilities.IsValid())
	{
		PropertyUtilities->ForceRefresh();
	}
}

// ------------------------------------------------------------------------------------------------
// General Binding & Children Logic
// ------------------------------------------------------------------------------------------------

bool FScriptableObjectCustomization::IsPropertyExtendable(TSharedPtr<IPropertyHandle> InPropertyHandle) const
{
	const FProperty* Property = InPropertyHandle->GetProperty();
	if (!Property || Property->HasAnyPropertyFlags(CPF_PersistentInstance | CPF_EditorOnly | CPF_Config | CPF_Deprecated)) return false;

	// Filter out internal framework properties
	if (Property->HasMetaData(TEXT("NoBinding")))
	{
		return false;
	}

	// Filter out Container types (Sets, Maps).
	// The custom binding logic wraps the property widget in a custom row, which breaks 
	// the native header logic of containers (causing duplicate 'Add' buttons or preventing expansion).
	// We let Unreal handle these natively by returning false.
	if (Property->IsA<FSetProperty>() || Property->IsA<FMapProperty>())
	{
		return false;
	}

	return true;
}

void FScriptableObjectCustomization::GenerateArrayElement(TSharedRef<IPropertyHandle> ChildHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder)
{
	IDetailPropertyRow& Row = ChildrenBuilder.AddProperty(ChildHandle);
	if (UScriptableObject* Obj = ScriptableObject.Get())
	{
		if (IsPropertyExtendable(ChildHandle))
		{
			BindPropertyRow(Row, ChildHandle, Obj);
		}
	}
}

void FScriptableObjectCustomization::BindPropertyRow(IDetailPropertyRow& Row, TSharedRef<IPropertyHandle> Handle, UScriptableObject* Obj)
{
	// 1. Fetch available variables (Need Action Handle for this to find Context)
	TArray<FPropertyBindingBindableStructDescriptor> AccessibleStructs;
	ScriptableFrameworkEditor::GetAccessibleStructs(Obj, Handle, AccessibleStructs);

	// 2. Paths
	FPropertyBindingPath TargetPath;
	ScriptableFrameworkEditor::MakeStructPropertyPathFromPropertyHandle(Obj, Handle, TargetPath);

	TSharedPtr<ScriptableBindingUI::FCachedBindingData> CachedData =
		MakeShared<ScriptableBindingUI::FCachedBindingData>(Obj, TargetPath, Handle, AccessibleStructs);

	// 3. Reset Handler Logic (Local to Object now!)
	FIsResetToDefaultVisible IsResetVisible = FIsResetToDefaultVisible::CreateLambda([this, TargetPath, Handle](TSharedPtr<IPropertyHandle> InHandle)
	{
		if (UScriptableObject* MyObj = ScriptableObject.Get())
		{
			// Check local bindings
			if (MyObj->GetPropertyBindings().HasPropertyBinding(TargetPath)) return true;
		}
		return InHandle->DiffersFromDefault();
	});

	FResetToDefaultHandler ResetHandler = FResetToDefaultHandler::CreateLambda([this, TargetPath, Handle, CachedData](TSharedPtr<IPropertyHandle> InHandle)
	{
		if (UScriptableObject* MyObj = ScriptableObject.Get())
		{
			FScopedTransaction Transaction(LOCTEXT("Reset", "Reset Property"));
			MyObj->Modify();

			// Remove local binding
			if (MyObj->GetPropertyBindings().HasPropertyBinding(TargetPath))
			{
				MyObj->GetPropertyBindings().RemovePropertyBindings(TargetPath);
				if (CachedData) CachedData->Invalidate();
			}
		}
		InHandle->ResetToDefault();
	});

	Row.OverrideResetToDefault(FResetToDefaultOverride::Create(IsResetVisible, ResetHandler));

	ScriptableBindingUI::ModifyRow(Obj, Row, CachedData);
}

#undef LOCTEXT_NAMESPACE