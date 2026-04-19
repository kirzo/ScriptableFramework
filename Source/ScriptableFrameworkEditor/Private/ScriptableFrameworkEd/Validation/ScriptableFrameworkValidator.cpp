// Copyright 2026 kirzo

#include "ScriptableFrameworkValidator.h"
#include "ScriptableObject.h"
#include "ScriptablePropertyUtilities.h"
#include "ScriptableFrameworkEditorHelpers.h"
#include "Bindings/ScriptablePropertyBindings.h"
#include "Engine/Blueprint.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "ScriptableFrameworkValidator"

UScriptableFrameworkValidator::UScriptableFrameworkValidator()
{
	bIsEnabled = true;
}

bool UScriptableFrameworkValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const
{
	// We want to validate ANY asset, because FScriptableAction or FScriptableRequirement 
	// can be instantiated inside anywhere
	return InAsset != nullptr;
}

EDataValidationResult UScriptableFrameworkValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext)
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	// 1. Determine the Root Object containing the memory instances
	UObject* TargetObject = InAsset;
	if (UBlueprint* Blueprint = Cast<UBlueprint>(InAsset))
	{
		if (Blueprint->GeneratedClass)
		{
			TargetObject = Blueprint->GeneratedClass->GetDefaultObject();
		}
		else
		{
			// Uncompiled or invalid blueprint, skip validation for now
			return Result;
		}
	}

	// 2. Recursively gather all UScriptableObjects nested anywhere within the target object
	TArray<UObject*> RawObjects;
	GetObjectsWithOuter(TargetObject, RawObjects, true /* bIncludeNestedObjects */);

	// 3. Validate bindings for each discovered object
	for (UObject* RawObj : RawObjects)
	{
		// Cast the raw object to our specific scriptable object type
		UScriptableObject* Obj = Cast<UScriptableObject>(RawObj);
		if (!Obj) continue;

		// Sanitize
		Obj->GetPropertyBindings().SanitizeObsoleteBindings(Obj);

		// Rebuild available contexts without relying on UI Handles
		TArray<FPropertyBindingBindableStructDescriptor> AccessibleStructs;
		FScriptablePropertyUtilities::GatherAccessibleStructs(Obj, AccessibleStructs);

		// Iterate through all properties of the ScriptableObject
		for (TFieldIterator<FProperty> It(Obj->GetClass()); It; ++It)
		{
			const FProperty* Prop = *It;

			const bool bIsInput = FScriptablePropertyUtilities::IsPropertyBindableInput(Prop);
			const bool bIsContext = FScriptablePropertyUtilities::IsPropertyBindableContext(Prop);

			if (!bIsInput && !bIsContext) continue;

			// Reconstruct the target binding path to check for manual overrides
			FPropertyBindingPath TargetPath;
			TargetPath.SetStructID(Obj->GetBindingID());
			TargetPath.AddPathSegment(Prop->GetFName());

			// Check strictly for manual bindings now
			const bool bIsManuallyBound = Obj->GetPropertyBindings().HasManualPropertyBinding(TargetPath);

			// RULE A: "In" properties MUST have a manual binding
			if (bIsInput && !bIsManuallyBound)
			{
				const FText ErrorText = FText::Format(
					LOCTEXT("MissingInputBindingError", "Validation Failed in {0} (Action: {1}): The Input property '{2}' MUST be connected to a value."),
					FText::FromName(InAssetData.AssetName),
					FText::FromString(Obj->GetName()),
					Prop->GetDisplayNameText()
				);

				AssetMessage(InAssetData, EMessageSeverity::Error, ErrorText);
				Result = EDataValidationResult::Invalid;
			}
			// RULE B: "Context" properties MUST either have a manual binding or resolve via auto-binding
			else if (bIsContext && !bIsManuallyBound)
			{
				FPropertyBindingPath AutoBindingPath;
				if (!FScriptablePropertyUtilities::FindAutoBindingPath(Prop, AccessibleStructs, AutoBindingPath))
				{
					// Resolution failed and no manual override exists
					const FText ErrorText = FText::Format(
						LOCTEXT("MissingContextBindingError", "Validation Failed in {0} (Action: {1}): Context property '{2}' could not be automatically resolved and requires a manual wire."),
						FText::FromName(InAssetData.AssetName),
						FText::FromString(Obj->GetName()),
						Prop->GetDisplayNameText()
					);

					AssetMessage(InAssetData, EMessageSeverity::Error, ErrorText);
					Result = EDataValidationResult::Invalid;
				}
			}
		}
	}

	// If no errors were found, explicitly state the asset passed our validation
	if (Result == EDataValidationResult::Valid)
	{
		AssetPasses(InAsset);
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE