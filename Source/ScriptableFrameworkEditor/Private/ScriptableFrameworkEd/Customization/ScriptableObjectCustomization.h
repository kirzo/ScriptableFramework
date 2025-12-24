// Copyright 2025 kirzo

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

class IPropertyHandle;
class IPropertyUtilities;
class UScriptableObject;

class FScriptableObjectCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance() { return MakeShareable(new FScriptableObjectCustomization); }

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	virtual void GatherChildProperties(TSharedPtr<IPropertyHandle> ChildPropertyHandle) {}

protected:
	bool bIsBlueprintClass = false;

	TSharedPtr<IPropertyUtilities> PropertyUtilities;

	TSharedPtr<SHorizontalBox> HorizontalBox;

	TSharedPtr<IPropertyHandle> PropertyHandle;
	TSharedPtr<IPropertyHandle> EnabledPropertyHandle;
	UScriptableObject* ScriptableObject = nullptr;

	UClass* GetBaseClass() const;

	ECheckBoxState GetEnabledCheckBoxState() const;
	void OnEnabledCheckBoxChanged(ECheckBoxState NewCheckedState);

	void OnNodePicked(const UStruct* InStruct, const FAssetData& InAssetData);

	void SetScriptableObjectType(const UStruct* InStruct);
	void OnAssetPicked(const FAssetData& AssetData);

	void OnUseSelected();
	void OnBrowseTo();
	void OnEdit();
	void OnClear();
};