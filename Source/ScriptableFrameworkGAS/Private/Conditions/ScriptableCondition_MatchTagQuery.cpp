// Copyright 2026 kirzo

#include "Conditions/ScriptableCondition_MatchTagQuery.h"
#include "GameplayTagAssetInterface.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

bool UScriptableCondition_MatchTagQuery::Evaluate_Implementation() const
{
	// Ensure we have a valid actor and the query actually has rules
	if (!IsValid(TargetActor) || TagQuery.IsEmpty())
	{
		return false;
	}

	const IGameplayTagAssetInterface* GameplayTagAssetInterface = Cast<IGameplayTagAssetInterface>(TargetActor);
	if (GameplayTagAssetInterface == nullptr)
	{
		if (const UAbilitySystemComponent* const AbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor))
		{
			GameplayTagAssetInterface = AbilitySystemComponent;
		}
	}

	if (GameplayTagAssetInterface != nullptr)
	{
		FGameplayTagContainer Tags;
		GameplayTagAssetInterface->GetOwnedGameplayTags(Tags);
		return TagQuery.Matches(Tags);
	}

	return false;
}

#if WITH_EDITOR
FText UScriptableCondition_MatchTagQuery::GetDisplayTitle() const
{
	FString TargetName;
	if (!GetBindingDisplayText(GET_MEMBER_NAME_CHECKED(UScriptableCondition_MatchTagQuery, TargetActor), TargetName))
	{
		TargetName = TargetActor ? TargetActor->GetActorLabel() : TEXT("None");
	}

	if (!TagQuery.IsEmpty())
	{
		// FGameplayTagQuery::GetDescription() automatically creates a readable string like "ALL(State.Grabbing, State.Burning)"
		return FText::Format(INVTEXT("{0} Matches Query [{1}]"), FText::FromString(TargetName), FText::FromString(TagQuery.GetDescription()));
	}

	return INVTEXT("Match Tag Query");
}
#endif