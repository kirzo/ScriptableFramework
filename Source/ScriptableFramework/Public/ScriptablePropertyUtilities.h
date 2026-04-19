// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"

class SCRIPTABLEFRAMEWORK_API FScriptablePropertyUtilities
{
public:
	/**
	 * Type checking logic including Bidirectional QoL for Objects.
	 * Used by Blueprint Thunks at runtime and Binding systems in the Editor.
	 */
	static bool ArePropertiesCompatible(const FProperty* InputProp, const FProperty* TargetProp);

#if WITH_EDITOR
	static bool IsPropertyBindableInput(const FProperty* Property);
	static bool IsPropertyBindableOutput(const FProperty* Property);
	static bool IsPropertyBindableContext(const FProperty* Property);

	/** Checks if the Parent class allows data binding between its children (Siblings). */
	static bool AreSiblingBindingsAllowed(const UObject* ParentObject);

	/** Scans the ParentObject for any Array Property that contains the CurrentChild, and collects all previous siblings. */
	static void CollectPreviousSiblings(const UObject* ParentObject, const UObject* CurrentChild, TArray<const class UScriptableObject*>& OutObjects);

	/** Gathers all external Context Structs (e.g., Global Contexts, Owner Contexts) accessible by this object. */
	static void GatherAccessibleStructs(const class UScriptableObject* TargetObject, TArray<struct FPropertyBindingBindableStructDescriptor>& OutStructs);

	/** Attempts to automatically discover a compatible binding path for a Context property. */
	static bool FindAutoBindingPath(const FProperty* TargetProperty, const TArray<struct FPropertyBindingBindableStructDescriptor>& AccessibleStructs, struct FPropertyBindingPath& OutPath);
#endif
};