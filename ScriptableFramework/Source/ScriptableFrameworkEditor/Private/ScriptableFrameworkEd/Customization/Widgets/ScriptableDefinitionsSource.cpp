// Copyright 2026 kirzo

#include "ScriptableFrameworkEd/Customization/Widgets/ScriptableDefinitionsSource.h"
#include "ScriptableContainer.h"
#include "ScriptableObject.h"
#include "ScriptableRuntimeData.h"
#include "ScriptableTasks/ScriptableTask_CallFunction.h"
#include "ScriptableTasks/ScriptableTask_SetLocal.h"
#include "Bindings/ScriptablePropertyBindings.h"

#include "Misc/PackageName.h"
#include "PropertyHandle.h"
#include "UObject/UObjectHash.h"

#define LOCTEXT_NAMESPACE "ScriptableDefinitionsSource"

namespace ScriptableDefinitionsSource
{
	/** The fields of a task that name a Local directly rather than binding to it. */
	void GetLocalNameFields(UScriptableObject& Object, TArray<FName*>& OutFields)
	{
		if (UScriptableTask_SetLocal* SetLocal = Cast<UScriptableTask_SetLocal>(&Object))
		{
			OutFields.Add(&SetLocal->VarName);
		}
		if (UScriptableTask_CallFunction* CallFunction = Cast<UScriptableTask_CallFunction>(&Object))
		{
			OutFields.Add(&CallFunction->ResultLocal);
		}
	}

	bool ReadsName(const FScriptablePropertyBinding& Binding, const FGuid& SourceID, FName Name)
	{
		const TConstArrayView<FPropertyBindingPathSegment> Segments = Binding.SourcePath.GetSegments();
		return Binding.SourceID == SourceID && Segments.Num() > 0 && Segments[0].GetName() == Name;
	}
}

const FGuid& FScriptableDefinitionsSource::GetSourceID(ETab Tab)
{
	return Tab == ETab::Locals ? ScriptableBindingSources::LocalsStructID : ScriptableBindingSources::ContextStructID;
}

void FScriptableDefinitionsSource::RedirectReferences(ETab Tab, FName OldName, FName NewName)
{
	const FGuid& SourceID = GetSourceID(Tab);
	ForEachObjectInScope(Tab, [&](UScriptableObject& Object)
	{
		TArray<FName*> LocalNameFields;
		if (Tab == ETab::Locals)
		{
			ScriptableDefinitionsSource::GetLocalNameFields(Object, LocalNameFields);
		}

		const bool bReadsOldName = Object.GetPropertyBindings().Bindings.ContainsByPredicate([&](const FScriptablePropertyBinding& Binding) { return ScriptableDefinitionsSource::ReadsName(Binding, SourceID, OldName); })
			|| LocalNameFields.ContainsByPredicate([OldName](const FName* Field) { return *Field == OldName; });
		if (!bReadsOldName)
		{
			return;
		}

		// Snapshot before the change, so undo restores the old name.
		Object.Modify();
		for (FScriptablePropertyBinding& Binding : Object.GetPropertyBindings().Bindings)
		{
			if (ScriptableDefinitionsSource::ReadsName(Binding, SourceID, OldName))
			{
				Binding.SourcePath.GetMutableSegments()[0].SetName(NewName);
			}
		}
		for (FName* Field : LocalNameFields)
		{
			if (*Field == OldName)
			{
				*Field = NewName;
			}
		}
	});
}

int32 FScriptableDefinitionsSource::CountReferences(ETab Tab, FName Name) const
{
	const FGuid& SourceID = GetSourceID(Tab);
	int32 Count = 0;
	ForEachObjectInScope(Tab, [&](UScriptableObject& Object)
	{
		for (const FScriptablePropertyBinding& Binding : Object.GetPropertyBindings().Bindings)
		{
			Count += ScriptableDefinitionsSource::ReadsName(Binding, SourceID, Name) ? 1 : 0;
		}

		if (Tab == ETab::Locals)
		{
			TArray<FName*> LocalNameFields;
			ScriptableDefinitionsSource::GetLocalNameFields(Object, LocalNameFields);
			for (const FName* Field : LocalNameFields)
			{
				Count += *Field == Name ? 1 : 0;
			}
		}
	});
	return Count;
}

void FScriptableDefinitionsSource::VisitObject(UScriptableObject& Object, ETab Tab, TFunctionRef<void(UScriptableObject&)> Visit)
{
	Visit(Object);

	// Nested containers (NestedAction's Action, NestedRequirement's Requirement) share the scope, except one
	// declaring its own Locals, whose bindings resolve to its own bag.
	for (TFieldIterator<FStructProperty> It(Object.GetClass()); It; ++It)
	{
		FStructProperty* StructProp = *It;
		if (!StructProp->Struct->IsChildOf(FScriptableContainer::StaticStruct()))
		{
			continue;
		}

		FScriptableContainer* Nested = StructProp->ContainerPtrToValuePtr<FScriptableContainer>(&Object);
		if (Nested && !(Tab == ETab::Locals && Nested->LocalsDefinitions.Num() > 0))
		{
			VisitContainer(*Nested, StructProp->Struct, Tab, Visit);
		}
	}
}

