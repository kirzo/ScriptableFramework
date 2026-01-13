// Copyright 2025 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableObject.h"
#include "ScriptableCondition.generated.h"

UCLASS(Abstract, DefaultToInstanced, EditInlineNew, Blueprintable, BlueprintType, HideCategories = (Hidden, Tick), CollapseCategories)
class SCRIPTABLEFRAMEWORK_API UScriptableCondition : public UScriptableObject
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = Hidden, meta = (NoBinding))
	uint8 bNegate : 1 = 0;

public:
	FORCEINLINE bool IsNegated() const { return bNegate; }

	/** Conditions should typically be instant checks, not ticking objects. */
	virtual bool CanEverTick() const final override { return false; }

#if WITH_EDITOR
	/**
	 * Returns a user-friendly description of this condition.
	 * Used by the Editor to display the condition in lists (e.g. "Health > 0").
	 */
	virtual FText GetDescription() const;
#endif

	/**
	 * Main entry point for evaluation.
	 * Handles Binding Resolution and Negation logic.
	 */
	bool CheckCondition();

protected:
	/**
	 * Implementation of the specific condition check.
	 * Do NOT handle Negation or Bindings here; just return the raw result.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = ScriptableCondition)
	bool Evaluate() const;

private:
	virtual bool Evaluate_Implementation() const { return false; }
};