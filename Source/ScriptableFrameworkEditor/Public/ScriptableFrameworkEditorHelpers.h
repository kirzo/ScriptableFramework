// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "PropertyHandle.h"
#include "PropertyBindingPath.h"

class UScriptableObject;
struct FBindableStructDesc;
struct FBindingChainElement;

namespace ScriptableFrameworkEditor
{
	// --- Metadata Constants ---
	static const FName MD_SystemCategory = TEXT("System");
	static const FName MD_TaskCategory = TEXT("TaskCategory");
	static const FName MD_TaskCategories = TEXT("TaskCategories");
	static const FName MD_ConditionCategory = TEXT("ConditionCategory");
	static const FName MD_ConditionCategories = TEXT("ConditionCategories");

	// --- Metadata & Visibility Logic ---

	bool IsPropertyVisible(TSharedRef<IPropertyHandle> PropertyHandle);
	void GetScriptableCategory(const UClass* ScriptableClass, FName& ClassCategoryMeta, FName& PropertyCategoryMeta);
	bool IsPropertyBindableOutput(const FProperty* Property);

	// --- Object Hierarchy & Traversal ---

	/** Finds the owning ScriptableObject from a property handle (handling inner objects). */
	UScriptableObject* GetOuterScriptableObject(const TSharedPtr<const IPropertyHandle>& InPropertyHandle);

	/** Generates the deterministic Runtime ID (BindingID) for an object. */
	FGuid GetScriptableObjectDataID(UScriptableObject* Owner);

	/** Scans the hierarchy to find bindable sources (Parents, Siblings, Context). */
	void GetAccessibleStructs(const UScriptableObject* TargetObject, TArray<FBindableStructDesc>& OutStructDescs);

	// --- Path Generation Utils ---

	/** Generates a full binding path from a PropertyHandle (Source of Truth). */
	void MakeStructPropertyPathFromPropertyHandle(UScriptableObject* ScriptableObject, TSharedPtr<const IPropertyHandle> InPropertyHandle, FPropertyBindingPath& OutPath);

	/** Converts a PropertyAccess BindingChain into our FPropertyBindingPath format. */
	void MakeStructPropertyPathFromBindingChain(const FGuid& StructID, const TArray<FBindingChainElement>& InBindingChain, FPropertyBindingPath& OutPath);
}