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

#include "Utils/KzObjectEditorUtils.h"
#include "StructUtils/PropertyBag.h"

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

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

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
		TSharedPtr<IPropertyHandle> PropertyHandle;
		TSharedPtr<IPropertyHandle> NotifierHandle;
		FProperty* DirectProperty = nullptr; // For dynamic variables without handle (PropertyBag)
		TArray<FPropertyBindingBindableStructDescriptor> AccessibleStructs;
		TWeakPtr<IPropertyUtilities> PropertyUtilities;

		FText Text;
		FText TooltipText;
		FLinearColor Color = FLinearColor::White;
		const FSlateBrush* Image = nullptr;
		bool bIsDataCached = false;

		FCachedBindingData(UScriptableObject* InObject, const FPropertyBindingPath& InTargetPath, TSharedPtr<IPropertyHandle> InHandle, TSharedPtr<IPropertyHandle> InNotifierHandle, const TArray<FPropertyBindingBindableStructDescriptor>& InStructs, TWeakPtr<IPropertyUtilities> InUtilities, FProperty* InDirectProperty = nullptr)
			: WeakScriptableObject(InObject)
			, TargetPath(InTargetPath)
			, PropertyHandle(InHandle)
			, NotifierHandle(InNotifierHandle)
			, DirectProperty(InDirectProperty)
			, AccessibleStructs(InStructs)
			, PropertyUtilities(InUtilities)
		{
		}

		// Helper to get the static or dynamic property safely
		FProperty* GetProperty() const
		{
			return PropertyHandle.IsValid() ? PropertyHandle->GetProperty() : DirectProperty;
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

			const FProperty* Prop = GetProperty();
			if (!Prop) return;

			UScriptableObject* ScriptableObject = WeakScriptableObject.Get();
			if (!ScriptableObject) return;

			const FScriptablePropertyBindings& Bindings = ScriptableObject->GetPropertyBindings();

			const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
			FEdGraphPinType PinType;
			Schema->ConvertPropertyToPinType(Prop, PinType);

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

								if (ScriptableFrameworkEditor::ArePropertiesCompatible(SourceProp, Prop))
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

			if (NotifierHandle.IsValid()) NotifierHandle->NotifyPreChange();

			FScopedTransaction Transaction(LOCTEXT("AddBinding", "Add Property Binding"));
			ScriptableObject->Modify();

			const FPropertyBindingBindableStructDescriptor& SelectedContext = AccessibleStructs[ContextIndex];

			TArray<FBindingChainElement> PropertyChain = InBindingChain;
			PropertyChain.RemoveAt(0);

			FPropertyBindingPath SourcePath;
			ScriptableFrameworkEditor::MakeStructPropertyPathFromBindingChain(SelectedContext.ID, PropertyChain, SourcePath);

			ScriptableObject->GetPropertyBindings().AddPropertyBinding(SourcePath, TargetPath);
			UpdateData();

			if (NotifierHandle.IsValid())
			{
				NotifierHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
				NotifierHandle->NotifyFinishedChangingProperties();
			}

			if (TSharedPtr<IPropertyUtilities> Utils = PropertyUtilities.Pin()) Utils->ForceRefresh();
		}

		void RemoveBinding()
		{
			UScriptableObject* ScriptableObject = WeakScriptableObject.Get();
			if (!ScriptableObject) return;

			if (NotifierHandle.IsValid()) NotifierHandle->NotifyPreChange();

			FScopedTransaction Transaction(LOCTEXT("RemoveBinding", "Remove Property Binding"));
			ScriptableObject->Modify();

			ScriptableObject->GetPropertyBindings().RemovePropertyBindings(TargetPath);
			UpdateData();

			if (NotifierHandle.IsValid())
			{
				NotifierHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
				NotifierHandle->NotifyFinishedChangingProperties();
			}

			if (TSharedPtr<IPropertyUtilities> Utils = PropertyUtilities.Pin()) Utils->ForceRefresh();
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
		Args.Property = CachedData->GetProperty();
		Args.bAllowPropertyBindings = true;
		Args.bGeneratePureBindings = true;
		Args.bAllowStructMemberBindings = true;
		Args.bAllowUObjectFunctions = true;

		Args.OnCanBindToClass = FOnCanBindToClass::CreateLambda([](UClass* InClass) { return true; });

		Args.OnCanBindToContextStructWithIndex = FOnCanBindToContextStructWithIndex::CreateLambda([CachedData](const UStruct* InStruct, int32 Index)
		{
			if (const FStructProperty* StructProp = CastField<FStructProperty>(CachedData->GetProperty()))
			{
				return StructProp->Struct == InStruct;
			}
			return false;
		});

		Args.OnCanBindPropertyWithBindingChain = FOnCanBindPropertyWithBindingChain::CreateLambda([CachedData](FProperty* InProperty, TArrayView<const FBindingChainElement> BindingChain)
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
			return ScriptableFrameworkEditor::ArePropertiesCompatible(InProperty, CachedData->GetProperty());
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
		if (!ScriptableObject || !CachedData.IsValid())
		{
			return;
		}

		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		TSharedPtr<IPropertyHandle> PropertyHandle = Row.GetPropertyHandle();

		// Use GetProperty() to safely support both handles and direct dynamic properties
		const FProperty* Prop = CachedData->GetProperty();
		if (!Prop) return;

		TSharedPtr<SWidget> NameWidget, ValueWidget;
		Row.GetDefaultWidgets(NameWidget, ValueWidget);

		FEdGraphPinType PinType;
		Schema->ConvertPropertyToPinType(Prop, PinType);

		const FSlateBrush* Icon = FBlueprintEditorUtils::GetIconFromPin(PinType, true);
		FText Text = GetPinTypeText(PinType);
		FLinearColor IconColor = Schema->GetPinTypeColor(PinType);

		const bool bIsManuallyBound = ScriptableObject->GetPropertyBindings().HasManualPropertyBinding(CachedData->TargetPath);

		const bool bIsInputProperty = ScriptableFrameworkEditor::IsPropertyBindableInput(Prop);
		const bool bIsOutputProperty = ScriptableFrameworkEditor::IsPropertyBindableOutput(Prop);
		const bool bIsContextCategory = ScriptableFrameworkEditor::IsPropertyBindableContext(Prop);

		// Determine Pill styling
		FText Label;
		FText LabelToolTip;

		if (bIsInputProperty)
		{
			Label = LOCTEXT("LabelInput", "IN");
			LabelToolTip = LOCTEXT("LabelInputToolTip", "This is an Input property. It is always expected to be bound to some other property.");
		}
		else if (bIsOutputProperty)
		{
			Label = LOCTEXT("LabelOutput", "OUT");
			LabelToolTip = LOCTEXT("LabelOutputToolTip", "This is an Output property. The node will always set its value, other nodes can bind to it.");
		}
		else if (bIsContextCategory)
		{
			Label = LOCTEXT("LabelContext", "CONTEXT");
			LabelToolTip = LOCTEXT("LabelContextToolTip", "This is a Context property. It is automatically connected to one of the Context objects, or can be overridden with property binding.");
		}

		// Initialize custom row
		FDetailWidgetRow& WidgetRow = Row.CustomWidget(true);

		// ---------------------------------------------------------
		// NAME CONTENT
		// ---------------------------------------------------------
		WidgetRow.NameContent()
			[
				SNew(SHorizontalBox)

					// Name
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						NameWidget.ToSharedRef()
					]

					// Pill (IN/OUT/CONTEXT)
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
			];

		// ---------------------------------------------------------
		// VALUE CONTENT
		// ---------------------------------------------------------
		if (bIsOutputProperty || (bIsInputProperty && !bIsManuallyBound))
		{
			// Read-Only Pin Display
			WidgetRow.ValueContent()
				[
					SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(4.0f, 0.0f)
						[
							SNew(SImage)
								.Image(Icon)
								.ColorAndOpacity(IconColor)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Font(IDetailLayoutBuilder::GetDetailFont())
								.ColorAndOpacity(FSlateColor::UseForeground())
								.Text(Text)
						]
				];
		}
		else if (!bIsManuallyBound)
		{
			// Unbound: Auto-Link vs Default Editable Widget
			if (bIsContextCategory)
			{
				FPropertyBindingPath AutoBindingPath;
				const bool bValidAutoBinding = ScriptableFrameworkEditor::TryDiscoverAutoBinding(Prop, CachedData->AccessibleStructs, AutoBindingPath);

				if (bValidAutoBinding)
				{
					const FName BoundParamName = AutoBindingPath.GetSegment(AutoBindingPath.NumSegments() - 1).GetName();
					WidgetRow.ValueContent()
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(4.0f, 0.0f)
								[
									SNew(SImage)
										.Image(FAppStyle::GetBrush("Icons.Link"))
										.ColorAndOpacity(IconColor)
								]
								+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
										.Font(IDetailLayoutBuilder::GetDetailFont())
										.ColorAndOpacity(FSlateColor::UseForeground())
										.Text(FText::FromName(BoundParamName))
										.ToolTipText(FText::Format(LOCTEXT("AutoBoundTooltip", "Connected to Context {0}"), FText::FromName(BoundParamName)))
								]
						];
				}
				else
				{
					WidgetRow.ValueContent()
						[
							SNew(SHorizontalBox)
								.ToolTipText(LOCTEXT("ContextWarningTooltip", "Could not connect Context property automatically."))
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(4.0f, 0.0f)
								[
									SNew(SImage)
										.Image(FAppStyle::GetBrush("Icons.Warning"))
										.ColorAndOpacity(IconColor)
								]
								+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
										.Font(IDetailLayoutBuilder::GetDetailFont())
										.ColorAndOpacity(FSlateColor::UseForeground())
										.Text(Prop->GetDisplayNameText())
								]
						];
				}
			}
			else
			{
				// Default Editable Widget
				WidgetRow.ValueContent()
					[
						SNew(SBox)[ValueWidget.ToSharedRef()]
					];
			}
		}

		// ---------------------------------------------------------
		// EXTENSION CONTENT
		// ---------------------------------------------------------
		if (!bIsOutputProperty)
		{
			WidgetRow.ExtensionContent()
				[
					SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(2.0f, 0.0f)
						[
							ScriptableBindingUI::CreateBindingWidget(PropertyHandle, CachedData).ToSharedRef()
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
}

/** Custom Array Builder that allows binding the Array property itself (Header) */
class FScriptableArrayBuilder : public FDetailArrayBuilder
{
public:
	FScriptableArrayBuilder(TSharedRef<IPropertyHandle> InBaseProperty, UScriptableObject* InObject, const TArray<FPropertyBindingBindableStructDescriptor>& InStructs, TWeakPtr<IPropertyUtilities> InUtilities)
		: FDetailArrayBuilder(InBaseProperty, true, true, true)
		, ScriptableObject(InObject)
		, AccessibleStructs(InStructs)
		, PropertyUtilities(InUtilities)
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
				MakeShared<ScriptableBindingUI::FCachedBindingData>(Obj, TargetPath, Handle, Handle, AccessibleStructs, PropertyUtilities);

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
	TWeakPtr<IPropertyUtilities> PropertyUtilities;
};

// ------------------------------------------------------------------------------------------------
// FScriptableStructBuilder
// ------------------------------------------------------------------------------------------------
class FScriptableStructBuilder : public IDetailCustomNodeBuilder
{
public:
	FScriptableStructBuilder(TSharedRef<IPropertyHandle> InHandle, UScriptableObject* InObj, FScriptableObjectCustomization* InCustomization, const TArray<FPropertyBindingBindableStructDescriptor>& InAccessibleStructs)
		: Handle(InHandle), Obj(InObj), Customization(InCustomization), AccessibleStructs(InAccessibleStructs) {
	}

	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override {}

	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override
	{
		FPropertyBindingPath TargetPath;
		ScriptableFrameworkEditor::MakeStructPropertyPathFromPropertyHandle(Obj, Handle, TargetPath);

		TSharedPtr<ScriptableBindingUI::FCachedBindingData> CachedData =
			MakeShared<ScriptableBindingUI::FCachedBindingData>(Obj, TargetPath, Handle, Handle, AccessibleStructs, Customization->GetPropertyUtilities());

		auto IsValueVisible = TAttribute<EVisibility>::Create([Obj = this->Obj, Handle = this->Handle]() -> EVisibility
			{
				if (!Obj) return EVisibility::Visible;
				FPropertyBindingPath TP;
				ScriptableFrameworkEditor::MakeStructPropertyPathFromPropertyHandle(Obj, Handle, TP);
				return Obj->GetPropertyBindings().HasPropertyBinding(TP) ? EVisibility::Collapsed : EVisibility::Visible;
			});

		TSharedPtr<SWidget> BindingWidget = ScriptableBindingUI::CreateBindingWidget(Handle, CachedData);

		FIsResetToDefaultVisible IsResetVisible = FIsResetToDefaultVisible::CreateLambda([Obj = this->Obj, TargetPath](TSharedPtr<IPropertyHandle> InHandle)
			{
				// Return true if we have a custom binding
				if (Obj && Obj->GetPropertyBindings().HasPropertyBinding(TargetPath)) return true;

				// Safely check if the handle is valid before checking its default state
				return InHandle.IsValid() && InHandle->DiffersFromDefault();
			});

		FResetToDefaultHandler ResetHandler = FResetToDefaultHandler::CreateLambda([Obj = this->Obj, TargetPath, CachedData, Customization = this->Customization](TSharedPtr<IPropertyHandle> InHandle)
			{
				if (Obj)
				{
					FScopedTransaction Transaction(LOCTEXT("Reset", "Reset Property"));
					Obj->Modify();
					if (Obj->GetPropertyBindings().HasPropertyBinding(TargetPath))
					{
						Obj->GetPropertyBindings().RemovePropertyBindings(TargetPath);
						if (CachedData) CachedData->Invalidate();
						if (Customization && Customization->GetPropertyUtilities().IsValid()) Customization->GetPropertyUtilities()->ForceRefresh();
					}
				}

				// Safely reset to default only if the handle is valid
				if (InHandle.IsValid())
				{
					InHandle->ResetToDefault();
				}
			});

		NodeRow.OverrideResetToDefault(FResetToDefaultOverride::Create(IsResetVisible, ResetHandler));

		NodeRow
			.NameContent()
			[
				Handle->CreatePropertyNameWidget()
			]
			.ValueContent()
			[
				SNew(SBox)
					.Visibility(IsValueVisible)
					[
						Handle->CreatePropertyValueWidget()
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
						Handle->CreateDefaultPropertyButtonWidgets()
					]
			];
	}

	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override
	{
		FPropertyBindingPath TargetPath;
		ScriptableFrameworkEditor::MakeStructPropertyPathFromPropertyHandle(Obj, Handle, TargetPath);

		// If the struct is entirely bound, we DO NOT generate its children properties
		if (Obj && Obj->GetPropertyBindings().HasPropertyBinding(TargetPath))
		{
			return;
		}

		uint32 NumChildren;
		if (Handle->GetNumChildren(NumChildren) == FPropertyAccess::Success)
		{
			for (uint32 i = 0; i < NumChildren; ++i)
			{
				TSharedRef<IPropertyHandle> ChildHandle = Handle->GetChildHandle(i).ToSharedRef();
				Customization->ProcessPropertyHandle(ChildHandle, ChildrenBuilder, Obj, AccessibleStructs);
			}
		}
	}

	virtual bool RequiresTick() const override { return false; }
	virtual FName GetName() const override { return Handle->GetProperty()->GetFName(); }
	virtual bool InitiallyCollapsed() const override { return true; }

private:
	TSharedRef<IPropertyHandle> Handle;
	UScriptableObject* Obj;
	FScriptableObjectCustomization* Customization;
	TArray<FPropertyBindingBindableStructDescriptor> AccessibleStructs;
};

// ------------------------------------------------------------------------------------------------
// FPropertyBagBindingsBuilder
// ------------------------------------------------------------------------------------------------
class FPropertyBagBindingsBuilder : public IDetailCustomNodeBuilder
{
public:
	FPropertyBagBindingsBuilder(TSharedRef<IPropertyHandle> InParentHandle, TSharedRef<IPropertyHandle> InBagHandle, UScriptableObject* InObj, const TArray<FPropertyBindingBindableStructDescriptor>& InAccessibleStructs, TWeakPtr<IPropertyUtilities> InUtils)
		: ParentHandle(InParentHandle), BagHandle(InBagHandle), Obj(InObj), AccessibleStructs(InAccessibleStructs), PropertyUtilities(InUtils)
	{
		// Refresh the UI if any property in the parent struct changes (e.g. selecting a new StateTree)
		ParentHandle->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateLambda([this]() { OnRegenerateChildren.ExecuteIfBound(); }));
	}

	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override { OnRegenerateChildren = InOnRegenerateChildren; }
	virtual bool RequiresTick() const override { return false; }
	virtual FName GetName() const override { return TEXT("DynamicPropertyBindings"); }
	virtual bool InitiallyCollapsed() const override { return false; }

	// Helper to check if we actually have properties to show
	bool HasAnyProperties() const
	{
		void* Data = nullptr;
		if (BagHandle->GetValueData(Data) == FPropertyAccess::Success && Data)
		{
			if (FInstancedPropertyBag* BagInstance = static_cast<FInstancedPropertyBag*>(Data))
			{
				if (const UPropertyBag* BagStruct = BagInstance->GetPropertyBagStruct())
				{
					return !BagStruct->GetPropertyDescs().IsEmpty();
				}
			}
		}
		return false;
	}

	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override
	{
		// Do not draw the header if there are no dynamic parameters
		if (!HasAnyProperties()) return;

		FText DynamicTitle;

		// Check if the Bag is nested inside another struct (e.g., Parent="StateTree", Bag="Parameters")
		if (ParentHandle->GetProperty() != BagHandle->GetProperty())
		{
			DynamicTitle = FText::Format(
				LOCTEXT("DynamicNestedBindingsFormat", "Dynamic {0} {1} Bindings"),
				ParentHandle->GetPropertyDisplayName(),
				BagHandle->GetPropertyDisplayName()
			);
		}
		else
		{
			// Direct PropertyBag case (e.g., just "MyBag" directly on the object)
			DynamicTitle = FText::Format(
				LOCTEXT("DynamicDirectBindingsFormat", "Dynamic {0} Bindings"),
				BagHandle->GetPropertyDisplayName()
			);
		}

		NodeRow.NameContent()
			[
				SNew(STextBlock)
					.Text(DynamicTitle)
					.Font(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"))
			];
	}

	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override
	{
		if (!Obj || !HasAnyProperties()) return;

		// Extract the raw FInstancedPropertyBag memory
		void* Data = nullptr;
		if (BagHandle->GetValueData(Data) != FPropertyAccess::Success || !Data) return;

		FInstancedPropertyBag* BagInstance = static_cast<FInstancedPropertyBag*>(Data);
		const UPropertyBag* BagStruct = BagInstance->GetPropertyBagStruct();
		if (!BagStruct) return;

		FPropertyBindingPath BasePath;
		ScriptableFrameworkEditor::MakeStructPropertyPathFromPropertyHandle(Obj, BagHandle, BasePath);

		// Iterate through the dynamically generated properties in the bag
		for (const FPropertyBagPropertyDesc& Desc : BagStruct->GetPropertyDescs())
		{
			FPropertyBindingPath TargetPath = BasePath;

			FString PathStr = TargetPath.ToString();
			if (!PathStr.IsEmpty())
			{
				PathStr += TEXT(".");
			}
			PathStr += Desc.Name.ToString();

			// Rebuild the path with the newly appended segment
			TargetPath.FromString(PathStr);

			// Pass the cached dynamic FProperty pointer instead of a Handle
			TSharedPtr<ScriptableBindingUI::FCachedBindingData> CachedData =
				MakeShared<ScriptableBindingUI::FCachedBindingData>(Obj, TargetPath, TSharedPtr<IPropertyHandle>(), BagHandle, AccessibleStructs, PropertyUtilities, const_cast<FProperty*>(Desc.CachedProperty));

			TSharedPtr<SWidget> BindingWidget = ScriptableBindingUI::CreateBindingWidget(nullptr, CachedData);

			// Draw a custom row for the dynamic property binding
			FDetailWidgetRow& Row = ChildrenBuilder.AddCustomRow(FText::FromName(Desc.Name));
			Row.NameContent()
				[
					SNew(STextBlock)
						.Text(FText::FromName(Desc.Name))
						.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
				]
				.ExtensionContent()
				[
					SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f, 0.0f)
						[
							BindingWidget.ToSharedRef()
						]
				];
		}
	}

private:
	TSharedRef<IPropertyHandle> ParentHandle;
	TSharedRef<IPropertyHandle> BagHandle;
	UScriptableObject* Obj;
	TArray<FPropertyBindingBindableStructDescriptor> AccessibleStructs;
	TWeakPtr<IPropertyUtilities> PropertyUtilities;
	FSimpleDelegate OnRegenerateChildren;
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

void FScriptableObjectCustomization::ProcessPropertyHandle(TSharedRef<IPropertyHandle> SubPropertyHandle, IDetailChildrenBuilder& ChildBuilder, UScriptableObject* Obj, const TArray<FPropertyBindingBindableStructDescriptor>& AccessibleStructs)
{
	if (!ScriptableFrameworkEditor::IsPropertyVisible(SubPropertyHandle))
	{
		return;
	}

	if (Obj && ScriptableFrameworkEditor::IsPropertyExtendable(SubPropertyHandle))
	{
		const FProperty* Prop = SubPropertyHandle->GetProperty();

		// Ensure the property pointer is valid before checking its type
		if (Prop)
		{
			if (Prop->IsA<FArrayProperty>())
			{
				TSharedRef<FScriptableArrayBuilder> ArrayBuilder = MakeShared<FScriptableArrayBuilder>(SubPropertyHandle, Obj, AccessibleStructs, GetPropertyUtilities());
				ArrayBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FScriptableObjectCustomization::GenerateArrayElement));
				ChildBuilder.AddCustomBuilder(ArrayBuilder);
			}
			else if (Prop->IsA<FStructProperty>())
			{
				const FStructProperty* StructProp = CastFieldChecked<FStructProperty>(Prop);

				// 1. DIRECT PROPERTY BAG DETECTION
				// If the property itself is an InstancedPropertyBag, intercept it immediately.
				if (StructProp->Struct->GetFName() == TEXT("InstancedPropertyBag"))
				{
					ChildBuilder.AddProperty(SubPropertyHandle); // Draw native bag UI
					TSharedRef<FPropertyBagBindingsBuilder> BagBuilder = MakeShared<FPropertyBagBindingsBuilder>(SubPropertyHandle, SubPropertyHandle, Obj, AccessibleStructs, PropertyUtilities);
					ChildBuilder.AddCustomBuilder(BagBuilder);
					return;
				}

				FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
				FCustomPropertyTypeLayoutMap EmptyLayoutMap;
				bool bHasCustomization = PropertyEditorModule.IsCustomizedStruct(StructProp->Struct, EmptyLayoutMap);

				FPropertyBindingPath TargetPath;
				ScriptableFrameworkEditor::MakeStructPropertyPathFromPropertyHandle(Obj, SubPropertyHandle, TargetPath);
				bool bIsBound = Obj->GetPropertyBindings().HasPropertyBinding(TargetPath);

				if (!bHasCustomization || bIsBound)
				{
					TSharedRef<FScriptableStructBuilder> StructBuilder = MakeShared<FScriptableStructBuilder>(SubPropertyHandle, Obj, this, AccessibleStructs);
					ChildBuilder.AddCustomBuilder(StructBuilder);
					return;
				}
				else
				{
					// Struct has native customization. Render normally.
					IDetailPropertyRow& Row = ChildBuilder.AddProperty(SubPropertyHandle);
					BindPropertyRow(Row, SubPropertyHandle, Obj);

					// 2. NESTED PROPERTY BAG DETECTION
					// For structs like FStateTreeReference that hide a PropertyBag inside them.
					// We use C++ reflection to find any internal bag and expose its dynamic variables.
					for (TFieldIterator<FProperty> It(StructProp->Struct); It; ++It)
					{
						if (const FStructProperty* InnerStructProp = CastField<FStructProperty>(*It))
						{
							if (InnerStructProp->Struct->GetFName() == TEXT("InstancedPropertyBag"))
							{
								TSharedPtr<IPropertyHandle> BagHandle = SubPropertyHandle->GetChildHandle(InnerStructProp->GetFName());
								if (BagHandle.IsValid())
								{
									// Pass SubPropertyHandle as the parent to track changes (e.g. StateTree asset swap)
									TSharedRef<FPropertyBagBindingsBuilder> BagBuilder = MakeShared<FPropertyBagBindingsBuilder>(SubPropertyHandle, BagHandle.ToSharedRef(), Obj, AccessibleStructs, PropertyUtilities);
									ChildBuilder.AddCustomBuilder(BagBuilder);
								}
							}
						}
					}

					return;
				}
			}
			else
			{
				IDetailPropertyRow& Row = ChildBuilder.AddProperty(SubPropertyHandle);
				BindPropertyRow(Row, SubPropertyHandle, Obj);
			}
		}
		else
		{
			// Fallback if property is null but handle exists
			ChildBuilder.AddProperty(SubPropertyHandle);
		}
	}
	else
	{
		ChildBuilder.AddProperty(SubPropertyHandle);
	}
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

				// We dispatch down to our recursive processor
				ProcessPropertyHandle(SubPropertyHandle, ChildBuilder, Obj, AccessibleStructs);
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
		FKzClipboardUtils::CopyObjectToClipboard(Obj);
	}
}

