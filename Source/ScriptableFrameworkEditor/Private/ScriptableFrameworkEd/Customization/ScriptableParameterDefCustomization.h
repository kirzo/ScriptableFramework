// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "StructUtils/PropertyBag.h"

class SComboButton;

/**
 * Customization for FScriptableParameterDef.
 * Shows: [Name] [Type Picker] [Optional SubType Picker] [Array Toggle]
 */
class FScriptableParameterDefCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance() { return MakeShareable(new FScriptableParameterDefCustomization); }

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	TSharedPtr<IPropertyHandle> StructHandle;

	TAttribute<FEdGraphPinType> PinType;

	void OnNameChanged();

	FEdGraphPinType GetTargetPinType() const;
	void OnPinTypeChanged(const FEdGraphPinType& PinType);
};