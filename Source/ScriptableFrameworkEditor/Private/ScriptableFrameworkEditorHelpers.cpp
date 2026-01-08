// Copyright 2026 kirzo

#include "ScriptableFrameworkEditorHelpers.h"
#include "PropertyHandle.h"
#include "ScriptableObject.h"
#include "ScriptableTasks/ScriptableTask.h"
#include "ScriptableTasks/ScriptableAction.h"
#include "ScriptableConditions/ScriptableCondition.h"
#include "StructUtils/InstancedStruct.h"
#include "IPropertyAccessEditor.h"

UE_DISABLE_OPTIMIZATION

namespace ScriptableObjectTraversal
{
	/** Checks if the Parent class allows data binding between its children (Siblings). */
	static bool AreSiblingBindingsAllowed(const UObject* ParentObject)
	{
		if (!ParentObject) return false;
		return !ParentObject->GetClass()->HasMetaData(TEXT("BlockSiblingBindings"));
	}

	/** Scans the ParentObject for any Array Property that contains the CurrentChild. */
	static void CollectPreviousSiblings(const UObject* ParentObject, const UObject* CurrentChild, TArray<const UScriptableObject*>& OutObjects)
	{
		if (!ParentObject || !CurrentChild) return;

		for (TFieldIterator<FArrayProperty> PropIt(ParentObject->GetClass()); PropIt; ++PropIt)
		{
			FArrayProperty* ArrayProp = *PropIt;
			FObjectProperty* InnerProp = CastField<FObjectProperty>(ArrayProp->Inner);

			if (InnerProp && InnerProp->PropertyClass->IsChildOf(UScriptableObject::StaticClass()))
			{
				FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(ParentObject));
				bool bFoundCurrentChildInArray = false;
				TArray<const UScriptableObject*> PotentialSiblings;

				for (int32 i = 0; i < Helper.Num(); ++i)
				{
					UObject* Item = InnerProp->GetObjectPropertyValue(Helper.GetRawPtr(i));
					if (Item == CurrentChild)
					{
						bFoundCurrentChildInArray = true;
						break;
					}
					if (const UScriptableObject* ScriptableItem = Cast<UScriptableObject>(Item))
					{
						PotentialSiblings.Add(ScriptableItem);
					}
				}

				if (bFoundCurrentChildInArray)
				{
					OutObjects.Append(PotentialSiblings);
					return;
				}
			}
		}
	}
}

namespace ScriptableFrameworkEditor
{
	bool IsPropertyVisible(TSharedRef<IPropertyHandle> PropertyHandle)
	{
		if (FProperty* Property = PropertyHandle->GetProperty())
		{
			if (Property->GetMetaData(TEXT("Category")) == TEXT("Hidden")) return false;
			if (Property->HasAllPropertyFlags(CPF_Edit | CPF_DisableEditOnInstance))
			{
				if (UObject* Object = Property->GetOwnerUObject())
				{
					return Object->IsTemplate();
				}
			}
		}
		return true;
	}

	void GetScriptableCategory(const UClass* ScriptableClass, FName& ClassCategoryMeta, FName& PropertyCategoryMeta)
	{
		if (ScriptableClass->IsChildOf(UScriptableTask::StaticClass()))
		{
			ClassCategoryMeta = MD_TaskCategory;
			PropertyCategoryMeta = MD_TaskCategories;
		}
		else if (ScriptableClass->IsChildOf(UScriptableCondition::StaticClass()))
		{
			ClassCategoryMeta = MD_ConditionCategory;
			PropertyCategoryMeta = MD_ConditionCategories;
		}
	}

	bool IsPropertyBindableOutput(const FProperty* Property)
	{
		if (!Property) return false;
		if (Property->HasMetaData(TEXT("ScriptableOutput"))) return true;
		const FString Category = Property->GetMetaData(TEXT("Category"));
		return Category.StartsWith(TEXT("Output"));
	}

