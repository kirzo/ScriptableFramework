// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableContainerCustomization.h"

class FScriptableActionCustomization : public FScriptableContainerCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance() { return MakeShareable(new FScriptableActionCustomization); }

protected:
	virtual UClass* GetBaseClass() const override;
	virtual FName GetListPropertyName() const override;
	virtual FName GetModePropertyName() const override;
	virtual FSlateColor GetIconColor() const override;
	virtual FText GetAddButtonTooltip() const override;
	virtual FText GetRemoveButtonTooltip() const override;

	virtual UClass* GetWrapperClass() const override;

	virtual FText GetModeText() const override;
	virtual FSlateColor GetModeColor() const override;
	virtual FText GetModeTooltip() const override;
};