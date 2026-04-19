// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableTasks/ScriptableTask.h"
#include "GameplayTagContainer.h"
#include "ScriptableTask_ManageGameplayTag.generated.h"

UENUM(BlueprintType)
enum class EScriptableTagOperation : uint8
{
	AddTags, RemoveTags
};

/** Adds or removes multiple Loose Gameplay Tags from the target actor. */
UCLASS(DisplayName = "Add/Remove Gameplay Tags", meta = (TaskCategory = "Gameplay|Abilities"))
class SCRIPTABLEFRAMEWORKGAS_API UScriptableTask_ManageGameplayTag : public UScriptableTask
{
	GENERATED_BODY()

public:
	/** The actor whose tags will be modified. */
	UPROPERTY(EditAnywhere, Category = "Tag Operation", meta = (ScriptableContext))
	TObjectPtr<AActor> TargetActor;

	/** The container of tags to add or remove. */
	UPROPERTY(EditAnywhere, Category = "Tag Operation")
	FGameplayTagContainer Tags;

	/** Whether to add or remove the tags. */
	UPROPERTY(EditAnywhere, Category = "Tag Operation")
	EScriptableTagOperation Operation = EScriptableTagOperation::AddTags;

	/** If true, calling Reset() on this task will revert the operation (e.g., removing the tags if they were added). */
	UPROPERTY(EditAnywhere, Category = "Tag Operation")
	bool bRevertOnReset = false;

protected:
	virtual void BeginTask() override;
	virtual void ResetTask() override;

private:
	/** Helper to perform the actual GAS injection/removal. */
	void ApplyTagOperation(AActor* InTarget, const FGameplayTagContainer& InTags, EScriptableTagOperation InOperation);

#if WITH_EDITOR
	virtual FText GetDisplayTitle() const override;
#endif
};