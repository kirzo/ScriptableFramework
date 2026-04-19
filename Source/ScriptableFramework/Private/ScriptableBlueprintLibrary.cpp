// Copyright 2026 kirzo

#include "ScriptableBlueprintLibrary.h"
#include "ScriptablePropertyUtilities.h"
#include "StructUtils/PropertyBag.h"

// Helper function to process context parameter assignment for any scriptable container.
static void AssignContextParameterToContainer(FFrame& Stack, const UScriptStruct* ExpectedStructType, const FString& FunctionName)
{
	// 1. Retrieve the strongly typed container struct.
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	FStructProperty* ContainerProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	void* ContainerPtr = Stack.MostRecentPropertyAddress;

	// 2. Retrieve the parameter name.
	P_GET_PROPERTY(FNameProperty, ParameterName);

	if (!ContainerProp || !ContainerPtr)
	{
		// Step to clear the stack if we abort early
		Stack.StepCompiledIn<FProperty>(nullptr);
		P_FINISH;
		return;
	}

	if (!ContainerProp->Struct->IsChildOf(ExpectedStructType))
	{
		Stack.StepCompiledIn<FProperty>(nullptr);
		P_FINISH;
		return;
	}

	// Safely cast to the base container since both actions and requirements inherit from it.
	FScriptableContainer* Container = static_cast<FScriptableContainer*>(ContainerPtr);

	// Construct the dynamic property bag on the actual instance memory if it's empty.
	if (!Container->Context.IsValid())
	{
		Container->ConstructContext();
	}

	const UScriptStruct* BagStruct = Container->Context.GetPropertyBagStruct();
	const FProperty* BagProp = BagStruct ? BagStruct->FindPropertyByName(ParameterName) : nullptr;

	if (!BagProp)
	{
		// Step to clear the stack
		Stack.StepCompiledIn<FProperty>(nullptr);
		P_FINISH;
#if WITH_EDITOR
		FFrame::KismetExecutionMessage(*FString::Printf(TEXT("%s: Parameter '%s' not found in ContextDefinitions."), *FunctionName, *ParameterName.ToString()), ELogVerbosity::Warning);
#endif
		return;
	}

	// 3. Evaluate Value into a local buffer based on the expected BagProp size.
	// This PREVENTS the "Attempted to reference 'self' as an addressable property" crash
	// by giving R-Values (like 'self' or Math functions) a valid memory space to write into.
	void* LocalValue = FMemory_Alloca(BagProp->GetSize());
	BagProp->InitializeValue(LocalValue);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentProperty = nullptr;

	Stack.StepCompiledIn<FProperty>(LocalValue);

	FProperty* ValueProp = Stack.MostRecentProperty;
	void* ValuePtr = (Stack.MostRecentPropertyAddress != nullptr) ? Stack.MostRecentPropertyAddress : LocalValue;

	P_FINISH;

	FStructView MutableStruct = Container->Context.GetMutableValue();
	uint8* StructMemory = MutableStruct.GetMemory();

	if (StructMemory)
	{
		uint8* DestPtr = BagProp->ContainerPtrToValuePtr<uint8>(StructMemory);

		// If ValueProp is valid, do the rigorous compatibility check
		if (ValueProp && !FScriptablePropertyUtilities::ArePropertiesCompatible(ValueProp, BagProp))
		{
#if WITH_EDITOR
			FFrame::KismetExecutionMessage(*FString::Printf(TEXT("%s: Type mismatch for parameter '%s'."), *FunctionName, *ParameterName.ToString()), ELogVerbosity::Warning);
#endif
		}
		else
		{
			// --- QoL: Safe Internal Object Casting ---
			if (const FObjectPropertyBase* TargetObjProp = CastField<FObjectPropertyBase>(BagProp))
			{
				UObject* SourceObject = nullptr;

				// Extract UObject from ValuePtr depending on whether ValueProp was valid (Variables) or Null (R-Values like 'self')
				if (const FObjectPropertyBase* SourceObjProp = CastField<FObjectPropertyBase>(ValueProp))
				{
					SourceObject = SourceObjProp->GetObjectPropertyValue(ValuePtr);
				}
				else if (!ValueProp)
				{
					// It's an R-Value (e.g. 'self'). ValuePtr directly points to the memory where EX_Self wrote the UObject*.
					SourceObject = *(UObject**)ValuePtr;
				}

				// Check if the runtime instance is actually a match for the target class
				if (SourceObject && SourceObject->IsA(TargetObjProp->PropertyClass))
				{
					TargetObjProp->SetObjectPropertyValue(DestPtr, SourceObject);
				}
				else
				{
					// Safe failure: assign nullptr and warn
					TargetObjProp->SetObjectPropertyValue(DestPtr, nullptr);

					if (SourceObject)
					{
#if WITH_EDITOR
						FFrame::KismetExecutionMessage(*FString::Printf(TEXT("%s: Runtime cast failed for parameter '%s'. Passed object '%s' is not of type '%s'."), *FunctionName, *ParameterName.ToString(), *SourceObject->GetName(), *TargetObjProp->PropertyClass->GetName()), ELogVerbosity::Warning);
#endif
					}
				}
			}
			else
			{
				// --- Standard Assignment for other types (Primitives, Structs, Arrays) ---
				BagProp->CopyCompleteValue(DestPtr, ValuePtr);
			}
		}
	}

	// Always clean up the local memory to prevent memory leaks with Arrays/Strings
	BagProp->DestroyValue(LocalValue);
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