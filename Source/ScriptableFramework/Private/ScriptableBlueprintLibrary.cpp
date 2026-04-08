// Copyright 2026 kirzo

#include "ScriptableBlueprintLibrary.h"
#include "StructUtils/PropertyBag.h"

// --- Custom Thunks Helpers ---

static bool ArePropertiesCompatible(const FProperty* InputProp, const FProperty* TargetProp)
{
	// Basic class check.
	if (InputProp->GetClass() != TargetProp->GetClass())
	{
		// Special case: Byte vs Enum.
		if ((InputProp->IsA<FByteProperty>() && TargetProp->IsA<FEnumProperty>()) ||
			(InputProp->IsA<FEnumProperty>() && TargetProp->IsA<FByteProperty>()))
		{
			return true;
		}
		return false;
	}

	// Struct check.
	if (const FStructProperty* InputStruct = CastField<FStructProperty>(InputProp))
	{
		const FStructProperty* TargetStruct = CastField<const FStructProperty>(TargetProp);
		return InputStruct->Struct == TargetStruct->Struct;
	}

	// Object check (Allow Child -> Parent).
	if (const FObjectPropertyBase* InputObj = CastField<FObjectPropertyBase>(InputProp))
	{
		const FObjectPropertyBase* TargetObj = CastField<const FObjectPropertyBase>(TargetProp);
		return InputObj->PropertyClass->IsChildOf(TargetObj->PropertyClass);
	}

	// Array check.
	if (const FArrayProperty* InputArray = CastField<FArrayProperty>(InputProp))
	{
		const FArrayProperty* TargetArray = CastField<const FArrayProperty>(TargetProp);
		return ArePropertiesCompatible(InputArray->Inner, TargetArray->Inner);
	}

	// Enum check.
	if (const FEnumProperty* InputEnum = CastField<FEnumProperty>(InputProp))
	{
		const FEnumProperty* TargetEnum = CastField<const FEnumProperty>(TargetProp);
		return InputEnum->GetEnum() == TargetEnum->GetEnum();
	}

	return true;
}

// Helper function to process context parameter assignment for any scriptable container.
static void AssignContextParameterToContainer(FFrame& Stack, const UScriptStruct* ExpectedStructType, const FString& FunctionName)
{
	// Retrieve the strongly typed container struct.
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	FStructProperty* ContainerProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	void* ContainerPtr = Stack.MostRecentPropertyAddress;

	// Retrieve the parameter name.
	P_GET_PROPERTY(FNameProperty, ParameterName);

	// Retrieve the wildcard value.
	Stack.StepCompiledIn<FProperty>(nullptr);
	FProperty* ValueProp = Stack.MostRecentProperty;
	void* ValuePtr = Stack.MostRecentPropertyAddress;

	P_FINISH;

	if (!ContainerProp || !ContainerPtr || !ValueProp || !ValuePtr)
	{
		return;
	}

	if (!ContainerProp->Struct->IsChildOf(ExpectedStructType))
	{
		return;
	}

	// Safely cast to the base container since both actions and requirements inherit from it.
	FScriptableContainer* Container = static_cast<FScriptableContainer*>(ContainerPtr);

	// Construct the dynamic property bag on the actual instance memory if it's empty.
	if (!Container->Context.IsValid())
	{
		Container->ConstructContext();
	}

	// Perform manual memory copy to bypass PropertyBag SetValue abstraction issues.
	const UScriptStruct* BagStruct = Container->Context.GetPropertyBagStruct();
	const FProperty* BagProp = BagStruct ? BagStruct->FindPropertyByName(ParameterName) : nullptr;

	if (!BagProp)
	{
#if WITH_EDITOR
		FFrame::KismetExecutionMessage(*FString::Printf(TEXT("%s: Parameter '%s' not found in ContextDefinitions."), *FunctionName, *ParameterName.ToString()), ELogVerbosity::Warning);
#endif
		return;
	}

	if (!ArePropertiesCompatible(ValueProp, BagProp))
	{
#if WITH_EDITOR
		FFrame::KismetExecutionMessage(*FString::Printf(TEXT("%s: Type mismatch for parameter '%s'."), *FunctionName, *ParameterName.ToString()), ELogVerbosity::Warning);
#endif
		return;
	}

	FStructView MutableStruct = Container->Context.GetMutableValue();
	uint8* StructMemory = MutableStruct.GetMemory();

	if (StructMemory)
	{
		uint8* DestPtr = BagProp->ContainerPtrToValuePtr<uint8>(StructMemory);
		BagProp->CopyCompleteValue(DestPtr, ValuePtr);
	}
}

// --- Thunk Implementations ---

DEFINE_FUNCTION(UScriptableBlueprintLibrary::execSetActionContextParameter)
{
	AssignContextParameterToContainer(Stack, FScriptableAction::StaticStruct(), TEXT("SetActionContextParameter"));
}

DEFINE_FUNCTION(UScriptableBlueprintLibrary::execSetRequirementContextParameter)
{
	AssignContextParameterToContainer(Stack, FScriptableRequirement::StaticStruct(), TEXT("SetRequirementContextParameter"));
}