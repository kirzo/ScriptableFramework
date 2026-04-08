// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ScriptableTasks/ScriptableAction.h"
#include "ScriptableConditions/ScriptableRequirement.h"
#include "ScriptableBlueprintLibrary.generated.h"

UCLASS()
class SCRIPTABLEFRAMEWORK_API UScriptableBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Sets a context parameter value by name.
	 * @param Action         The scriptable action to modify.
	 * @param ParameterName  The name of the parameter (must match ContextDefinitions).
	 * @param Value          The value to set. Type must match the definition.
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "Scriptable Framework|Context", meta = (CustomStructureParam = "Value", AutoCreateRefTerm = "Value"))
	static void SetActionContextParameter(UPARAM(Ref) FScriptableAction& Action, FName ParameterName, const int32& Value);

	/**
	 * Sets a context parameter value by name.
	 * @param Requirement    The scriptable requirement to modify.
	 * @param ParameterName  The name of the parameter (must match ContextDefinitions).
	 * @param Value          The value to set. Type must match the definition.
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "Scriptable Framework|Context", meta = (CustomStructureParam = "Value", AutoCreateRefTerm = "Value"))
	static void SetRequirementContextParameter(UPARAM(Ref) FScriptableRequirement& Requirement, FName ParameterName, const int32& Value);

private:
	DECLARE_FUNCTION(execSetActionContextParameter);
	DECLARE_FUNCTION(execSetRequirementContextParameter);
};