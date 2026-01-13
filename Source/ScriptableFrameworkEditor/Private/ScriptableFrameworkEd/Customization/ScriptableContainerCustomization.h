// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Input/Reply.h"
#include "Styling/SlateColor.h"

class IPropertyUtilities;
class SComboButton;

/**
 * Base customization for Scriptable Containers (Actions, Requirements).
 * Handles the standard Header (Title, Mode Toggle, Add Button) and the List Builder.
 */
class FScriptableContainerCustomization : public IPropertyTypeCustomization
{
public:
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	virtual void InitCustomization(TSharedRef<IPropertyHandle> InStructHandle, IPropertyTypeCustomizationUtils& CustomizationUtils);

	// --- Header Content Extension Points ---
	virtual TSharedPtr<SHorizontalBox> GetHeaderNameContent();
	virtual TSharedPtr<SHorizontalBox> GetHeaderValueContent();
	virtual TSharedPtr<SHorizontalBox> GetHeaderExtensionContent();

protected:
	/** The class used for the Picker (e.g. UScriptableTask or UScriptableCondition). */
	virtual UClass* GetBaseClass() const = 0;

	/** The name of the array property (e.g. "Tasks" or "Conditions"). */
	virtual FName GetListPropertyName() const = 0;

	/** The name of the mode property (e.g. "Mode"). */
	virtual FName GetModePropertyName() const = 0;

	/** Color for the Add button icon. */
	virtual FSlateColor GetIconColor() const = 0;

	/** Tooltip for the Add button. */
	virtual FText GetAddButtonTooltip() const = 0;

	// --- Wrapper Configuration ---

	/** Returns the class of the wrapper task/condition (e.g. UScriptableTask_RunAsset). */
	virtual UClass* GetWrapperClass() const = 0;

	// --- Mode Logic ---

	virtual FText GetModeText() const = 0;
	virtual FSlateColor GetModeColor() const = 0;
	virtual FText GetModeTooltip() const = 0;

protected:
	TSharedPtr<IPropertyHandle> StructHandle;
	TSharedPtr<IPropertyHandle> ListHandle;
	TSharedPtr<IPropertyHandle> ModeHandle;

	TSharedPtr<SComboButton> AddComboButton;
	TSharedPtr<IPropertyUtilities> PropertyUtilities;

private:
	/** Callback from Picker. */
	void OnTypePicked(const UStruct* InStruct, const struct FAssetData& AssetData);

	/** Handles asset selection. */
	void OnAssetPicked(const FAssetData& AssetData);

	/** Adds a new element of the given class to the array. */
	TSharedPtr<IPropertyHandle> AddElement(const UClass* ElementClass);

	/** Callback to toggle mode (generic implementation flips the uint8). */
	FReply OnModeClicked();

	/** Array Row Generator. */
	void OnGenerateElement(TSharedRef<IPropertyHandle> ElementHandle, int32 Index, IDetailChildrenBuilder& Builder);
};