void FScriptableObjectCustomization::OnPasteNode()
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);
	if (OuterObjects.IsEmpty() || !OuterObjects[0]) return;

	UObject* NewInstance = FKzClipboardUtils::PasteObjectFromClipboard<UObject>(OuterObjects[0]);

	if (NewInstance)
	{
		// Type validation
		UClass* BaseClass = GetBaseClass();
		if (BaseClass && !NewInstance->IsA(BaseClass))
		{
			FNotificationInfo Info(FText::Format(LOCTEXT("PasteTypeMismatch", "Cannot paste '{0}' here. Expected '{1}'."), NewInstance->GetClass()->GetDisplayNameText(), BaseClass->GetDisplayNameText()));
			Info.ExpireDuration = 3.0f;
			Info.Image = FAppStyle::GetBrush("Icons.Error");
			FSlateNotificationManager::Get().AddNotification(Info);

			NewInstance->ClearFlags(RF_Transactional);
			NewInstance->MarkAsGarbage();
			return;
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

void FScriptableObjectCustomization::GenerateArrayElement(TSharedRef<IPropertyHandle> ChildHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder)
{
	if (UScriptableObject* Obj = ScriptableObject.Get())
	{
		if (ScriptableFrameworkEditor::IsPropertyExtendable(ChildHandle))
		{
			TArray<FPropertyBindingBindableStructDescriptor> AccessibleStructs;
			ScriptableFrameworkEditor::GetAccessibleStructs(Obj, ChildHandle, AccessibleStructs);
			ProcessPropertyHandle(ChildHandle, ChildrenBuilder, Obj, AccessibleStructs);
			return;
		}
	}
	ChildrenBuilder.AddProperty(ChildHandle);
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
		MakeShared<ScriptableBindingUI::FCachedBindingData>(Obj, TargetPath, Handle, Handle, AccessibleStructs, GetPropertyUtilities());

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
				if (GetPropertyUtilities().IsValid()) GetPropertyUtilities()->ForceRefresh();
			}
		}
		InHandle->ResetToDefault();
	});

	Row.OverrideResetToDefault(FResetToDefaultOverride::Create(IsResetVisible, ResetHandler));

	ScriptableBindingUI::ModifyRow(Obj, Row, CachedData);
}

#undef LOCTEXT_NAMESPACE