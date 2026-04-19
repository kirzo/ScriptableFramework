// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableCondition.h"
#include "ScriptableCondition_Logic.generated.h"

UENUM(BlueprintType)
enum class EScriptableBoolOp : uint8
{
	And             UMETA(DisplayName = "AND"),
	Or              UMETA(DisplayName = "OR"),
	Xor             UMETA(DisplayName = "XOR"),
	Nand            UMETA(DisplayName = "NAND"),
	Equal           UMETA(DisplayName = "=="),
	NotEqual        UMETA(DisplayName = "!=")
};

/** Basic condition that returns the value of a boolean property (from context or static). */
UCLASS(DisplayName = "Bool Check", meta = (ConditionCategory = "System|Logic"))
class SCRIPTABLEFRAMEWORK_API UScriptableCondition_Bool : public UScriptableCondition
{
	GENERATED_BODY()

public:
	/** The boolean value to check. Bind this to a Context variable. */
	UPROPERTY(EditAnywhere, Category = "Config")
	bool bValue = true;

#if WITH_EDITOR
	virtual FText GetDisplayTitle() const override;
#endif

protected:
	virtual bool Evaluate_Implementation() const override;
};

/** System condition to compare two boolean values with logical operators. */
UCLASS(DisplayName = "Compare Booleans", meta = (ConditionCategory = "System|Logic"))
class SCRIPTABLEFRAMEWORK_API UScriptableCondition_CompareBooleans : public UScriptableCondition
{
	GENERATED_BODY()

public:
	/** The first boolean value */
	UPROPERTY(EditAnywhere, Category = "Config")
	bool bA = false;

	/** The second boolean value */
	UPROPERTY(EditAnywhere, Category = "Config")
	bool bB = false;

	/** The logical operator to apply */
	UPROPERTY(EditAnywhere, Category = "Config")
	EScriptableBoolOp Operation = EScriptableBoolOp::And;

#if WITH_EDITOR
	virtual FText GetDisplayTitle() const override;
#endif

protected:
	virtual bool Evaluate_Implementation() const override;
};

/**
 * System condition that returns true based on a percentage chance.
 * Useful for non-deterministic behavior (e.g., 30% chance to attack).
 */
UCLASS(DisplayName = "Probability", meta = (ConditionCategory = "System|Logic"))
class SCRIPTABLEFRAMEWORK_API UScriptableCondition_Probability : public UScriptableCondition
{
	GENERATED_BODY()

public:
	/** The probability to return true (0 to 1) */
	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = 0, ClampMax = 1, UIMin = 0, UIMax = 1))
	float Chance = 0.5f;

#if WITH_EDITOR
	virtual FText GetDisplayTitle() const override;
#endif

protected:
	virtual bool Evaluate_Implementation() const override;
};

/** Checks if a UObject is valid (not null and not pending kill). */
UCLASS(DisplayName = "Is Valid", meta = (ConditionCategory = "System|Object"))
class SCRIPTABLEFRAMEWORK_API UScriptableCondition_IsValid : public UScriptableCondition
{
	GENERATED_BODY()

public:
	/** The object to validate. */
	UPROPERTY(EditAnywhere, Category = "Config", meta = (ScriptableContext))
	TObjectPtr<UObject> TargetObject = nullptr;

#if WITH_EDITOR
	virtual FText GetDisplayTitle() const override;
#endif

protected:
	virtual bool Evaluate_Implementation() const override;
};