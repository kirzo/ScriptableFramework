// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableObjectAsset.h"
#include "ScriptableRequirement.h"
#include "ScriptableCondition.h"
#include "ScriptableRequirementAsset.generated.h"

/** An asset that defines a reusable Requirement. */
UCLASS(BlueprintType, Const)
class SCRIPTABLEFRAMEWORK_API UScriptableRequirementAsset final : public UScriptableObjectAsset
{
	GENERATED_BODY()

public:
	/** The requirement definition. Holds the Context variables and the list of Conditions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FScriptableRequirement Requirement;
};

UCLASS(EditInlineNew, BlueprintType, NotBlueprintable, meta = (DisplayName = "Evaluate Asset", Hidden))
class SCRIPTABLEFRAMEWORK_API UScriptableCondition_Asset final : public UScriptableCondition
{
	GENERATED_BODY()

public:
	/** The asset containing the Requirement definition (Context + Conditions) to evaluate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ScriptableCondition)
	class UScriptableRequirementAsset* Asset;

protected:
	/** The actual instance created from the asset template. */
	UPROPERTY(Transient)
	UScriptableCondition* Condition;
};