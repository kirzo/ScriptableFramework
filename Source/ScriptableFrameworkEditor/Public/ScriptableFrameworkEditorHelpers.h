// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "PropertyHandle.h"
#include "PropertyBindingPath.h"

class UScriptableObject;
struct FPropertyBindingBindableStructDescriptor;
struct FBindingChainElement;

namespace ScriptableFrameworkEditor
{
	// --- Metadata Constants ---
	static const FName MD_SystemCategory = TEXT("System");
	static const FName MD_TaskCategory = TEXT("TaskCategory");
	static const FName MD_TaskCategories = TEXT("TaskCategories");
	static const FName MD_ConditionCategory = TEXT("ConditionCategory");
	static const FName MD_ConditionCategories = TEXT("ConditionCategories");

	bool IsPropertyVisible(TSharedRef<IPropertyHandle> PropertyHandle);
	bool IsPropertyExtendable(TSharedPtr<IPropertyHandle> InPropertyHandle);
	void GetScriptableCategory(const UClass* ScriptableClass, FName& ClassCategoryMeta, FName& PropertyCategoryMeta);

	/** Retrieves metadata from a property handle, bubbling up to parent handles if necessary. */
	FString GetPropertyMetaData(const TSharedPtr<IPropertyHandle>& Handle, const FName& MetaDataKey);

	/** Validates if a binding path exists and returns the final (Leaf) property for type checking. */
	bool ValidateBindingPath(const UStruct* ContextStruct, const FPropertyBindingPath* Path, const FProperty*& OutLeafProperty);

	/** Generates a clean text for the UI, hiding "K2_" prefixes. */
	FText GetCleanBindingPathText(const FPropertyBindingPath* Path);

	/** Finds the owning ScriptableObject from a property handle (handling inner objects). */
	UScriptableObject* GetOuterScriptableObject(const TSharedPtr<const IPropertyHandle>& InPropertyHandle);

	/** Generates the deterministic Runtime ID (BindingID) for an object. */
	FGuid GetScriptableObjectDataID(UScriptableObject* Owner);

	/** Generates a full binding path from a PropertyHandle (Source of Truth). */
	void MakeStructPropertyPathFromPropertyHandle(UScriptableObject* ScriptableObject, TSharedPtr<const IPropertyHandle> InPropertyHandle, FPropertyBindingPath& OutPath);

	/** Converts a PropertyAccess BindingChain into our FPropertyBindingPath format. */
	void MakeStructPropertyPathFromBindingChain(const FGuid& StructID, const TArray<FBindingChainElement>& InBindingChain, FPropertyBindingPath& OutPath);

	/** Helper to set the internal asset on wrapper tasks. */
	void SetWrapperAssetProperty(TSharedPtr<IPropertyHandle> Handle, UObject* Asset);
}