	bool ArePropertiesCompatible(const FProperty* SourceProp, const FProperty* TargetProp)
	{
		if (!SourceProp || !TargetProp) return false;

		// 1. Base Case: Identical Types
		if (SourceProp->SameType(TargetProp))
		{
			return true;
		}

		// Arrays
		if (const FArrayProperty* SourceArray = CastField<FArrayProperty>(SourceProp))
		{
			if (const FArrayProperty* TargetArray = CastField<FArrayProperty>(TargetProp))
			{
				const FObjectPropertyBase* SourceInner = CastField<FObjectPropertyBase>(SourceArray->Inner);
				const FObjectPropertyBase* TargetInner = CastField<FObjectPropertyBase>(TargetArray->Inner);

				if (SourceInner && TargetInner)
				{
					return SourceInner->PropertyClass->IsChildOf(TargetInner->PropertyClass);
				}
				return false;
			}
			return false;
		}
		if (TargetProp->IsA<FArrayProperty>()) return false;

		// 2. Objects: Covariance
		if (const FObjectPropertyBase* SourceObj = CastField<FObjectPropertyBase>(SourceProp))
		{
			if (const FObjectPropertyBase* TargetObj = CastField<FObjectPropertyBase>(TargetProp))
			{
				return SourceObj->PropertyClass->IsChildOf(TargetObj->PropertyClass);
			}
		}

		// 3. Numeric Promotions
		const bool bSourceIsReal = SourceProp->IsA<FFloatProperty>() || SourceProp->IsA<FDoubleProperty>();
		const bool bTargetIsReal = TargetProp->IsA<FFloatProperty>() || TargetProp->IsA<FDoubleProperty>();
		if (bSourceIsReal && bTargetIsReal) return true;
		if (SourceProp->IsA<FIntProperty>() && bTargetIsReal) return true;
		if (SourceProp->IsA<FBoolProperty>() && TargetProp->IsA<FNumericProperty>()) return true;

		return false;
	}

	UScriptableObject* GetOuterScriptableObject(const TSharedPtr<const IPropertyHandle>& InPropertyHandle)
	{
		TArray<UObject*> OuterObjects;
		InPropertyHandle->GetOuterObjects(OuterObjects);
		for (UObject* OuterObject : OuterObjects)
		{
			if (OuterObject)
			{
				if (UScriptableObject* ScriptableObject = Cast<UScriptableObject>(OuterObject)) return ScriptableObject;
				if (UScriptableObject* OuterScriptableObject = OuterObject->GetTypedOuter<UScriptableObject>()) return OuterScriptableObject;
			}
		}
		return nullptr;
	}

	FGuid GetScriptableObjectDataID(UScriptableObject* Owner)
	{
		return Owner ? Owner->GetBindingID() : FGuid();
	}

	TSharedPtr<IPropertyHandle> FindActionStructHandle(TSharedPtr<IPropertyHandle> ChildHandle)
	{
		TSharedPtr<IPropertyHandle> Current = ChildHandle;
		while (Current.IsValid())
		{
			if (const FStructProperty* StructProp = CastField<FStructProperty>(Current->GetProperty()))
			{
				if (StructProp->Struct == FScriptableAction::StaticStruct())
				{
					return Current;
				}
			}
			Current = Current->GetParentHandle();
		}
		return nullptr;
	}

	void GetAccessibleStructs(const UScriptableObject* TargetObject, const TSharedPtr<IPropertyHandle>& Handle, TArray<FBindableStructDesc>& OutStructDescs)
	{
		if (!TargetObject) return;
		const UScriptableObject* RootObject = TargetObject->GetRoot();
		if (!RootObject) return;

		// 1. Context (From FScriptableAction via Handle)
		// We try to find the Action struct walking up the handle hierarchy.
		if (TSharedPtr<IPropertyHandle> ActionHandle = FindActionStructHandle(Handle))
		{
			// We access the raw data of the struct to read the Context
			void* ActionData = nullptr;
			if (ActionHandle->GetValueData(ActionData) == FPropertyAccess::Success && ActionData)
			{
				const FScriptableAction* Action = static_cast<const FScriptableAction*>(ActionData);
				if (Action->Context.IsValid())
				{
					FBindableStructDesc& ContextDesc = OutStructDescs.AddDefaulted_GetRef();
					ContextDesc.Name = FName(TEXT("Context"));
					ContextDesc.Struct = Action->Context.GetPropertyBagStruct();
					ContextDesc.ID = FGuid(); // Context ID is empty/null
				}
			}
		}

		// 2. Traversal (Siblings/Parents)
		TArray<const UScriptableObject*> AccessibleObjects;
		const UObject* IteratorNode = TargetObject;

		while (IteratorNode)
		{
			const UObject* ParentNode = IteratorNode->GetOuter();
			// Stop if we hit the top of the hierarchy (Action Owner or Package)
			if (!ParentNode || ParentNode == RootObject->GetOuter()) break;

			if (const UScriptableObject* ParentScriptableObject = Cast<UScriptableObject>(ParentNode))
			{
				AccessibleObjects.Add(ParentScriptableObject); // Parent
				if (ScriptableObjectTraversal::AreSiblingBindingsAllowed(ParentScriptableObject))
				{
					ScriptableObjectTraversal::CollectPreviousSiblings(ParentScriptableObject, IteratorNode, AccessibleObjects); // Siblings
				}
			}
			IteratorNode = ParentNode;
		}

		// 3. Convert
		for (const UScriptableObject* Obj : AccessibleObjects)
		{
			FBindableStructDesc& Desc = OutStructDescs.AddDefaulted_GetRef();
			Desc.Name = FName(*Obj->GetName());
			Desc.Struct = Obj->GetClass();
			Desc.ID = Obj->GetBindingID();
		}
	}

