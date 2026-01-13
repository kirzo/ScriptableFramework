// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableObjectCustomization.h"

/**
 * Customization specifically for UScriptableTask*.
 * Overrides GetHeaderValueContent to inject Loop/DoOnce badges.
 */
class FScriptableTaskCustomization : public FScriptableObjectCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance() { return MakeShareable(new FScriptableTaskCustomization); }

protected:
	virtual UClass* GetWrapperClass() const override;
	virtual TSharedPtr<SHorizontalBox> GetHeaderValueContent() override;

private:
	// --- DoOnce Logic ---
	bool GetDoOnceValue() const;
	FReply OnDoOnceClicked();
	FSlateColor GetDoOnceColor() const;
	FText GetDoOnceText() const;
	FText GetDoOnceTooltip() const;

	// --- Loop Logic ---
	bool GetLoopValue() const;
	FReply OnLoopClicked();
	FSlateColor GetLoopColor() const;
	FText GetLoopText() const;
	FText GetLoopTooltip() const;

	// Helpers to get handles
	TSharedPtr<IPropertyHandle> GetControlProperty(FName PropertyName) const;
};