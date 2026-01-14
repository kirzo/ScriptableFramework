// Copyright 2026 kirzo

#include "ScriptableFrameworkEditorHelpers.h"
#include "PropertyHandle.h"
#include "ScriptableObject.h"
#include "ScriptableContainer.h"
#include "ScriptableTasks/ScriptableTask.h"
#include "ScriptableConditions/ScriptableCondition.h"
#include "ScriptableConditions/ScriptableRequirement.h"
#include "StructUtils/InstancedStruct.h"
#include "IPropertyAccessEditor.h"

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
			if (Property->HasMetaData(TEXT("Hidden")) || Property->GetMetaData(TEXT("Category")) == TEXT("Hidden")) return false;
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

		// 3. Object -> Bool (IsValid check)
		// Allows binding an Object directly to a Bool (True if valid, False if null)
		if (SourceProp->IsA<FObjectPropertyBase>() && TargetProp->IsA<FBoolProperty>())
		{
			return true;
		}

		// 4. Numeric & Bool Conversions (Bidirectional)
		const bool bSourceNumeric = SourceProp->IsA<FNumericProperty>();
		const bool bTargetNumeric = TargetProp->IsA<FNumericProperty>();
		const bool bSourceBool = SourceProp->IsA<FBoolProperty>();
		const bool bTargetBool = TargetProp->IsA<FBoolProperty>();

		// Numeric <-> Numeric (Int to Float, Float to Int, Byte to Double, etc.)
		if (bSourceNumeric && bTargetNumeric)
		{
			return true;
		}

		// Bool <-> Numeric (0/1 Logic)
		if ((bSourceBool && bTargetNumeric) || (bSourceNumeric && bTargetBool))
		{
			return true;
		}

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

	TSharedPtr<IPropertyHandle> FindContainerStructHandle(TSharedPtr<IPropertyHandle> ChildHandle)
	{
		TSharedPtr<IPropertyHandle> Current = ChildHandle;
		while (Current.IsValid())
		{
			if (const FStructProperty* StructProp = CastField<FStructProperty>(Current->GetProperty()))
			{
				if (StructProp->Struct && StructProp->Struct->IsChildOf<FScriptableContainer>())
				{
					return Current;
				}
			}
			Current = Current->GetParentHandle();
		}
		return nullptr;
	}

	TSharedPtr<IPropertyHandle> FindObjectHandleInHierarchy(TSharedPtr<IPropertyHandle> StartHandle, const UObject* TargetObject)
	{
		TSharedPtr<IPropertyHandle> Current = StartHandle;
		while (Current.IsValid())
		{
			UObject* Obj = nullptr;
			if (Current->GetValue(Obj) == FPropertyAccess::Success && Obj == TargetObject)
			{
				return Current;
			}
			Current = Current->GetParentHandle();
		}
		return nullptr;
	}

	void CollectSiblingsFromHandle(TSharedPtr<IPropertyHandle> ObjectHandle, TArray<const UScriptableObject*>& OutObjects)
	{
		if (!ObjectHandle.IsValid()) return;

		TSharedPtr<IPropertyHandle> ParentHandle = ObjectHandle->GetParentHandle();
		if (!ParentHandle.IsValid()) return;

		TSharedPtr<IPropertyHandleArray> ArrayHandle = ParentHandle->AsArray();
		if (ArrayHandle.IsValid())
		{
			// It is an array. Get our index.
			int32 MyIndex = ObjectHandle->GetIndexInArray();
			if (MyIndex != INDEX_NONE)
			{
				// Iterate all previous elements
				for (int32 i = 0; i < MyIndex; ++i)
				{
					TSharedRef<IPropertyHandle> Element = ArrayHandle->GetElement(i);
					UObject* Value = nullptr;
					if (Element->GetValue(Value) == FPropertyAccess::Success)
					{
						if (const UScriptableObject* Sibling = Cast<UScriptableObject>(Value))
						{
							OutObjects.Add(Sibling);
						}
					}
				}
			}
		}
	}

	void GetAccessibleStructs(const UScriptableObject* TargetObject, const TSharedPtr<IPropertyHandle>& Handle, TArray<FBindableStructDesc>& OutStructDescs)
	{
		if (!TargetObject) return;
		const UScriptableObject* RootObject = TargetObject->GetRoot();
		if (!RootObject) return;

		// 1. Context Hierarchy (Single Effective Scope)
		// We walk up the hierarchy to find the closest valid Context.
		// If a container (like a Group Requirement) has no variables, we skip it and keep looking up.

		TSharedPtr<IPropertyHandle> CurrentSearchHandle = Handle;
		bool bFoundContext = false;

		while (TSharedPtr<IPropertyHandle> ContainerHandle = FindContainerStructHandle(CurrentSearchHandle))
		{
			void* StructData = nullptr;
			if (ContainerHandle->GetValueData(StructData) == FPropertyAccess::Success && StructData)
			{
				const FScriptableContainer* Container = static_cast<const FScriptableContainer*>(StructData);

				// Only consider this context if it actually has properties defined.
				if (Container->HasContext() && Container->Context.GetNumPropertiesInBag() > 0)
				{
					FBindableStructDesc& ContextDesc = OutStructDescs.AddDefaulted_GetRef();
					ContextDesc.Name = FName(TEXT("Context")); // Always named "Context" as it's the only one visible
					ContextDesc.Struct = Container->Context.GetPropertyBagStruct();
					ContextDesc.ID = FGuid();

					bFoundContext = true;
					break; // Stop searching. We only expose the most relevant (local) context.
				}
			}

			// Move search up past this container
			CurrentSearchHandle = ContainerHandle->GetParentHandle();
		}

		// -------------------------------------------------------------------------------
		// 2. Siblings via Handle
		// -------------------------------------------------------------------------------
		TArray<const UScriptableObject*> AccessibleObjects;

		if (TSharedPtr<IPropertyHandle> ObjectHandle = FindObjectHandleInHierarchy(Handle, TargetObject))
		{
			CollectSiblingsFromHandle(ObjectHandle, AccessibleObjects);
		}

		// -------------------------------------------------------------------------------
		// 3. Traversal (Hierarchical Parents & Siblings of Parents)
		// -------------------------------------------------------------------------------
		const UObject* IteratorNode = TargetObject;
		while (IteratorNode)
		{
			const UObject* ParentNode = IteratorNode->GetOuter();
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

		// 4. Convert to Output
		for (const UScriptableObject* Obj : AccessibleObjects)
		{
			if (Obj->GetBindingID().IsValid())
			{
				FBindableStructDesc& Desc = OutStructDescs.AddDefaulted_GetRef();
				FString DisplayName = Obj->GetName();
				Desc.Name = FName(*DisplayName);
				Desc.Struct = Obj->GetClass();
				Desc.ID = Obj->GetBindingID();
			}
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
				const UStruct* OwnerStruct = Property->GetOwnerStruct();

				// If it's a Class, ensure the ScriptableObject is a child of it.
				if (const UClass* PropertyOwnerClass = Cast<UClass>(OwnerStruct))
				{
					if (!ScriptableObject->GetClass()->IsChildOf(PropertyOwnerClass))
					{
						break;
					}
				}
				// If it's the Container Struct, we went too far up. Stop.
				else if (OwnerStruct->IsChildOf<FScriptableContainer>())
				{
					break;
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
			static const FName NAME_Asset = TEXT("Asset");
			if (FObjectProperty* AssetProp = CastField<FObjectProperty>(NewObj->GetClass()->FindPropertyByName(NAME_Asset)))
			{
				NewObj->Modify();
				void* ValuePtr = AssetProp->ContainerPtrToValuePtr<void>(NewObj);
				AssetProp->SetObjectPropertyValue(ValuePtr, Asset);
			}
		}
	}
}