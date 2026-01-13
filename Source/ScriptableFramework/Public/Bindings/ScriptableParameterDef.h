// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/PropertyBag.h"
#include "ScriptableParameterDef.generated.h"

/**
 * Defines a single input parameter.
 * Used to generate the Context signature without exposing values.
 */
USTRUCT(BlueprintType)
struct SCRIPTABLEFRAMEWORK_API FScriptableParameterDef
{
	GENERATED_BODY()

	FScriptableParameterDef() = default;
	FScriptableParameterDef(const FName InName, const EPropertyBagPropertyType InValueType, const UObject* InValueTypeObject = nullptr)
		: Name(InName)
		, ValueTypeObject(InValueTypeObject)
		, ValueType(InValueType)
	{
	}
	FScriptableParameterDef(const FName InName, const EPropertyBagContainerType InContainerType, const EPropertyBagPropertyType InValueType, const UObject* InValueTypeObject = nullptr)
		: Name(InName)
		, ValueTypeObject(InValueTypeObject)
		, ValueType(InValueType)
		, ContainerType(InContainerType)
	{
	}

	bool IsValid() const
	{
		return Name != NAME_None && ValueType != EPropertyBagPropertyType::None;
	}

	/** Name for the property. */
	UPROPERTY(EditAnywhere)
	FName Name;

	/** Pointer to object that defines the Enum, Struct, or Class. */
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UObject> ValueTypeObject = nullptr;

	/** Type of the value described by this property. */
	UPROPERTY(EditAnywhere)
	EPropertyBagPropertyType ValueType = EPropertyBagPropertyType::Bool;

	/** Type of the container described by this property. */
	UPROPERTY(EditAnywhere)
	EPropertyBagContainerType ContainerType = EPropertyBagContainerType::None;
};