// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "Components/KzComponentReference.h"
#include "LimbicTarget.h"
#include "Misc/KzTransformSource.h"
#include "ScriptableTasks/ScriptableTask.h"
#include "ScriptableTask_MakeLimbicTarget.generated.h"

/** Builds a target for a Set Limbic Slot after it to bind its target parameter to: a component or socket followed as it moves, or a fixed transform. */
UCLASS(DisplayName = "Make Limbic Target", meta = (TaskCategory = "Animation|Limbic"))
class SCRIPTABLEFRAMEWORKLIMBIC_API UScriptableTask_MakeLimbicTarget : public UScriptableTask
{
	GENERATED_BODY()

public:
	/** What to follow, when bound: a component or socket with its offset, an actor's root, or a fixed transform in the world. */
	UPROPERTY(EditAnywhere, Category = "Target")
	FKzTransformSource Source;

	/** The actor whose component ComponentReference names, when Source is not bound. */
	UPROPERTY(EditAnywhere, Category = "Target")
	TObjectPtr<AActor> Actor;

	/** A component of Actor, with its socket and offset, when Source is not bound. */
	UPROPERTY(EditAnywhere, Category = "Target")
	FKzComponentSocketReference ComponentReference;

	/** The target built, unset when there is nothing to follow. */
	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category = "Output")
	FLimbicTarget Target;

	virtual bool IsStoppable() const override { return false; }

protected:
	virtual void BeginTask() override;
};