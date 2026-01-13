// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableConditionCustomization.h"

class SComboButton;

/**
 * Customization for UScriptableCondition_Group.
 * Merges the Group Object UI with the Inner Requirement UI.
 */
class FScriptableConditionGroupCustomization : public FScriptableConditionCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance() { return MakeShareable(new FScriptableConditionGroupCustomization); }

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	virtual void InitCustomization(TSharedRef<IPropertyHandle> InPropertyHandle, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual TSharedPtr<SHorizontalBox> GetHeaderNameContent() override;
	virtual TSharedPtr<SHorizontalBox> GetHeaderExtensionContent() override;

private:
	/** Helper instance to handle the inner requirement logic */
	TSharedPtr<class FScriptableRequirementCustomization> RequirementCustomization;

	FText GetGroupName() const;
	void OnGroupNameCommitted(const FText& NewText, ETextCommit::Type CommitInfo);
	bool VerifyGroupNameChanged(const FText& NewText, FText& OutErrorMessage);
};