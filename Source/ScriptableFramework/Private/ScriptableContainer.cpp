// Copyright 2026 kirzo

#include "ScriptableContainer.h"
#include "ScriptableObject.h"

UE_DISABLE_OPTIMIZATION

UScriptableObject* FScriptableContainer::FindBindingSource(const FGuid& InID) const
{
	if (const TObjectPtr<UScriptableObject>* Found = BindingSourceMap.Find(InID))
	{
		return Found->Get();
	}
	return nullptr;
}

void FScriptableContainer::AddBindingSource(UScriptableObject* InSource)
{
	if (InSource)
	{
		InSource->InitRuntimeData(&Context, &BindingSourceMap);

		FGuid ID = InSource->GetBindingID();
		if (ID.IsValid())
		{
			BindingSourceMap.Add(ID, InSource);
		}
	}
}

void FScriptableContainer::Register(UObject* InOwner)
{
	Owner = InOwner;
	BindingSourceMap.Reset(); // Clean slate
}

void FScriptableContainer::Unregister()
{
	BindingSourceMap.Empty();
	Owner = nullptr;
}

UE_ENABLE_OPTIMIZATION