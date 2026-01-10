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

/**
 * Base customization for UScriptableObject.
 * Handles the common "Header" (Checkbox, Icon, Title, Action Buttons).
 * Derived classes can inject custom widgets into Name, Value, or Extension slots.
 */
class FScriptableObjectCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance() { return MakeShareable(new FScriptableObjectCustomization); }

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

protected:
	// --- Overridable Extension Points ---

	virtual TSharedPtr<SHorizontalBox> GetHeaderNameContent();
	virtual TSharedPtr<SHorizontalBox> GetHeaderValueContent();
	virtual TSharedPtr<SHorizontalBox> GetHeaderExtensionContent();

	// --- Internal Logic Helpers ---

	/** Returns the base class allowed for this property (e.g., UScriptableTask, UScriptableCondition, or UScriptableObject). */
	UClass* GetBaseClass() const;

	/** Determines if a child property supports binding (skips Arrays/Maps/Sets and metadata "NoBinding"). */
	bool IsPropertyExtendable(TSharedPtr<IPropertyHandle> InPropertyHandle) const;

	/** Helper to check if a class is a wrapper (RunAsset / EvaluateAsset). */
	bool IsWrapperClass(const UClass* Class) const;

	/** Helper to get the inner asset from a wrapper object. */
	UObject* GetInnerAsset(UScriptableObject* Obj) const;

	/** Gets the display title (handles wrapping logic). */
	FText GetDisplayTitle(UScriptableObject* Obj) const;

	/** Creates a new instance of the selected class and assigns it to the handle. */
	void SetScriptableObjectType(const UClass* NewClass);

	/** Handles setting up a wrapper task/condition when an asset is picked. */
	void OnAssetPicked(const FAssetData& AssetData);

	// --- Child Generation ---

	/** Callback to generate rows for array elements (recursively applies binding logic). */
	void GenerateArrayElement(TSharedRef<IPropertyHandle> ChildHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder);

	/** Main function to inject the Binding UI into a property row. */
	void BindPropertyRow(IDetailPropertyRow& Row, TSharedRef<IPropertyHandle> Handle, UScriptableObject* Obj);

	// --- UI Callbacks ---

	/** Checkbox state for "bEnabled". */
	ECheckBoxState OnGetEnabled() const;
	void OnSetEnabled(ECheckBoxState NewState);

	/** Reset to default visibility logic (Yellow arrow). */
	bool IsResetToDefaultVisible(TSharedPtr<IPropertyHandle> Handle) const;
	void OnResetToDefault(TSharedPtr<IPropertyHandle> Handle);

	/** Button Callbacks. */
	void OnDelete();
	void OnClear();
	void OnBrowse();
	void OnEdit();
	void OnUseSelected();

	/** Callback when a type is picked from the SScriptableTypePicker. */
	void OnTypePicked(const UStruct* InStruct, const FAssetData& AssetData);

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
};