// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableNodes/ScriptableNode.h"
#include "ScriptableConditions/ScriptableRequirement.h"
#include "ScriptableNode_Branch.generated.h"

/** Evaluates a FScriptableRequirement at activation time and fires "True" or "False" accordingly. */
UCLASS(DisplayName = "Branch", meta = (NodeCategory = "System|Flow"))
class SCRIPTABLEFRAMEWORK_API UScriptableNode_Branch : public UScriptableNode
{
	GENERATED_BODY()

public:
	/** Requirement evaluated when the node is activated. Empty/unset evaluates to true, mirroring the rest of the framework's convention. */
	UPROPERTY(EditAnywhere, Category = "Branch")
	FScriptableRequirement Requirement;

	/** Canonical pin names. */
	static const FName InInputName;
	static const FName TrueOutputName;
	static const FName FalseOutputName;

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