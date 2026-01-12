// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableObjectAsset.generated.h"

/**
 * Base class for all Scriptable Assets (Actions, Requirements, etc).
 * Centralizes editor-only logic like Menu Categories.
 */
UCLASS(Abstract, BlueprintType, Const, meta = (PrioritizeCategories = "Editor"))
class SCRIPTABLEFRAMEWORK_API UScriptableObjectAsset : public UObject
{
	GENERATED_BODY()

public:
#if WITH_EDITORONLY_DATA
	/** Category used for organization in the editor picker (e.g. "Combat|Melee"). */
	UPROPERTY(EditDefaultsOnly, Category = "Editor", meta = (AssetRegistrySearchable))
	FString MenuCategory;
#endif

#if WITH_EDITOR
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
#endif
};