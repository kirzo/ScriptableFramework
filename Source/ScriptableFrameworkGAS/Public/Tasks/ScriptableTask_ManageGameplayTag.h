// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableTasks/ScriptableTask.h"
#include "GameplayTagContainer.h"
#include "ScriptableTask_ManageGameplayTag.generated.h"

UENUM(BlueprintType)
enum class EScriptableTagOperation : uint8
{
	AddTag      UMETA(DisplayName = "Add Tag"),
	RemoveTag   UMETA(DisplayName = "Remove Tag")
};

/** Adds or removes a Loose Gameplay Tag from the target actor. */
UCLASS(DisplayName = "Add/Remove Gameplay Tag", meta = (TaskCategory = "Gameplay|Abilities"))
class SCRIPTABLEFRAMEWORKGAS_API UScriptableTask_ManageGameplayTag : public UScriptableTask
{
	GENERATED_BODY()

public:
	/** The actor whose tags will be modified. */
	UPROPERTY(EditAnywhere, Category = "Tag Operation")
	TObjectPtr<AActor> TargetActor;

	/** The tag to add or remove. */
	UPROPERTY(EditAnywhere, Category = "Tag Operation")
	FGameplayTag Tag;

	/** Whether to add or remove the tag. */
	UPROPERTY(EditAnywhere, Category = "Tag Operation")
	EScriptableTagOperation Operation = EScriptableTagOperation::AddTag;

	/** If true, calling Reset() on this task will revert the operation (e.g., removing the tag if it was added). */
	UPROPERTY(EditAnywhere, Category = "Tag Operation")
	bool bRevertOnReset = false;

protected:
	virtual void BeginTask() override;
	virtual void ResetTask() override;

private:
	/** Helper to perform the actual GAS injection/removal. */
	void ApplyTagOperation(AActor* InTarget, const FGameplayTag& InTag, EScriptableTagOperation InOperation);

#if WITH_EDITOR
	virtual FText GetDisplayTitle() const override;
#endif
};