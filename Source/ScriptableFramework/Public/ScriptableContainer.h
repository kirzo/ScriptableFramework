// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/PropertyBag.h"
#include "Utils/PropertyBagHelpers.h"
#include "ScriptableContainer.generated.h"

class UScriptableObject;

/** Base struct for any container that provides a Context. */
USTRUCT(BlueprintType)
struct SCRIPTABLEFRAMEWORK_API FScriptableContainer
{
	GENERATED_BODY()

public:
	/** Shared memory (Blackboard) for this scope. */
	UPROPERTY(EditAnywhere, Category = "Config")
	FInstancedPropertyBag Context;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UObject> Owner = nullptr;

private:
	/** Runtime lookup map for Sibling bindings (Guid -> Object Instance). */
	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UScriptableObject>> BindingSourceMap;

public:
	bool HasContext() const { return Context.IsValid(); }

	FInstancedPropertyBag& GetContext() { return Context; }
	const FInstancedPropertyBag& GetContext() const { return Context; }

	bool HasContextProperty(const FName& Name) const
	{
		return Context.FindPropertyDescByName(Name) != nullptr;
	}

	void ResetContext()
	{
		Context.Reset();
	}

	template <typename T>
	void AddContextProperty(const FName& Name)
	{
		ScriptablePropertyBag::Add<T>(Context, Name);
	}

	template <typename T>
	void SetContextProperty(const FName& Name, const T& Value)
	{
		ScriptablePropertyBag::Set(Context, Name, Value);
	}

	template <typename T>
	T GetContextProperty(const FName& Name) const
	{
		auto Result = ScriptablePropertyBag::Get<T>(Context, Name);
		return Result.HasValue() ? Result.GetValue() : T();
	}

	/** Finds a registered object by its persistent ID (used by Property Bindings). */
	UScriptableObject* FindBindingSource(const FGuid& InID) const;

protected:
	/** Populates the map and initializes the child with this context. */
	void AddBindingSource(UScriptableObject* InSource);

public:
	/** Initializes the container. */
	void Register(UObject* InOwner);

	/** Cleans up the container and clears the Binding Map. */
	void Unregister();
};