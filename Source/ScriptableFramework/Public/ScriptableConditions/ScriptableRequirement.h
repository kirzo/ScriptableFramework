// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/PropertyBag.h"
#include "Utils/PropertyBagHelpers.h"
#include "ScriptableRequirement.generated.h"

class UScriptableCondition;
class UScriptableObject;

/** Logical operation for the requirement group. */
UENUM(BlueprintType)
enum class EScriptableRequirementMode : uint8
{
	And,
	Or
};

/** A container for a list of conditions with a logic operation (AND/OR). */
USTRUCT(BlueprintType)
struct SCRIPTABLEFRAMEWORK_API FScriptableRequirement
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Logic")
	EScriptableRequirementMode Mode = EScriptableRequirementMode::And;

	/** If true, the result of the entire group evaluation is inverted. */
	UPROPERTY(EditAnywhere, Category = "Logic")
	uint8 bNegate : 1 = false;

	/** Shared memory (Blackboard) for this requirement scope. */
	UPROPERTY(EditAnywhere, Category = "Config")
	FInstancedPropertyBag Context;

	UPROPERTY(EditAnywhere, Instanced, Category = "Conditions")
	TArray<TObjectPtr<UScriptableCondition>> Conditions;

private:
	UPROPERTY(Transient)
	uint8 bIsRegistered : 1 = false;

	UPROPERTY(Transient)
	TObjectPtr<UObject> Owner = nullptr;

	/** Map of conditions available for binding within this scope. */
	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UScriptableObject>> BindingSourceMap;

	// -------------------------------------------------------------------
	// API
	// -------------------------------------------------------------------
public:
	/** Registers all conditions and sets up the Binding Context. */
	void Register(UObject* InOwner);

	void Unregister();

	bool Evaluate() const;

	bool IsEmpty() const { return Conditions.IsEmpty(); }

	// --- Context & Binding Accessors ---

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

private:
	/** Populates the map and initializes the child with this context. */
	void AddBindingSource(UScriptableObject* InSource);

public:
	/** Static entry point to run an action. Handles registration and startup. */
	static bool EvaluateRequirement(UObject* Owner, FScriptableRequirement& Action);
};