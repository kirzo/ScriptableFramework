// Copyright Kirzo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ScriptableObjectCustomization.h"

class IPropertyHandle;

class FScriptableConditionCustomization : public FScriptableObjectCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance() { return MakeShareable(new FScriptableConditionCustomization); }

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void GatherChildProperties(TSharedPtr<IPropertyHandle> ChildPropertyHandle) override;

private:
	TSharedPtr<IPropertyHandle> NegatePropertyHandle;

	ECheckBoxState GetNegateCheckBoxState() const;
	void OnNegateCheckBoxChanged(ECheckBoxState NewCheckedState);
};