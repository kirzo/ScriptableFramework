// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/PropertyBag.h"
#include "Core/KzParamDef.h"
#include "Core/KzNamedVariant.h"
#include "Core/KzPropertyBagHelpers.h"
#include "ScriptableContainer.generated.h"

class UScriptableObject;
struct FScriptableContext;

/** Base struct for any container that provides a Context. */
USTRUCT(BlueprintType)
struct SCRIPTABLEFRAMEWORK_API FScriptableContainer
{
	GENERATED_BODY()

public:
	/** Defines the input parameters required by this container. */
	UPROPERTY(EditAnywhere, Category = "Context", meta = (TitleProperty = "Name"))
	TSet<FKzParamDef> ContextDefinitions;

	/** Shared memory (Blackboard) for this scope. */
	UPROPERTY(Transient)
	FInstancedPropertyBag Context;

	/** Per-instance mutable state variables. Seeded with each entry's default value at Register. */
	UPROPERTY(EditAnywhere, Category = "Locals", meta = (TitleProperty = "Name"))
	TArray<FKzNamedVariant> LocalsDefinitions;

	/** Runtime mutable bag for LocalsDefinitions. Written by SetLocal-style tasks, read by bindings via FScriptableRuntimeData. */
	UPROPERTY(Transient)
	FInstancedPropertyBag Locals;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UObject> Owner = nullptr;

private:
	/** Runtime lookup map for Sibling bindings (Guid -> Object Instance). */
	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UScriptableObject>> BindingSourceMap;

	/** Optional external Locals bag that overrides the local one. Set by wrappers (nested actions / requirements) that share the parent scope's Locals with their inner tasks. Plain ptr because it points at a UPROPERTY of an outer scope which outlives us during the wrapper's lifetime. */
	FInstancedPropertyBag* InheritedLocalsBag = nullptr;

	/** Optional external Context bag handed to children instead of the local one. Set by nested wrappers so inner units read the parent scope's context without copying it. */
	const FInstancedPropertyBag* InheritedContextBag = nullptr;

public:
	bool HasContext() const { return Context.IsValid(); }

	/** Returns the bag, rebuilding it from ContextDefinitions if it is out of sync (transient bag empty after load). */
	FInstancedPropertyBag& GetContext()
	{
		if (IsContextBagOutOfSync()) { ConstructContext(); }
		return Context;
	}
	const FInstancedPropertyBag& GetContext() const { return Context; }

	/** True when the transient bag does not match the persisted ContextDefinitions (e.g. just after load). */
	bool IsContextBagOutOfSync() const;

	/** Returns the runtime Locals bag. Resolves to the inherited bag when one was injected (nested-action style); otherwise rebuilds the local shape + initial values from LocalsDefinitions on demand. */
	FInstancedPropertyBag& GetLocals()
	{
		if (InheritedLocalsBag) return *InheritedLocalsBag;
		if (IsLocalsBagOutOfSync()) { ConstructLocals(); }
		return Locals;
	}
	const FInstancedPropertyBag& GetLocals() const
	{
		return InheritedLocalsBag ? *InheritedLocalsBag : Locals;
	}

	/** True when the transient Locals bag does not match the persisted LocalsDefinitions. */
	bool IsLocalsBagOutOfSync() const;

	/** Rebuilds the Locals bag's shape from LocalsDefinitions and writes each entry's default value. */
	void ConstructLocals();

	void ResetLocals() { Locals.Reset(); }

	/** Points GetLocals at an external bag instead of the local one. Used by nested wrappers to share the parent scope's Locals with inner tasks (write-through). Pass null to revert. Cleared on Unregister. */
	void SetInheritedLocals(FInstancedPropertyBag* External) { InheritedLocalsBag = External; }
	FInstancedPropertyBag* GetInheritedLocals() const { return InheritedLocalsBag; }

	/** Hands an external Context bag to the children registered next, taking precedence over the local one. Cleared on Unregister. */
	void SetInheritedContext(const FInstancedPropertyBag* External) { InheritedContextBag = External; }

	bool HasContextProperty(const FName& Name) const
	{
		return Context.FindPropertyDescByName(Name) != nullptr;
	}

	void ResetContext()
	{
		Context.Reset();
	}

	void ConstructContext();

	template <typename T>
	void AddContextProperty(const FName& Name)
	{
		ContextDefinitions.Add(FKzParamDef::Make<T>(Name));
		ConstructContext();
	}

	template <typename T>
	void SetContextProperty(const FName& Name, const T& Value)
	{
		KzPropertyBag::Set(Context, Name, Value);
	}

	template <typename T>
	T GetContextProperty(const FName& Name) const
	{
		auto Result = KzPropertyBag::Get<T>(Context, Name);
		return Result.HasValue() ? Result.GetValue() : T();
	}

	/** Copies the shape definitions from the given context into this container, then reconstructs the internal bag. */
	void AddContext(const FScriptableContext& InContext);

	/** Copies the values from the given context into this container. */
	void SetContext(const FScriptableContext& InContext);

	/** Finds a registered object by its persistent ID (used by Property Bindings). */
	UScriptableObject* FindBindingSource(const FGuid& InID) const;

	/** Adds outer-scope sources (e.g. a graph's other nodes) to the children's lookup map. Call after Register; Unregister drops them. */
	void AppendBindingSources(const TMap<FGuid, TObjectPtr<UScriptableObject>>& Sources) { BindingSourceMap.Append(Sources); }

protected:
	/** Populates the map and initializes the child with this context. */
	void AddBindingSource(UScriptableObject* InSource);

public:
	/** Initializes the container. */
	void Register(UObject* InOwner);

	/** Cleans up the container and clears the Binding Map. */
	void Unregister();
};