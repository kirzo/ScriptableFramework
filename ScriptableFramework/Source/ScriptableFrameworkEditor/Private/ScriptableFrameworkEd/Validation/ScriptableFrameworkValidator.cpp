// Copyright 2026 kirzo

#include "ScriptableFrameworkValidator.h"
#include "ScriptableFrameworkEd/Validation/ScriptableBindingsValidation.h"
#include "ScriptableNodes/ScriptableGraph.h"
#include "Validation/KzAssetValidationUtils.h"

UScriptableFrameworkValidator::UScriptableFrameworkValidator()
{
	bIsEnabled = true;
}

bool UScriptableFrameworkValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const
{
	// A graph's UKzAssetValidators already run its bindings pass (through KzLib's bridge): skip it to avoid duplicates.
	return InAsset && !InAsset->IsA<UScriptableGraph>();
}

EDataValidationResult UScriptableFrameworkValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext)
{
	TArray<FKzValidationIssue> Issues;
	FScriptableBindingsValidation::ValidateBindings(InAsset, Issues);
	return FKzAssetValidationUtils::ReportToContext(InAssetData, Issues, InContext);
}