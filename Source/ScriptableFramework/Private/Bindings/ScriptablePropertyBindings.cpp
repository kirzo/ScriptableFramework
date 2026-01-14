// Copyright 2025 kirzo

#include "Bindings/ScriptablePropertyBindings.h"
#include "ScriptableObject.h"

#if WITH_EDITOR
void FScriptablePropertyBindings::AddPropertyBinding(const FPropertyBindingPath& SourcePath, const FPropertyBindingPath& TargetPath)
{
	// If a binding already exists for this target, update it
	for (FScriptablePropertyBinding& Binding : Bindings)
	{
		if (Binding.TargetPath == TargetPath)
		{
			Binding.SourcePath = SourcePath;
			Binding.SourceID = SourcePath.GetStructID();
			return;
		}
	}

	// Otherwise, create a new one
	FScriptablePropertyBinding& NewBinding = Bindings.AddDefaulted_GetRef();
	NewBinding.SourcePath = SourcePath;
	NewBinding.TargetPath = TargetPath;
	NewBinding.SourceID = SourcePath.GetStructID();
}

void FScriptablePropertyBindings::RemovePropertyBindings(const FPropertyBindingPath& TargetPath)
{
	Bindings.RemoveAll([&TargetPath](const FScriptablePropertyBinding& Binding)
	{
		return Binding.TargetPath == TargetPath;
	});
}

bool FScriptablePropertyBindings::HasPropertyBinding(const FPropertyBindingPath& TargetPath) const
{
	return Bindings.ContainsByPredicate([&TargetPath](const FScriptablePropertyBinding& Binding)
	{
		return Binding.TargetPath == TargetPath;
	});
}

void FScriptablePropertyBindings::HandleArrayElementRemoved(const FName& ArrayName, int32 IndexRemoved)
{
	if (IndexRemoved < 0) return;

	// Iterate backwards to safely remove elements while iterating
	for (int32 i = Bindings.Num() - 1; i >= 0; --i)
	{
		FScriptablePropertyBinding& Binding = Bindings[i];

		// Check if the Target path is valid and starts with the modified Array
		if (Binding.TargetPath.NumSegments() > 0)
		{
			// We get a mutable view of the segments to modify the index directly
			TArrayView<FPropertyBindingPathSegment> Segments = Binding.TargetPath.GetMutableSegments();
			FPropertyBindingPathSegment& RootSegment = Segments[0];

			if (RootSegment.GetName() == ArrayName)
			{
				const int32 BindingIndex = RootSegment.GetArrayIndex();

				// CASE 1: The binding belongs to the removed element -> REMOVE
				if (BindingIndex == IndexRemoved)
				{
					Bindings.RemoveAt(i);
				}
				// CASE 2: The binding is above the removed index -> SHIFT DOWN
				// E.g., If we remove [0], the binding pointing to [1] must now point to [0]
				else if (BindingIndex > IndexRemoved)
				{
					RootSegment.SetArrayIndex(BindingIndex - 1);
				}
			}
		}
	}
}

void FScriptablePropertyBindings::HandleArrayClear(const FName& ArrayName)
{
	Bindings.RemoveAll([&ArrayName](const FScriptablePropertyBinding& Binding)
	{
		if (Binding.TargetPath.NumSegments() > 0)
		{
			const FPropertyBindingPathSegment& RootSegment = Binding.TargetPath.GetSegment(0);
			return RootSegment.GetName() == ArrayName;
		}
		return false;
	});
}

const FPropertyBindingPath* FScriptablePropertyBindings::GetPropertyBinding(const FPropertyBindingPath& TargetPath) const
{
	const FScriptablePropertyBinding* FoundBinding = Bindings.FindByPredicate([&TargetPath](const FScriptablePropertyBinding& Binding)
	{
		return Binding.TargetPath == TargetPath;
	});

	return FoundBinding ? &FoundBinding->SourcePath : nullptr;
}
#endif

void FScriptablePropertyBindings::ResolveBindings(UScriptableObject* TargetObject)
{
	if (!TargetObject) return;

	// Prepare the Context View in advance (it might be used by multiple bindings)
	const FInstancedPropertyBag* Context = TargetObject->GetContext();

	// Prepare Context View
	FPropertyBindingDataView ContextView;
	if (Context && Context->IsValid())
	{
		ContextView = FPropertyBindingDataView(Context->GetPropertyBagStruct(), const_cast<FInstancedPropertyBag*>(Context)->GetMutableValue().GetMemory());
	}

	// The Target View is always the object requesting the resolution
	FPropertyBindingDataView TargetView(TargetObject);

	for (const FScriptablePropertyBinding& Binding : Bindings)
	{
		// Determine the Source Data View (Who are we copying FROM?)
		FPropertyBindingDataView SourceView;
		if (Binding.SourceID.IsValid())
		{
			// CASE A: Sibling Binding
			// Direct lookup via the injected map in TargetObject
			if (UScriptableObject* SourceObj = TargetObject->FindBindingSource(Binding.SourceID))
			{
				SourceView = FPropertyBindingDataView(SourceObj);
			}
			else
			{
				// Source object not found (e.g., was deleted or not loaded yet). Skip binding.
				continue;
			}
		}
		else
		{
			// CASE B: Context Binding
			SourceView = ContextView;
		}

		// Perform the Copy
		if (SourceView.IsValid())
		{
			CopySingleBinding(Binding, SourceView, TargetView);
		}
	}
}

