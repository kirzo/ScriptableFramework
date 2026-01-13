// Copyright 2026 kirzo

#include "ScriptableObjectAsset.h"
#include "UObject/AssetRegistryTagsContext.h"

UE_DISABLE_OPTIMIZATION

#if WITH_EDITOR
void UScriptableObjectAsset::GetAssetRegistryTags(FAssetRegistryTagsContext RegContext) const
{
	Super::GetAssetRegistryTags(RegContext);
	RegContext.AddTag(FAssetRegistryTag(GET_MEMBER_NAME_CHECKED(UScriptableObjectAsset, MenuCategory), MenuCategory, FAssetRegistryTag::TT_Alphabetical));
}

void UScriptableObjectAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GetContainerName())
	{
		RefreshContext();
	}
}

void UScriptableObjectAsset::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GetContainerName())
	{
		RefreshContext();
	}
}

void UScriptableObjectAsset::RefreshContext()
{
	if (FInstancedPropertyBag* ContextRef = GetContext())
	{
		ContextRef->Reset();
		for (const FScriptableParameterDef& ScriptableParam : Context)
		{
			if (ScriptableParam.IsValid())
			{
				ContextRef->AddContainerProperty(ScriptableParam.Name, ScriptableParam.ContainerType, ScriptableParam.ValueType, ScriptableParam.ValueTypeObject);
			}
		}
	}
}
#endif

UE_ENABLE_OPTIMIZATION