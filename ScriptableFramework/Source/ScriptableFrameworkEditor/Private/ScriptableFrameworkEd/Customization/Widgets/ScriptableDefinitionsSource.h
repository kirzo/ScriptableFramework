// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "Core/KzNamedVariant.h"
#include "Core/KzParamDef.h"

class IPropertyHandle;
class UScriptableObject;
struct FScriptableContainer;

enum class EScriptableDefinitionsTab : uint8
{
	Context,
	Locals
};

/** What the definitions editor edits: the Context and Locals of a container or a graph, the objects reading them, and how their owners hear of a change. */
class FScriptableDefinitionsSource
{
public:
	using ETab = EScriptableDefinitionsTab;

	virtual ~FScriptableDefinitionsSource() = default;

	/** False once an owner is gone, or when there is nothing single to edit. */
	virtual bool IsValid() const = 0;

	/** Identifies what is edited, so a second window on it brings back the first. */
	virtual const void* GetKey() const = 0;

	virtual FText GetTitle() const = 0;

	/** The package the values live in. */
	virtual UPackage* GetPackage() const = 0;

	virtual TArray<FKzParamDef> GetContext() const = 0;

	/** Replaces the Context entries, kept in this order; only between BeginChange and EndChange. */
	virtual void SetContext(const TArray<FKzParamDef>& Entries) = 0;

	virtual TArray<FKzNamedVariant>* GetLocals() const = 0;

	/** Snapshots the owners for undo before a change of the tab, and lets them and the objects reading it catch up after it. */
	virtual void BeginChange(ETab Tab) = 0;
	virtual void EndChange(ETab Tab) = 0;

	/** Visits each scriptable object that sees the tab's entries. */
	virtual void ForEachObjectInScope(ETab Tab, TFunctionRef<void(UScriptableObject&)> Visit) const = 0;

	/** Rewrites the bindings and the task fields naming the entry, from OldName to NewName; only inside a change. */
	void RedirectReferences(ETab Tab, FName OldName, FName NewName);

	/** Bindings and task fields reading the entry. */
	int32 CountReferences(ETab Tab, FName Name) const;

	static const FGuid& GetSourceID(ETab Tab);

protected:
	/** Visits the object, then the objects of the containers nested in it that share the tab's scope. */
	static void VisitObject(UScriptableObject& Object, ETab Tab, TFunctionRef<void(UScriptableObject&)> Visit);

	/** Visits the scriptable objects in the container's arrays, each with its nested ones. */
	static void VisitContainer(FScriptableContainer& Container, const UScriptStruct* Struct, ETab Tab, TFunctionRef<void(UScriptableObject&)> Visit);
};

/** A container behind a property handle: an FScriptableAction or FScriptableRequirement on an actor, a component or an asset. */
class FScriptableContainerDefinitionsSource : public FScriptableDefinitionsSource
{
public:
	explicit FScriptableContainerDefinitionsSource(const TSharedRef<IPropertyHandle>& InHandle);

	virtual bool IsValid() const override;
	virtual const void* GetKey() const override { return Container; }
	virtual FText GetTitle() const override;
	virtual UPackage* GetPackage() const override;
	virtual TArray<FKzParamDef> GetContext() const override;
	virtual void SetContext(const TArray<FKzParamDef>& Entries) override;
	virtual TArray<FKzNamedVariant>* GetLocals() const override;
	virtual void BeginChange(ETab Tab) override;
	virtual void EndChange(ETab Tab) override;
	virtual void ForEachObjectInScope(ETab Tab, TFunctionRef<void(UScriptableObject&)> Visit) const override;

private:
	FScriptableContainer* GetContainer() const;

	TSharedPtr<IPropertyHandle> Handle;
	TArray<TWeakObjectPtr<UObject>> Owners;
	FScriptableContainer* Container = nullptr;
	const UScriptStruct* ContainerStruct = nullptr;
};