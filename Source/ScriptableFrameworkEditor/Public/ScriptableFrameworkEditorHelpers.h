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

	bool IsPropertyVisible(TSharedRef<IPropertyHandle> PropertyHandle);
	void GetScriptableCategory(const UClass* ScriptableClass, FName& ClassCategoryMeta, FName& PropertyCategoryMeta);
	bool IsPropertyBindableOutput(const FProperty* Property);

	/** Checks if two properties are compatible for binding. */
	bool ArePropertiesCompatible(const FProperty* SourceProp, const FProperty* TargetProp);

	/** Finds the owning ScriptableObject from a property handle (handling inner objects). */
	UScriptableObject* GetOuterScriptableObject(const TSharedPtr<const IPropertyHandle>& InPropertyHandle);

	/** Generates the deterministic Runtime ID (BindingID) for an object. */
	FGuid GetScriptableObjectDataID(UScriptableObject* Owner);

	/** Scans the hierarchy to find bindable sources (Action Context, Parents, Siblings). */
	void GetAccessibleStructs(const UScriptableObject* TargetObject, const TSharedPtr<IPropertyHandle>& Handle, TArray<FBindableStructDesc>& OutStructDescs);

	/** Generates a full binding path from a PropertyHandle (Source of Truth). */
	void MakeStructPropertyPathFromPropertyHandle(UScriptableObject* ScriptableObject, TSharedPtr<const IPropertyHandle> InPropertyHandle, FPropertyBindingPath& OutPath);

	/** Converts a PropertyAccess BindingChain into our FPropertyBindingPath format. */
	void MakeStructPropertyPathFromBindingChain(const FGuid& StructID, const TArray<FBindingChainElement>& InBindingChain, FPropertyBindingPath& OutPath);

	/** Helper to set the internal asset on wrapper tasks. */
	void SetWrapperAssetProperty(TSharedPtr<IPropertyHandle> Handle, UObject* Asset);

	// --- Action & Binding Helpers ---

	/** Finds the PropertyHandle for the FScriptableContainer struct that contains the given child handle. */
	TSharedPtr<IPropertyHandle> FindContainerStructHandle(TSharedPtr<IPropertyHandle> ChildHandle);

	// Finds the PropertyHandle corresponding to the TargetObject by walking up ---
	TSharedPtr<IPropertyHandle> FindObjectHandleInHierarchy(TSharedPtr<IPropertyHandle> StartHandle, const UObject* TargetObject);

	// Collect siblings using Editor Handles (works for Struct Arrays)
	void CollectSiblingsFromHandle(TSharedPtr<IPropertyHandle> ObjectHandle, TArray<const UScriptableObject*>& OutObjects);
}