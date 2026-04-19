// Copyright 2026 kirzo

#include "ScriptableFrameworkEditorHelpers.h"
#include "ScriptablePropertyUtilities.h"
#include "PropertyHandle.h"
#include "ScriptableObject.h"
#include "ScriptableContainer.h"
#include "ScriptableTasks/ScriptableTask.h"
#include "ScriptableConditions/ScriptableCondition.h"
#include "ScriptableConditions/ScriptableRequirement.h"
#include "StructUtils/InstancedStruct.h"
#include "IPropertyAccessEditor.h"
#include "PropertyBindingBindableStructDescriptor.h"

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

	bool IsPropertyExtendable(TSharedPtr<IPropertyHandle> InPropertyHandle)
	{
		const FProperty* Property = InPropertyHandle->GetProperty();
		if (!Property || Property->HasAnyPropertyFlags(CPF_PersistentInstance | CPF_EditorOnly | CPF_Config | CPF_Deprecated)) return false;

		// Filter out internal framework properties
		if (Property->HasMetaData(TEXT("NoBinding")))
		{
			return false;
		}

		// Filter containers. Native UI handles them, we only extend elements.
		if (Property->IsA<FSetProperty>() || Property->IsA<FMapProperty>())
		{
			return false;
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

	FString GetPropertyMetaData(const TSharedPtr<IPropertyHandle>& Handle, const FName& MetaDataKey)
	{
		if (!Handle.IsValid())
		{
			return FString();
		}

		// Try to get the filter from the handle itself
		FString FilterString = Handle->GetMetaData(MetaDataKey);

		// If empty, check the Parent (Covers FScriptableAction / Wrapper Struct case)
		if (FilterString.IsEmpty() && Handle->GetParentHandle().IsValid())
		{
			FilterString = Handle->GetParentHandle()->GetMetaData(MetaDataKey);

			// If it is an Array, metadata is usually on the ArrayProperty, not the element.
			// This covers: Array -> Struct -> Object
			if (FilterString.IsEmpty() && Handle->GetParentHandle()->GetParentHandle().IsValid())
			{
				FilterString = Handle->GetParentHandle()->GetParentHandle()->GetMetaData(MetaDataKey);
			}
		}

		return FilterString;
	}

	bool ValidateBindingPath(const UStruct* ContextStruct, const FPropertyBindingPath* Path, const FProperty*& OutLeafProperty)
	{
		if (!ContextStruct || !Path) return false;

		const UStruct* CurrentStruct = ContextStruct;
		const FProperty* CurrentProp = nullptr;

		for (int32 i = 0; i < Path->NumSegments(); ++i)
		{
			if (!CurrentStruct) return false;

			const FPropertyBindingPathSegment& Segment = Path->GetSegment(i);
			const FName SegName = Segment.GetName();

			// 1. Check if it's a standard property
			if (const FProperty* FoundProp = CurrentStruct->FindPropertyByName(SegName))
			{
				CurrentProp = FoundProp;
			}
			// 2. Check if it's a Function (Functions only live in UClasses)
			else if (const UClass* CurrentClass = Cast<UClass>(CurrentStruct))
			{
				if (UFunction* Func = CurrentClass->FindFunctionByName(SegName))
				{
					CurrentProp = Func->GetReturnProperty();
					if (!CurrentProp) return false; // If the function returns nothing, the path is invalid
				}
				else return false;
			}
			else return false;

			// 3. Resolve if we are accessing an Array index
			if (Segment.GetArrayIndex() != INDEX_NONE)
			{
				if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(CurrentProp))
				{
					CurrentProp = ArrayProp->Inner;
				}
				else return false;
			}

			// 4. Prepare CurrentStruct for the next loop iteration
			if (i < Path->NumSegments() - 1)
			{
				if (const FStructProperty* StructProp = CastField<FStructProperty>(CurrentProp))
				{
					CurrentStruct = StructProp->Struct;
				}
				else if (const FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(CurrentProp))
				{
					CurrentStruct = ObjProp->PropertyClass;
				}
				else return false; // We reached a primitive type (e.g. Float) but the path continues, error.
			}
		}

		OutLeafProperty = CurrentProp;
		return true;
	}

	FText GetCleanBindingPathText(const FPropertyBindingPath* Path)
	{
		if (!Path || Path->IsPathEmpty()) return FText::GetEmpty();

		FString DisplayString;
		for (const FPropertyBindingPathSegment& Seg : Path->GetSegments())
		{
			FString SegStr = Seg.GetName().ToString();

			// Remove ugly Blueprint prefixes
			if (SegStr.StartsWith(TEXT("K2_")))
			{
				SegStr = SegStr.Mid(3);
			}

			if (Seg.GetArrayIndex() != INDEX_NONE)
			{
				SegStr += FString::Printf(TEXT("[%d]"), Seg.GetArrayIndex());
			}

			if (!DisplayString.IsEmpty()) DisplayString += TEXT(".");
			DisplayString += SegStr;
		}

		return FText::FromString(DisplayString);
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
			// Check if the element is a standard property
			if (const FProperty* Property = Element.Field.Get<FProperty>())
			{
				OutPath.AddPathSegment(Property->GetFName(), Element.ArrayIndex);
			}
			// Check if the element is a function (e.g., K2_GetActorLocation)
			else if (const UFunction* Function = Element.Field.Get<UFunction>())
			{
				OutPath.AddPathSegment(Function->GetFName(), Element.ArrayIndex);
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