// Copyright 2026 kirzo

#include "ScriptablePropertyUtilities.h"
#include "ScriptableObject.h"
#include "ScriptableContainer.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "StructUtils/PropertyBag.h"

bool FScriptablePropertyUtilities::ArePropertiesCompatible(const FProperty* SourceProp, const FProperty* TargetProp)
{
	if (!SourceProp || !TargetProp) return false;

	// 1. Fast Path: Identical Types
	if (SourceProp->SameType(TargetProp))
	{
		return true;
	}

	// 2. Arrays (Recursive Check)
	if (const FArrayProperty* SourceArray = CastField<FArrayProperty>(SourceProp))
	{
		if (const FArrayProperty* TargetArray = CastField<FArrayProperty>(TargetProp))
		{
			// Recursively check inner properties to support array covariance and conversions
			return ArePropertiesCompatible(SourceArray->Inner, TargetArray->Inner);
		}
		return false;
	}
	// If Source is not an array, but Target is, they are incompatible
	if (TargetProp->IsA<FArrayProperty>()) return false;

	// 3. Objects
	if (const FObjectPropertyBase* SourceObj = CastField<FObjectPropertyBase>(SourceProp))
	{
		// 3A. Object <-> Object (Bidirectional QoL for Blueprints)
		if (const FObjectPropertyBase* TargetObj = CastField<FObjectPropertyBase>(TargetProp))
		{
			return SourceObj->PropertyClass->IsChildOf(TargetObj->PropertyClass) ||
				TargetObj->PropertyClass->IsChildOf(SourceObj->PropertyClass);
		}

		// 3B. Object -> Bool (IsValid check)
		// Allows binding an Object directly to a Bool (True if valid, False if null)
		if (TargetProp->IsA<FBoolProperty>())
		{
			return true;
		}
	}

	// 4. Structs
	if (const FStructProperty* SourceStruct = CastField<FStructProperty>(SourceProp))
	{
		if (const FStructProperty* TargetStruct = CastField<FStructProperty>(TargetProp))
		{
			return SourceStruct->Struct == TargetStruct->Struct;
		}
	}

	// 5. Enums & Bytes
	// Special case: Allow cross-binding between Bytes and Enums
	if ((SourceProp->IsA<FByteProperty>() && TargetProp->IsA<FEnumProperty>()) ||
		(SourceProp->IsA<FEnumProperty>() && TargetProp->IsA<FByteProperty>()))
	{
		return true;
	}
	// Strict Enum check
	if (const FEnumProperty* SourceEnum = CastField<FEnumProperty>(SourceProp))
	{
		if (const FEnumProperty* TargetEnum = CastField<FEnumProperty>(TargetProp))
		{
			return SourceEnum->GetEnum() == TargetEnum->GetEnum();
		}
	}

	// 6. Numeric & Bool Conversions
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

#if WITH_EDITOR
bool FScriptablePropertyUtilities::IsPropertyBindableInput(const FProperty* Property)
{
	if (!Property) return false;
	if (Property->HasMetaData(TEXT("ScriptableInput"))) return true;
	const FString Category = Property->GetMetaData(TEXT("Category"));
	return Category.Contains(TEXT("Input"));
}

bool FScriptablePropertyUtilities::IsPropertyBindableOutput(const FProperty* Property)
{
	if (!Property) return false;
	if (Property->HasMetaData(TEXT("ScriptableOutput"))) return true;
	const FString Category = Property->GetMetaData(TEXT("Category"));
	return Category.Contains(TEXT("Output"));
}

bool FScriptablePropertyUtilities::IsPropertyBindableContext(const FProperty* Property)
{
	if (!Property) return false;
	if (Property->HasMetaData(TEXT("ScriptableContext"))) return true;
	const FString Category = Property->GetMetaData(TEXT("Category"));
	return Category.Contains(TEXT("Context"));
}

bool FScriptablePropertyUtilities::AreSiblingBindingsAllowed(const UObject* ParentObject)
{
	if (!ParentObject) return false;
	return !ParentObject->GetClass()->HasMetaData(TEXT("BlockSiblingBindings"));
}

void FScriptablePropertyUtilities::CollectPreviousSiblings(const UObject* ParentObject, const UObject* CurrentChild, TArray<const UScriptableObject*>& OutObjects)
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

void FScriptablePropertyUtilities::GatherAccessibleStructs(const UScriptableObject* TargetObject, TArray<FPropertyBindingBindableStructDescriptor>& OutStructDescs)
{
	OutStructDescs.Reset();
	if (!TargetObject) return;
	const UScriptableObject* RootObject = TargetObject->GetRoot();
	if (!RootObject) return;

	// =======================================================================================
	// LAMBDA 1: Reflection Helper to find the FScriptableContainer holding a specific Node
	// =======================================================================================
	auto FindContainerForNode = [](const UObject* Node) -> const FScriptableContainer*
		{
			if (!Node) return nullptr;
			const UObject* Owner = Node->GetOuter();
			if (!Owner) return nullptr;

			const UScriptStruct* BaseContainerStruct = FScriptableContainer::StaticStruct();

			// Iterate through all properties of the Owner
			for (TFieldIterator<FProperty> It(Owner->GetClass()); It; ++It)
			{
				// CASE A: Direct Struct Property (e.g., FScriptableAction MyAction;)
				if (const FStructProperty* StructProp = CastField<FStructProperty>(*It))
				{
					if (StructProp->Struct->IsChildOf(BaseContainerStruct))
					{
						const FScriptableContainer* Container = StructProp->ContainerPtrToValuePtr<FScriptableContainer>(Owner);

						// Reflect inside the container to see if it holds our Node
						for (TFieldIterator<FProperty> InnerIt(StructProp->Struct); InnerIt; ++InnerIt)
						{
							if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(*InnerIt))
							{
								if (const FObjectProperty* ObjProp = CastField<FObjectProperty>(ArrayProp->Inner))
								{
									FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Container));
									for (int32 i = 0; i < Helper.Num(); ++i)
									{
										if (ObjProp->GetObjectPropertyValue(Helper.GetRawPtr(i)) == Node) return Container;
									}
								}
							}
							else if (const FObjectProperty* ObjProp = CastField<FObjectProperty>(*InnerIt))
							{
								if (ObjProp->GetObjectPropertyValue_InContainer(Container) == Node) return Container;
							}
						}
					}
				}
				// CASE B: Array of Structs (e.g., TArray<FScriptableRequirement> Requirements;)
				else if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(*It))
				{
					if (const FStructProperty* InnerStructProp = CastField<FStructProperty>(ArrayProp->Inner))
					{
						if (InnerStructProp->Struct->IsChildOf(BaseContainerStruct))
						{
							FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Owner));
							for (int32 Index = 0; Index < Helper.Num(); ++Index)
							{
								const FScriptableContainer* Container = reinterpret_cast<const FScriptableContainer*>(Helper.GetRawPtr(Index));

								for (TFieldIterator<FProperty> InnerIt(InnerStructProp->Struct); InnerIt; ++InnerIt)
								{
									if (const FArrayProperty* InnerArrayProp = CastField<FArrayProperty>(*InnerIt))
									{
										if (const FObjectProperty* ObjProp = CastField<FObjectProperty>(InnerArrayProp->Inner))
										{
											FScriptArrayHelper InnerHelper(InnerArrayProp, InnerArrayProp->ContainerPtrToValuePtr<void>(Container));
											for (int32 i = 0; i < InnerHelper.Num(); ++i)
											{
												if (ObjProp->GetObjectPropertyValue(InnerHelper.GetRawPtr(i)) == Node) return Container;
											}
										}
									}
									else if (const FObjectProperty* ObjProp = CastField<FObjectProperty>(*InnerIt))
									{
										if (ObjProp->GetObjectPropertyValue_InContainer(Container) == Node) return Container;
									}
								}
							}
						}
					}
				}
			}
			return nullptr;
		};

	// =======================================================================================
	// LAMBDA 2: Reflection Helper to collect previous siblings purely from memory arrays
	// =======================================================================================
	auto CollectSiblingsHeadless = [](const UObject* Node, TArray<const UScriptableObject*>& OutSiblings)
		{
			if (!Node) return;
			const UObject* Owner = Node->GetOuter();
			if (!Owner) return;

			for (TFieldIterator<FProperty> It(Owner->GetClass()); It; ++It)
			{
				if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(*It))
				{
					// Array of Objects (TArray<UScriptableObject*>)
					if (const FObjectProperty* ObjProp = CastField<FObjectProperty>(ArrayProp->Inner))
					{
						FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Owner));
						int32 TargetIndex = INDEX_NONE;

						for (int32 i = 0; i < Helper.Num(); ++i)
						{
							if (ObjProp->GetObjectPropertyValue(Helper.GetRawPtr(i)) == Node)
							{
								TargetIndex = i;
								break;
							}
						}

						if (TargetIndex != INDEX_NONE)
						{
							for (int32 i = 0; i < TargetIndex; ++i)
							{
								if (const UScriptableObject* PrevSibling = Cast<UScriptableObject>(ObjProp->GetObjectPropertyValue(Helper.GetRawPtr(i))))
								{
									OutSiblings.Add(PrevSibling);
								}
							}
							return; // Found the array holding our node, we can stop searching
						}
					}
					// Array of Containers (TArray<FScriptableContainer>)
					else if (const FStructProperty* InnerStructProp = CastField<FStructProperty>(ArrayProp->Inner))
					{
						if (InnerStructProp->Struct->IsChildOf(FScriptableContainer::StaticStruct()))
						{
							FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Owner));
							for (int32 Index = 0; Index < Helper.Num(); ++Index)
							{
								const void* ContainerMemory = Helper.GetRawPtr(Index);
								for (TFieldIterator<FProperty> InnerIt(InnerStructProp->Struct); InnerIt; ++InnerIt)
								{
									if (const FArrayProperty* InnerArrayProp = CastField<FArrayProperty>(*InnerIt))
									{
										if (const FObjectProperty* InnerObjProp = CastField<FObjectProperty>(InnerArrayProp->Inner))
										{
											FScriptArrayHelper InnerHelper(InnerArrayProp, InnerArrayProp->ContainerPtrToValuePtr<void>(ContainerMemory));
											int32 TargetIndex = INDEX_NONE;

											for (int32 i = 0; i < InnerHelper.Num(); ++i)
											{
												if (InnerObjProp->GetObjectPropertyValue(InnerHelper.GetRawPtr(i)) == Node)
												{
													TargetIndex = i;
													break;
												}
											}

											if (TargetIndex != INDEX_NONE)
											{
												for (int32 i = 0; i < TargetIndex; ++i)
												{
													if (const UScriptableObject* PrevSibling = Cast<UScriptableObject>(InnerObjProp->GetObjectPropertyValue(InnerHelper.GetRawPtr(i))))
													{
														OutSiblings.Add(PrevSibling);
													}
												}
												return;
											}
										}
									}
								}
							}
						}
					}
				}
			}
		};

	// -------------------------------------------------------------------------------
	// 1. Context Hierarchy (Single Effective Scope via Reflection)
	// -------------------------------------------------------------------------------
	const UObject* CurrentNode = TargetObject;
	bool bFoundContext = false;

	while (CurrentNode && !bFoundContext)
	{
		// Try to find the FScriptableContainer structurally wrapping the CurrentNode
		if (const FScriptableContainer* Container = FindContainerForNode(CurrentNode))
		{
			if (Container->HasContext() && Container->Context.GetNumPropertiesInBag() > 0)
			{
				FPropertyBindingBindableStructDescriptor& ContextDesc = OutStructDescs.AddDefaulted_GetRef();
				ContextDesc.Name = FName(TEXT("Context"));
				ContextDesc.Struct = Container->Context.GetPropertyBagStruct();
				ContextDesc.ID = FGuid(); // Use an empty GUID for Context, just like the UI

				bFoundContext = true;
				break;
			}
		}

		// Move up to the next parent in the hierarchy
		CurrentNode = CurrentNode->GetOuter();
		if (CurrentNode && CurrentNode->IsA<UPackage>()) break;
	}

	// -------------------------------------------------------------------------------
	// 2. Siblings (Via Memory Traversal)
	// -------------------------------------------------------------------------------
	TArray<const UScriptableObject*> AccessibleObjects;
	CollectSiblingsHeadless(TargetObject, AccessibleObjects);

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
			if (AreSiblingBindingsAllowed(ParentScriptableObject))
			{
				CollectPreviousSiblings(ParentScriptableObject, IteratorNode, AccessibleObjects);
			}
		}
		IteratorNode = ParentNode;
	}

	// -------------------------------------------------------------------------------
	// 4. Convert to Output
	// -------------------------------------------------------------------------------
	for (const UScriptableObject* Obj : AccessibleObjects)
	{
		if (Obj->GetBindingID().IsValid())
		{
			FPropertyBindingBindableStructDescriptor& Desc = OutStructDescs.AddDefaulted_GetRef();
			FString DisplayName = Obj->GetName();
			Desc.Name = FName(*DisplayName);
			Desc.Struct = Obj->GetClass();
			Desc.ID = Obj->GetBindingID();
		}
	}
}