	void MakeStructPropertyPathFromPropertyHandle(UScriptableObject* ScriptableObject, TSharedPtr<const IPropertyHandle> InPropertyHandle, FPropertyBindingPath& OutPath)
	{
		OutPath.Reset();
		if (!ScriptableObject) return;

		TArray<FPropertyBindingPathSegment> PathSegments;
		TSharedPtr<const IPropertyHandle> CurrentPropertyHandle = InPropertyHandle;

		while (CurrentPropertyHandle.IsValid())
		{
			const FProperty* Property = CurrentPropertyHandle->GetProperty();
			if (Property)
			{
				if (const UClass* PropertyOwnerClass = Cast<UClass>(Property->GetOwnerStruct()))
				{
					if (!ScriptableObject->GetClass()->IsChildOf(PropertyOwnerClass)) break;
				}

				FPropertyBindingPathSegment& Segment = PathSegments.InsertDefaulted_GetRef(0);
				Segment.SetName(Property->GetFName());
				Segment.SetArrayIndex(CurrentPropertyHandle->GetIndexInArray());

				if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
				{
					if (ObjectProperty->HasAnyPropertyFlags(CPF_PersistentInstance | CPF_InstancedReference))
					{
						const UObject* Object = nullptr;
						if (CurrentPropertyHandle->GetValue(Object) == FPropertyAccess::Success && Object)
						{
							Segment.SetInstanceStruct(Object->GetClass());
						}
					}
				}
				else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					if (StructProperty->Struct == TBaseStructure<FInstancedStruct>::Get())
					{
						void* Address = nullptr;
						if (CurrentPropertyHandle->GetValueData(Address) == FPropertyAccess::Success && Address)
						{
							const FInstancedStruct& Struct = *static_cast<const FInstancedStruct*>(Address);
							Segment.SetInstanceStruct(Struct.GetScriptStruct());
						}
					}
				}

				if (Segment.GetArrayIndex() != INDEX_NONE)
				{
					TSharedPtr<const IPropertyHandle> ParentPropertyHandle = CurrentPropertyHandle->GetParentHandle();
					if (ParentPropertyHandle.IsValid())
					{
						const FProperty* ParentProperty = ParentPropertyHandle->GetProperty();
						if (ParentProperty && ParentProperty->IsA<FArrayProperty>() && Property->GetFName() == ParentProperty->GetFName())
						{
							CurrentPropertyHandle = ParentPropertyHandle;
						}
					}
				}
			}
			CurrentPropertyHandle = CurrentPropertyHandle->GetParentHandle();
		}

		if (PathSegments.Num() > 0)
		{
			FGuid OwnerID = GetScriptableObjectDataID(ScriptableObject);
			OutPath = FPropertyBindingPath(OwnerID, PathSegments);
		}
	}

	void MakeStructPropertyPathFromBindingChain(const FGuid& StructID, const TArray<FBindingChainElement>& InBindingChain, FPropertyBindingPath& OutPath)
	{
		OutPath.Reset();
		OutPath.SetStructID(StructID);

		for (const FBindingChainElement& Element : InBindingChain)
		{
			if (const FProperty* Property = Element.Field.Get<FProperty>())
			{
				OutPath.AddPathSegment(Property->GetFName(), Element.ArrayIndex);
			}
		}
	}

	void SetWrapperAssetProperty(TSharedPtr<IPropertyHandle> Handle, UObject* Asset)
	{
		if (!Handle.IsValid()) return;
		UObject* NewObj = nullptr;
		if (Handle->GetValue(NewObj) == FPropertyAccess::Success && NewObj)
		{
			static const FName CandidateProps[] = { FName("AssetToRun"), FName("AssetToEvaluate") };
			for (const FName& P : CandidateProps)
			{
				if (FObjectProperty* AssetProp = CastField<FObjectProperty>(NewObj->GetClass()->FindPropertyByName(P)))
				{
					NewObj->Modify();
					void* ValuePtr = AssetProp->ContainerPtrToValuePtr<void>(NewObj);
					AssetProp->SetObjectPropertyValue(ValuePtr, Asset);
					return;
				}
			}
		}
	}
}

UE_ENABLE_OPTIMIZATION