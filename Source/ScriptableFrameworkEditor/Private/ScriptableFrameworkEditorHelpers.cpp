// Copyright 2025 kirzo

#include "ScriptableFrameworkEditorHelpers.h"
#include "PropertyHandle.h"

#include "ScriptableObject.h"
#include "ScriptableTasks/ScriptableTask.h"
#include "ScriptableConditions/ScriptableCondition.h"

namespace ScriptableObjectTraversal
{
	/** Checks if the Parent class allows data binding between its children (Siblings). */
	static bool AreSiblingBindingsAllowed(const UObject* ParentObject)
	{
		if (!ParentObject) return false;
		return !ParentObject->GetClass()->HasMetaData(TEXT("BlockSiblingBindings"));
	}

	/**
	 * Scans the ParentObject for any Array Property that contains the CurrentChild.
	 * If found, it collects all objects in that array that reside at an index lower than CurrentChild.
	 */
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
			if (Property->GetMetaData(TEXT("Category")) == TEXT("Hidden"))
			{
				return false;
			}

			// CPF_Edit and CPF_DisableEditOnInstance are automatically set on properties marked as 'EditDefaultsOnly'
			// See HeaderParser.cpp around line 3716 (switch case EVariableSpecifier::EditDefaultsOnly)
			if (Property->HasAllPropertyFlags(CPF_Edit | CPF_DisableEditOnInstance))
			{
				if (UObject* Object = Property->GetOwnerUObject())
				{
					// Properties of instanced UObjects marked as 'EditDefaultsOnly' are visible when the owner of the instance is an asset
					// This properties should only be visible in the CDO
					// Note: This also works with blueprint variables that are not 'InstanceEditable'
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

		// 1. Explicit C++ Meta Tag
		if (Property->HasMetaData(TEXT("ScriptableOutput")))
		{
			return true;
		}

		// 2. Category Convention
		const FString Category = Property->GetMetaData(TEXT("Category"));
		return Category.StartsWith(TEXT("Output"));
	}

	void GetAccessibleStructs(const UScriptableObject* TargetObject, TArray<struct FBindableStructDesc>& OutStructDescs)
	{
		if (!TargetObject) return;

		const UScriptableObject* RootObject = TargetObject->GetRoot();
		if (!RootObject) return;

		// ----------------------------------------------------------------
		// 1. Add Global Context (From Root)
		// ----------------------------------------------------------------

		// Access the Context PropertyBag directly. 
		const FInstancedPropertyBag& Bag = RootObject->GetContext();
		if (Bag.IsValid())
		{
			FBindableStructDesc& ContextDesc = OutStructDescs.AddDefaulted_GetRef();
			ContextDesc.Name = FName(TEXT("Context"));
			ContextDesc.Struct = Bag.GetPropertyBagStruct();
			ContextDesc.ID = FGuid();
		}

		// ----------------------------------------------------------------
		// 2. Traverse Hierarchy
		// ----------------------------------------------------------------

		TArray<const UScriptableObject*> AccessibleObjects;
		const UObject* IteratorNode = TargetObject;

		while (IteratorNode)
		{
			const UObject* ParentNode = IteratorNode->GetOuter();

			// Stop if we go past the Root
			if (!ParentNode || ParentNode == RootObject->GetOuter())
			{
				break;
			}

			if (const UScriptableObject* ParentScriptableObject = Cast<UScriptableObject>(ParentNode))
			{
				// A. Add Parent
				AccessibleObjects.Add(ParentScriptableObject);

				// B. Add Previous Siblings
				if (ScriptableObjectTraversal::AreSiblingBindingsAllowed(ParentScriptableObject))
				{
					ScriptableObjectTraversal::CollectPreviousSiblings(ParentScriptableObject, IteratorNode, AccessibleObjects);
				}
			}

			IteratorNode = ParentNode;
		}

		// ----------------------------------------------------------------
		// 3. Convert to Bindable Descs
		// ----------------------------------------------------------------

		for (const UScriptableObject* Obj : AccessibleObjects)
		{
			bool bHasOutputs = false;
			for (TFieldIterator<FProperty> PropIt(Obj->GetClass()); PropIt; ++PropIt)
			{
				if (ScriptableFrameworkEditor::IsPropertyBindableOutput(*PropIt))
				{
					bHasOutputs = true;
					break;
				}
			}

			if (bHasOutputs)
			{
				FBindableStructDesc& Desc = OutStructDescs.AddDefaulted_GetRef();
				Desc.Name = FName(*Obj->GetName());
				Desc.Struct = Obj->GetClass();
				Desc.ID = Obj->GetBindingID();
			}
		}
	}
}