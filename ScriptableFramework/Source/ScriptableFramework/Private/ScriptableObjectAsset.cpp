// Copyright 2026 kirzo

#include "ScriptableObjectAsset.h"
#include "UObject/AssetRegistryTagsContext.h"

#if WITH_EDITOR
void UScriptableObjectAsset::GetAssetRegistryTags(FAssetRegistryTagsContext RegContext) const
{
	Super::GetAssetRegistryTags(RegContext);
	RegContext.AddTag(FAssetRegistryTag(GET_MEMBER_NAME_CHECKED(UScriptableObjectAsset, MenuCategory), MenuCategory, FAssetRegistryTag::TT_Alphabetical));
}
#endif