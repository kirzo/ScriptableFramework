// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableConditions/ScriptableCondition.h"
#include "ScriptableConditions/ScriptableRequirement.h"
#include "ScriptableCondition_Group.generated.h"

/**
 * A grouping condition that creates a nested scope with its own logic (AND/OR).
 * It holds a local Context and a list of sub-conditions.
 */
UCLASS(meta = (DisplayName = "Nested Group", ConditionCategory = "System"))
class SCRIPTABLEFRAMEWORK_API UScriptableCondition_Group : public UScriptableCondition
{
	GENERATED_BODY()

public:
	/** Optional descriptive name for this group (e.g. "Ammo Checks"). */
	UPROPERTY(EditAnywhere, Category = "Group", meta = (NoBinding))
	FString GroupName;

	/** The nested logic block (Conditions + Context + Mode). */
	UPROPERTY(EditAnywhere, Category = "Group", meta = (ShowOnlyInnerProperties, NoBinding))
	FScriptableRequirement Requirement;

#if WITH_EDITOR
	virtual FText GetDescription() const override;
#endif

	// Lifecycle Forwarding
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

protected:
	virtual bool Evaluate_Implementation() const override;
};