void FScriptableDefinitionsSource::VisitContainer(FScriptableContainer& Container, const UScriptStruct* Struct, ETab Tab, TFunctionRef<void(UScriptableObject&)> Visit)
{
	for (TFieldIterator<FArrayProperty> It(Struct); It; ++It)
	{
		FArrayProperty* ArrayProp = *It;
		FObjectProperty* InnerObjProp = CastField<FObjectProperty>(ArrayProp->Inner);
		if (!InnerObjProp || !InnerObjProp->PropertyClass->IsChildOf(UScriptableObject::StaticClass()))
		{
			continue;
		}

		FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(&Container));
		for (int32 Index = 0; Index < Helper.Num(); ++Index)
		{
			if (UScriptableObject* Object = Cast<UScriptableObject>(InnerObjProp->GetObjectPropertyValue(Helper.GetRawPtr(Index))))
			{
				VisitObject(*Object, Tab, Visit);
			}
		}
	}
}

FScriptableContainerDefinitionsSource::FScriptableContainerDefinitionsSource(const TSharedRef<IPropertyHandle>& InHandle)
	: Handle(InHandle)
{
	TArray<UObject*> OuterObjects;
	InHandle->GetOuterObjects(OuterObjects);
	for (UObject* Outer : OuterObjects)
	{
		Owners.Add(Outer);
	}

	TArray<void*> RawData;
	InHandle->AccessRawData(RawData);
	Container = RawData.Num() == 1 ? static_cast<FScriptableContainer*>(RawData[0]) : nullptr;
	const FStructProperty* StructProperty = CastField<FStructProperty>(InHandle->GetProperty());
	ContainerStruct = StructProperty ? StructProperty->Struct.Get() : FScriptableContainer::StaticStruct();
}

FScriptableContainer* FScriptableContainerDefinitionsSource::GetContainer() const
{
	for (const TWeakObjectPtr<UObject>& Owner : Owners)
	{
		if (!Owner.IsValid())
		{
			return nullptr;
		}
	}
	return Owners.IsEmpty() ? nullptr : Container;
}

bool FScriptableContainerDefinitionsSource::IsValid() const
{
	return GetContainer() != nullptr;
}

FText FScriptableContainerDefinitionsSource::GetTitle() const
{
	const UPackage* Package = GetPackage();
	const FText AssetName = Package ? FText::FromString(FPackageName::GetShortName(Package->GetName())) : FText::GetEmpty();
	return FText::Format(LOCTEXT("ContainerTitle", "{0} {1}: Context & Locals"), AssetName, Handle->GetPropertyDisplayName());
}

UPackage* FScriptableContainerDefinitionsSource::GetPackage() const
{
	const UObject* Owner = Owners.IsEmpty() ? nullptr : Owners[0].Get();
	return Owner ? Owner->GetPackage() : nullptr;
}

TArray<FKzParamDef> FScriptableContainerDefinitionsSource::GetContext() const
{
	const FScriptableContainer* Current = GetContainer();
	return Current ? Current->ContextDefinitions.Array() : TArray<FKzParamDef>();
}

void FScriptableContainerDefinitionsSource::SetContext(const TArray<FKzParamDef>& Entries)
{
	// The set iterates in insertion order once rebuilt.
	if (FScriptableContainer* Current = GetContainer())
	{
		Current->ContextDefinitions.Empty(Entries.Num());
		Current->ContextDefinitions.Append(Entries);
	}
}

TArray<FKzNamedVariant>* FScriptableContainerDefinitionsSource::GetLocals() const
{
	FScriptableContainer* Current = GetContainer();
	return Current ? &Current->LocalsDefinitions : nullptr;
}

void FScriptableContainerDefinitionsSource::BeginChange(ETab Tab)
{
	if (Handle->IsValidHandle())
	{
		Handle->NotifyPreChange();
		return;
	}

	// The details panel the window came from rebuilt: the owners themselves are snapshot.
	for (const TWeakObjectPtr<UObject>& Owner : Owners)
	{
		if (UObject* Object = Owner.Get())
		{
			Object->Modify();
		}
	}
}

void FScriptableContainerDefinitionsSource::EndChange(ETab Tab)
{
	FScriptableContainer* Current = GetContainer();
	if (!Current)
	{
		return;
	}

	Current->ConstructContext();
	Current->ConstructLocals();

	if (Handle->IsValidHandle())
	{
		Handle->NotifyPostChange(EPropertyChangeType::ValueSet);
	}
	else
	{
		for (const TWeakObjectPtr<UObject>& Owner : Owners)
		{
			if (UObject* Object = Owner.Get())
			{
				Object->PostEditChange();
			}
		}
	}

	// The owners hear of the change, but not the tasks nested in them, whose auto-bindings read the definitions.
	for (const TWeakObjectPtr<UObject>& Owner : Owners)
	{
		if (UObject* Object = Owner.Get())
		{
			TArray<UObject*> NestedObjects;
			GetObjectsWithOuter(Object, NestedObjects, /*bIncludeNestedObjects=*/true);
			for (UObject* Nested : NestedObjects)
			{
				if (UScriptableObject* Scriptable = Cast<UScriptableObject>(Nested))
				{
					Scriptable->BakeAutoBindings();
				}
			}
		}
	}
}

void FScriptableContainerDefinitionsSource::ForEachObjectInScope(ETab Tab, TFunctionRef<void(UScriptableObject&)> Visit) const
{
	if (FScriptableContainer* Current = GetContainer())
	{
		VisitContainer(*Current, ContainerStruct, Tab, Visit);
	}
}

#undef LOCTEXT_NAMESPACE