void FScriptablePropertyBindings::CopySingleBinding(const FScriptablePropertyBinding& Binding, const FPropertyBindingDataView& SrcView, const FPropertyBindingDataView& DestView)
{
	TArray<FPropertyBindingPathIndirection> SourceIndirections;
	if (!Binding.SourcePath.ResolveIndirectionsWithValue(SrcView, SourceIndirections)) return;

	TArray<FPropertyBindingPathIndirection> TargetIndirections;
	if (!Binding.TargetPath.ResolveIndirectionsWithValue(DestView, TargetIndirections)) return;

	const FPropertyBindingPathIndirection& SourceLeaf = SourceIndirections.Last();
	const FPropertyBindingPathIndirection& TargetLeaf = TargetIndirections.Last();

	const FProperty* SourceProp = SourceLeaf.GetProperty();
	const FProperty* TargetProp = TargetLeaf.GetProperty();
	const void* SourceAddr = SourceLeaf.GetPropertyAddress();
	void* TargetAddr = TargetLeaf.GetMutablePropertyAddress();

	if (SourceProp && TargetProp && SourceAddr && TargetAddr)
	{
		// Identical Types (Fast Copy)
		if (SourceProp->SameType(TargetProp))
		{
			SourceProp->CopyCompleteValue(TargetAddr, SourceAddr);
		}
		else
		{
			// Object Reference Handling (TObjectPtr <-> Raw Ptr, Child -> Parent)
			if (const FObjectPropertyBase* SrcObjProp = CastField<FObjectPropertyBase>(SourceProp))
			{
				// TObjectPtr <-> Raw Ptr
				if (const FObjectPropertyBase* TgtObjProp = CastField<FObjectPropertyBase>(TargetProp))
				{
					// This gets the UObject* regardless of whether it's stored as TObjectPtr or raw pointer
					UObject* SourceObject = SrcObjProp->GetObjectPropertyValue(SourceAddr);

					if (!SourceObject || SourceObject->IsA(TgtObjProp->PropertyClass))
					{
						TgtObjProp->SetObjectPropertyValue(TargetAddr, SourceObject);
					}
				}
				// Object -> Bool
				else if (const FBoolProperty* TgtBool = CastField<FBoolProperty>(TargetProp))
				{
					// Get the pointer (works for TObjectPtr and raw pointers)
					const UObject* SourceObject = SrcObjProp->GetObjectPropertyValue(SourceAddr);

					// True if not null, False if null
					TgtBool->SetPropertyValue(TargetAddr, SourceObject != nullptr);
				}
			}
			// Numeric <-> Numeric Conversion
			else if (SourceProp->IsA<FNumericProperty>() && TargetProp->IsA<FNumericProperty>())
			{
				const FNumericProperty* SrcNum = CastField<FNumericProperty>(SourceProp);
				const FNumericProperty* TgtNum = CastField<FNumericProperty>(TargetProp);

				if (SrcNum->IsFloatingPoint())
				{
					const double Val = SrcNum->GetFloatingPointPropertyValue(SourceAddr);
					if (TgtNum->IsFloatingPoint()) TgtNum->SetFloatingPointPropertyValue(TargetAddr, Val);
					else TgtNum->SetIntPropertyValue(TargetAddr, (int64)Val);
				}
				else
				{
					const int64 Val = SrcNum->GetSignedIntPropertyValue(SourceAddr);
					if (TgtNum->IsFloatingPoint()) TgtNum->SetFloatingPointPropertyValue(TargetAddr, (double)Val);
					else TgtNum->SetIntPropertyValue(TargetAddr, Val);
				}
			}
			// Bool -> Numeric (True=1, False=0)
			else if (const FBoolProperty* SrcBool = CastField<FBoolProperty>(SourceProp))
			{
				if (const FNumericProperty* TgtNum = CastField<FNumericProperty>(TargetProp))
				{
					const bool bVal = SrcBool->GetPropertyValue(SourceAddr);
					if (TgtNum->IsFloatingPoint()) TgtNum->SetFloatingPointPropertyValue(TargetAddr, bVal ? 1.0 : 0.0);
					else TgtNum->SetIntPropertyValue(TargetAddr, int64(bVal ? 1 : 0));
				}
			}
			// Numeric -> Bool (0=False, !=0 True)
			else if (const FNumericProperty* SrcNum = CastField<FNumericProperty>(SourceProp))
			{
				if (const FBoolProperty* TgtBool = CastField<FBoolProperty>(TargetProp))
				{
					bool bResult = false;
					if (SrcNum->IsFloatingPoint()) bResult = !FMath::IsNearlyZero(SrcNum->GetFloatingPointPropertyValue(SourceAddr));
					else bResult = (SrcNum->GetSignedIntPropertyValue(SourceAddr) != 0);

					TgtBool->SetPropertyValue(TargetAddr, bResult);
				}
			}
		}
	}
}