// Copyright 2026 kirzo

#include "ScriptableContainer.h"
#include "ScriptableContext.h"
#include "ScriptableObject.h"
#include "ScriptableRuntimeData.h"
#include "ScriptablePropertyUtilities.h"
#include "Core/KzBagOps.h"
#include "UObject/UnrealType.h"

void FScriptableContainer::ConstructContext()
{
	ResetContext();
	KzBagOps::AddProperties(Context, ContextDefinitions);
}

bool FScriptableContainer::IsContextBagOutOfSync() const
{
	const UPropertyBag* BagStruct = Context.GetPropertyBagStruct();
	const int32 BagPropertyCount = BagStruct ? BagStruct->GetPropertyDescs().Num() : 0;
	if (BagPropertyCount != ContextDefinitions.Num()) return true;

	if (BagStruct)
	{
		for (const FKzParamDef& Def : ContextDefinitions)
		{
			if (!BagStruct->FindPropertyDescByName(Def.Name)) return true;
		}
	}
	return false;
}

void FScriptableContainer::ConstructLocals()
{
	ResetLocals();

	TArray<FPropertyBagPropertyDesc> Descs;
	Descs.Reserve(LocalsDefinitions.Num());
	for (const FKzNamedVariant& Var : LocalsDefinitions)
	{
		if (!Var.IsValid()) continue;
		Descs.Add(Var.ToPropertyDesc());
	}
	if (Descs.Num() > 0)
	{
		Locals.AddProperties(Descs);
		for (const FKzNamedVariant& Var : LocalsDefinitions)
		{
			if (!Var.IsValid()) continue;
			Var.WriteToBag(Locals);
		}
	}
}

bool FScriptableContainer::IsLocalsBagOutOfSync() const
{
	int32 ValidDefsCount = 0;
	for (const FKzNamedVariant& Var : LocalsDefinitions)
	{
		if (Var.IsValid()) ++ValidDefsCount;
	}

	const UPropertyBag* BagStruct = Locals.GetPropertyBagStruct();
	const int32 BagPropertyCount = BagStruct ? BagStruct->GetPropertyDescs().Num() : 0;
	if (BagPropertyCount != ValidDefsCount) return true;

	if (BagStruct)
	{
		for (const FKzNamedVariant& Var : LocalsDefinitions)
		{
			if (!Var.IsValid()) continue;
			if (!BagStruct->FindPropertyDescByName(Var.GetName())) return true;
		}
	}
	return false;
}

void FScriptableContainer::AddContext(const FScriptableContext& InContext)
{
	const UPropertyBag* BagStruct = InContext.GetBag().GetPropertyBagStruct();
	if (!BagStruct)
	{
		return;
	}

	for (const FPropertyBagPropertyDesc& Desc : BagStruct->GetPropertyDescs())
	{
		ContextDefinitions.Add(FKzParamDef(Desc.Name, Desc.ContainerTypes.GetFirstContainerType(), Desc.ValueType, Desc.ValueTypeObject));
	}

	ConstructContext();
}

void FScriptableContainer::SetContext(const FScriptableContext& InContext)
{
	const FInstancedPropertyBag& SourceBag = InContext.GetBag();
	const UPropertyBag* SourceStruct = SourceBag.GetPropertyBagStruct();
	if (!SourceStruct) return;

	const UPropertyBag* TargetStruct = Context.GetPropertyBagStruct();
	if (!TargetStruct)
	{
		// Local bag is empty (no ContextDefinitions on this container). Adopt the source bag
		// wholesale — there's no authored shape to preserve.
		Context = SourceBag;
		return;
	}

	const uint8* SourceMemory = SourceBag.GetValue().GetMemory();
	uint8* TargetMemory = Context.GetMutableValue().GetMemory();
	if (!SourceMemory || !TargetMemory) return;

	for (const FPropertyBagPropertyDesc& SourceDesc : SourceStruct->GetPropertyDescs())
	{
		if (!SourceDesc.CachedProperty) continue;
		const FPropertyBagPropertyDesc* TargetDesc = TargetStruct->FindPropertyDescByName(SourceDesc.Name);
		if (!TargetDesc || !TargetDesc->CachedProperty) continue;
		if (!TargetDesc->CachedProperty->SameType(SourceDesc.CachedProperty)) continue;

		void* TargetAddr = TargetDesc->CachedProperty->ContainerPtrToValuePtr<void>(TargetMemory);
		const void* SourceAddr = SourceDesc.CachedProperty->ContainerPtrToValuePtr<void>(SourceMemory);
		TargetDesc->CachedProperty->CopyCompleteValue(TargetAddr, SourceAddr);
	}
}

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
		// A wrapper-injected bag wins: it is the parent scope's context.
		if (InheritedContextBag)
		{
			ContextToUse = InheritedContextBag;
		}
		// If our local context is valid and has properties, use it (it acts as the top-most scope).
		else if (Context.IsValid() && Context.GetNumPropertiesInBag() > 0)
		{
			ContextToUse = &Context;
		}
		// Otherwise, try to inherit the context from the Owner (Parent Scope).
		else if (UScriptableObject* ScriptableOwner = Cast<UScriptableObject>(Owner))
		{
			ContextToUse = ScriptableOwner->GetContext();
		}

		// 2. Inject Data. Locals are handed down when this container declares its own OR has an inherited
		// bag from a wrapper (nested action / requirement); a null pointer leaves the child without locals.
		FScriptableRuntimeData Data;
		Data.Context = ContextToUse;
		if (LocalsDefinitions.Num() > 0 || InheritedLocalsBag != nullptr)
		{
			Data.Locals = &GetLocals();
		}
		Data.BindingsMap = &BindingSourceMap;
		InSource->InitRuntimeData(Data);

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
	InheritedLocalsBag = nullptr;
	InheritedContextBag = nullptr;
	BindingSourceMap.Empty();
	Owner = nullptr;
}