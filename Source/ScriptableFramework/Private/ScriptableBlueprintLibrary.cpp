// Copyright 2026 kirzo

#include "ScriptableBlueprintLibrary.h"
#include "ScriptableContainer.h"
#include "StructUtils/PropertyBag.h"

DEFINE_FUNCTION(UScriptableBlueprintLibrary::execSetContextParameter)
{
	// Retrieve the Container (Wildcard Struct)
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	FStructProperty* ContainerProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	void* ContainerPtr = Stack.MostRecentPropertyAddress;

	// Retrieve the Parameter Name (Standard Property)
	P_GET_PROPERTY(FNameProperty, ParameterName);

	// Retrieve the Value (Wildcard Property)
	Stack.StepCompiledIn<FProperty>(nullptr);
	FProperty* ValueProp = Stack.MostRecentProperty;
	void* ValuePtr = Stack.MostRecentPropertyAddress;

	P_FINISH;

	if (!ContainerProp || !ContainerPtr || !ValueProp || !ValuePtr)
	{
		return;
	}

	// Is the passed struct actually a FScriptableContainer (or a child like FScriptableAction)?
	if (!ContainerProp->Struct->IsChildOf(FScriptableContainer::StaticStruct()))
	{
		FFrame::KismetExecutionMessage(
			*FString::Printf(TEXT("SetContextParameter: Input struct '%s' is not a ScriptableContainer."), *ContainerProp->Struct->GetName()),
			ELogVerbosity::Error
		);
		return;
	}

	FScriptableContainer* ScriptableContainer = static_cast<FScriptableContainer*>(ContainerPtr);

	// Ensure Context exists
	if (!ScriptableContainer->Context.IsValid())
	{
		ScriptableContainer->ConstructContext();
	}

	// Set Value
	const EPropertyBagResult Result = ScriptableContainer->Context.SetValue(ParameterName, ValueProp, ValuePtr);

#if WITH_EDITOR
	if (Result == EPropertyBagResult::PropertyNotFound)
	{
		FFrame::KismetExecutionMessage(*FString::Printf(TEXT("SetContextParameter: Parameter '%s' not found in ContextDefinitions."), *ParameterName.ToString()), ELogVerbosity::Warning);
	}
	else if (Result == EPropertyBagResult::TypeMismatch)
	{
		FFrame::KismetExecutionMessage(*FString::Printf(TEXT("SetContextParameter: Type mismatch for parameter '%s'."), *ParameterName.ToString()), ELogVerbosity::Warning);
	}
#endif
}