// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableObjectAsset.generated.h"

struct FInstancedPropertyBag;

/**
 * Base class for all Scriptable Assets (Actions, Requirements, etc).
 * Centralizes editor-only logic like Menu Categories.
 */
UCLASS(Abstract, BlueprintType, Const, meta = (PrioritizeCategories = "Config"))
class SCRIPTABLEFRAMEWORK_API UScriptableObjectAsset : public UObject
{
	GENERATED_BODY()

public:
#if WITH_EDITORONLY_DATA
	/** Category used for organization in the editor picker (e.g. "Combat|Melee"). */
	UPROPERTY(EditDefaultsOnly, Category = "Config", meta = (AssetRegistrySearchable))
	FString MenuCategory;
#endif

#if WITH_EDITOR
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
#endif

	/** The context bag this asset expects, as binding discovery reads it. */
	virtual FInstancedPropertyBag* GetContext() PURE_VIRTUAL(UScriptableObjectAsset::GetContext(), return nullptr;)
};