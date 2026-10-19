// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableNodes/ScriptableNode.h"
#include "ScriptableConditions/ScriptableRequirement.h"
#include "ScriptableNode_Switch.generated.h"

/**
 * Evaluates an ordered list of requirements and fires the output of the first that passes, or
 * "Default" if none do. Like a C++ switch but with arbitrary predicates; collapses nested Branches
 * in mutually-exclusive decision trees. Fires exactly one output per activation.
 */
UCLASS(DisplayName = "Switch", meta = (NodeCategory = "System|Flow"))
class SCRIPTABLEFRAMEWORK_API UScriptableNode_Switch : public UScriptableNode
{
	GENERATED_BODY()

public:
	/** Ordered cases. The first that evaluates true fires its "Case <index>" output. */
	UPROPERTY(EditAnywhere, Category = "Switch")
	TArray<FScriptableRequirement> Cases;

	/** Canonical name of the input pin. */
	static const FName InInputName;

	/** Canonical name of the fallback output, fired when no case passes. */
	static const FName DefaultOutputName;

	/** Builds the output pin name for a case index ("Case 0", "Case 1", ...). */
	static FName MakeCaseOutputName(int32 CaseIndex);

	//~ UScriptableNode interface
	virtual TArray<FName> GetInputPins() const override;
	virtual TArray<FName> GetDeclaredOutputPins() const override;
	//~ End of UScriptableNode interface

	//~ UScriptableObject interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	//~ End of UScriptableObject interface

protected:
	//~ UScriptableNode interface
	virtual void ProcessInput(FName InputName) override;
	//~ End of UScriptableNode interface
};
