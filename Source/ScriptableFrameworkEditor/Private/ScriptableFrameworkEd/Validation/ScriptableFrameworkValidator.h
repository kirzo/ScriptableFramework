// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "ScriptableFrameworkValidator.generated.h"

/**
 * Global validator that scans any saved or validated Asset in the editor to ensure
 * that ScriptableObjects (Actions/Requirements) have valid data flow bindings.
 */
UCLASS()
class UScriptableFrameworkValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	UScriptableFrameworkValidator();

protected:
	//~UEditorValidatorBase interface
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) override;
	//~End of UEditorValidatorBase interface
};