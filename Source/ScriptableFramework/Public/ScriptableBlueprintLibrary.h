// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ScriptableContainer.h"
#include "ScriptableBlueprintLibrary.generated.h"

UCLASS()
class SCRIPTABLEFRAMEWORK_API UScriptableBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Sets a context parameter value by name.
	 * @param Container      The scriptable container (Action/Requirement) to modify.
	 * @param ParameterName  The name of the parameter (must match ContextDefinitions).
	 * @param Value          The value to set. Type must match the definition.
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "Scriptable Framework|Context", meta = (CustomStructureParam = "Container,Value", AutoCreateRefTerm = "Value"))
	static void SetContextParameter(UPARAM(Ref) int32& Container, FName ParameterName, const int32& Value);

private:
	DECLARE_FUNCTION(execSetContextParameter);
};