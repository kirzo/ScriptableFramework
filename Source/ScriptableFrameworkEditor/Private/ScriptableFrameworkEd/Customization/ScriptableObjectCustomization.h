// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Layout/Visibility.h"
#include "Styling/SlateTypes.h"

class IPropertyHandle;
class IPropertyUtilities;
class UScriptableObject;
class IDetailPropertyRow;
struct FAssetData;
struct FPropertyBindingBindableStructDescriptor;

/**
 * Base customization for UScriptableObject.
 * Handles the common "Header" (Checkbox, Icon, Title, Action Buttons).
 * Derived classes can inject custom widgets into Name, Value, or Extension slots.
 */
class FScriptableObjectCustomization : public IPropertyTypeCustomization
{
public:
	virtual ~FScriptableObjectCustomization();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	/** Initializes handles and cached data. Call this if overriding CustomizeHeader completely. */
	virtual void InitCustomization(TSharedRef<IPropertyHandle> InPropertyHandle, IPropertyTypeCustomizationUtils& CustomizationUtils);

	TSharedPtr<IPropertyUtilities> GetPropertyUtilities() const { return PropertyUtilities; }

	void ProcessPropertyHandle(TSharedRef<IPropertyHandle> SubPropertyHandle, IDetailChildrenBuilder& ChildBuilder, UScriptableObject* Obj, const TArray<FPropertyBindingBindableStructDescriptor>& AccessibleStructs);

	// --- Overridable Extension Points ---

	virtual TSharedPtr<SHorizontalBox> GetHeaderNameContent();
	virtual TSharedPtr<SHorizontalBox> GetHeaderValueContent();
	virtual TSharedPtr<SHorizontalBox> GetHeaderExtensionContent();

protected:
	// --- Internal Logic Helpers ---

	/** Returns the base class allowed for this property (e.g., UScriptableTask, UScriptableCondition, or UScriptableObject). */
	UClass* GetBaseClass() const;

	/** Returns the class of the wrapper task/condition (e.g. UScriptableTask_RunAsset). */
	virtual UClass* GetWrapperClass() const = 0;

	/** Helper to check if a class is a wrapper (RunAsset / EvaluateAsset). */
	bool IsWrapperClass(const UClass* Class) const;

	/** Helper to get the inner asset from a wrapper object. */
	UObject* GetInnerAsset(UScriptableObject* Obj) const;

	// --- Child Generation ---

	/** Callback to generate rows for array elements (recursively applies binding logic). */
	void GenerateArrayElement(TSharedRef<IPropertyHandle> ChildHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder);

	/** Main function to inject the Binding UI into a property row. */
	void BindPropertyRow(IDetailPropertyRow& Row, TSharedRef<IPropertyHandle> Handle, UScriptableObject* Obj);

	// --- UI Callbacks ---

	/** Callback to get the dynamic title text */
	FText GetDisplayTitle() const;

	/** Checkbox state for "bEnabled". */
	ECheckBoxState OnGetEnabled() const;
	void OnSetEnabled(ECheckBoxState NewState);

	/** Reset to default visibility logic (Yellow arrow). */
	bool IsResetToDefaultVisible(TSharedPtr<IPropertyHandle> Handle) const;
	void OnResetToDefault(TSharedPtr<IPropertyHandle> Handle);

	/** Handler for right-clicking on the row to open the context menu */
	FReply OnRowMouseUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/** Button Callbacks. */
	TSharedRef<SWidget> GenerateOptionsMenu();
	void GeneratePickerMenu(class FMenuBuilder& InMenuBuilder);
	void OnCopyNode();
	void OnPasteNode();
	void OnDuplicateNode() const;
	void OnDelete();
	void OnClear();
	void OnBrowse();
	void OnEdit();
	void OnUseSelected();

	/** Checks if the Wrapper has access to all variables required by the Inner Asset. */
	bool GetContextWarning(FText& OutTooltip) const;

	/** Callback from Picker. */
	void OnTypePicked(const UStruct* InStruct, const FAssetData& AssetData);

	/** Handles asset selection. */
	void OnAssetPicked(const FAssetData& AssetData);

	/**
	 * Creates a new instance of the selected class and assigns it to the handle.
	 * @param OnInstanceCreated Optional callback to configure the object BEFORE the UI refreshes.
	 */
	void InstantiateClass(const UClass* Class, TFunction<void()> OnInstanceCreated = nullptr);

	/** Callback for the global property changed event. */
	void OnObjectPropertyChanged(UObject* InObject, FPropertyChangedEvent& InEvent);

	/** Safe refresh called on next tick */
	void HandleForceRefresh();

protected:
	/** Handle to the property being customized (the Object pointer). */
	TSharedPtr<IPropertyHandle> PropertyHandle;

	/** Utilities to force UI refresh. */
	TSharedPtr<IPropertyUtilities> PropertyUtilities;

	/** Weak pointer to the actual object instance (if valid). */
	TWeakObjectPtr<UScriptableObject> ScriptableObject;

	// Cached data for UI generation
	FText NodeTitle;
	FText NodeDescription;

	FDelegateHandle OnObjectPropertyChangedHandle;
};