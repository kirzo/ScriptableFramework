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
		const FInstancedPropertyBag* ContextToUse = nullptr;

		// 1. Determine which Context to pass down.
		// If our local context is valid and has properties, use it (it acts as the top-most scope).
		if (Context.IsValid() && Context.GetNumPropertiesInBag() > 0)
		{
			ContextToUse = &Context;
		}
		// Otherwise, try to inherit the context from the Owner (Parent Scope).
		else if (UScriptableObject* ScriptableOwner = Cast<UScriptableObject>(Owner))
		{
			ContextToUse = ScriptableOwner->GetContext();
		}

		// 2. Inject Data
		InSource->InitRuntimeData(ContextToUse, &BindingSourceMap);

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