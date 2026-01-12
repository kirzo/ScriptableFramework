// Copyright 2026 kirzo

#include "ScriptableObjectAsset.h"
#include "UObject/AssetRegistryTagsContext.h"

#if WITH_EDITOR
void UScriptableObjectAsset::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);
	Context.AddTag(FAssetRegistryTag(GET_MEMBER_NAME_CHECKED(UScriptableObjectAsset, MenuCategory), MenuCategory, FAssetRegistryTag::TT_Alphabetical));
}
#endif