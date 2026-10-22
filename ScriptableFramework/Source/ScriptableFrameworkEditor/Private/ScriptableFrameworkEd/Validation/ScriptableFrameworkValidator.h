// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "ScriptableFrameworkValidator.generated.h"

/**
 * Global save-time validator: runs the shared binding pass on any asset that may
 * nest UScriptableObjects and forwards issues to the message log. UScriptableGraph
 * assets are skipped: their UKzAssetValidators run through KzLib's bridge instead.
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