bool FScriptablePropertyUtilities::FindAutoBindingPath(const FProperty* TargetProperty, const TArray<FPropertyBindingBindableStructDescriptor>& AccessibleStructs, FPropertyBindingPath& OutPath)
{
	if (!TargetProperty) return false;

	// Check if the property is actually marked for context binding (meta or category)
	if (!IsPropertyBindableContext(TargetProperty))
	{
		return false;
	}

	// Search through all available contexts (Global, Local, Owner, etc.)
	for (const FPropertyBindingBindableStructDescriptor& ContextDesc : AccessibleStructs)
	{
		const UStruct* Struct = ContextDesc.Struct.Get();
		if (!Struct)
		{
			continue;
		}

		// This handles cases where both the ScriptableObject and Context have a property with the same name
		if (const FProperty* ExactMatchProp = Struct->FindPropertyByName(TargetProperty->GetFName()))
		{
			if (ArePropertiesCompatible(ExactMatchProp, TargetProperty))
			{
				OutPath.Reset();
				OutPath.SetStructID(ContextDesc.ID);
				OutPath.AddPathSegment(ExactMatchProp->GetFName());
				return true;
			}
		}

		// This handles your edge case: Context has "Owner" (AActor*) and Task asks for "TargetActor" (AActor*)
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FProperty* SourceProp = *It;

			// Skip the exact name match since we already tested it above
			if (SourceProp->GetFName() == TargetProperty->GetFName())
			{
				continue;
			}

			// First property that satisfies the strict type compatibility wins
			if (ArePropertiesCompatible(SourceProp, TargetProperty))
			{
				OutPath.Reset();
				OutPath.SetStructID(ContextDesc.ID);
				OutPath.AddPathSegment(SourceProp->GetFName());
				return true;
			}
		}
	}

	// No compatible property was found in any of the accessible contexts
	return false;
}

#endif