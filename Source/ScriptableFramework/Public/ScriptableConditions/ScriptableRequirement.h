// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableContainer.h"
#include "ScriptableRequirement.generated.h"

class UScriptableCondition;
class UScriptableObject;

/** Logical operation for the requirement group. */
UENUM(BlueprintType)
enum class EScriptableRequirementMode : uint8
{
	And,
	Or
};

/** A container for a list of conditions with a logic operation (AND/OR). */
USTRUCT(BlueprintType)
struct SCRIPTABLEFRAMEWORK_API FScriptableRequirement : public FScriptableContainer
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Logic")
	EScriptableRequirementMode Mode = EScriptableRequirementMode::And;

	/** If true, the result of the entire group evaluation is inverted. */
	UPROPERTY(EditAnywhere, Category = "Logic")
	uint8 bNegate : 1 = false;

	UPROPERTY(EditAnywhere, Instanced, Category = "Conditions")
	TArray<TObjectPtr<UScriptableCondition>> Conditions;

private:
	UPROPERTY(Transient)
	uint8 bIsRegistered : 1 = false;

	// -------------------------------------------------------------------
	// API
	// -------------------------------------------------------------------
public:
	/** Registers all conditions and sets up the Binding Context. */
	void Register(UObject* InOwner);

	void Unregister();

	bool Evaluate() const;

	bool IsEmpty() const { return Conditions.IsEmpty(); }

public:
	/** Static entry point to run an action. Handles registration and startup. */
	static bool EvaluateRequirement(UObject* Owner, FScriptableRequirement& Action);
};