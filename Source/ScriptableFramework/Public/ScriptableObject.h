// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "StructUtils/PropertyBag.h"
#include "ScriptableObjectTypes.h"
#include "PropertyBindingPath.h"
#include "Bindings/ScriptablePropertyBindings.h"
#include "ScriptableObject.generated.h"

SCRIPTABLEFRAMEWORK_API DECLARE_LOG_CATEGORY_EXTERN(LogScriptableObject, Log, All);

struct FBindableStructDesc;

/** Base class for all scriptable objects in the framework. */
UCLASS(Abstract, DefaultToInstanced, EditInlineNew, Blueprintable, BlueprintType, HideCategories = (Hidden), CollapseCategories)
class SCRIPTABLEFRAMEWORK_API UScriptableObject : public UObject
{
	GENERATED_BODY()

	friend class UScriptableCondition;

protected:
	/** Indicates if this object is currently registered with a scene. */
	uint8 bRegistered : 1 = false;

	UPROPERTY(EditDefaultsOnly, Category = Tick)
	uint8 bCanEverTick : 1 = false;

	/** Main tick function for the object */
	UPROPERTY(EditDefaultsOnly, Category = Tick, meta = (ShowOnlyInnerProperties))
	FScriptableObjectTickFunction PrimaryObjectTick;

	UPROPERTY(EditAnywhere, Category = Hidden)
	uint8 bEnabled : 1 = true;

	/** Input data (Context) available for this object and its children. */
	UPROPERTY(EditDefaultsOnly, Category = "Context", meta = (FixedLayout, NoBinding))
	FInstancedPropertyBag Context;

private:
	/** Data bindings definition (which property copies from where). */
	UPROPERTY()
	FScriptablePropertyBindings PropertyBindings;

	/** Cached pointer to owning object */
	UObject* OwnerPrivate;

	/**
	 * Pointer to the world that this object is currently registered with.
	 * This is only non-NULL when the object is registered.
	 */
	UWorld* WorldPrivate;

	/** If WorldPrivate isn't set this will determine the world from outers */
	UWorld* GetWorld_Uncached() const;

	FDelegateHandle OnWorldBeginTearDownHandle;
	void OnWorldBeginTearDown(UWorld* InWorld);

public:
	UScriptableObject();

	FORCEINLINE bool IsEnabled() const { return bEnabled; }

	/** See if this component is currently registered */
	FORCEINLINE bool virtual IsRegistered() const { return bRegistered; }

	FORCEINLINE virtual bool CanEverTick() const { return bCanEverTick; }
	FORCEINLINE virtual bool IsReadyToTick() const { return true; }

	/** Finds the root object of the hierarchy. */
	UFUNCTION(BlueprintCallable, Category = ScriptableObject)
	UScriptableObject* GetRoot() const;

	/** Returns the mutable reference to the Context property bag. */
	FInstancedPropertyBag& GetContext() { return Context; }

	/** Resolves and applies bindings (copies data from the context to the properties). */
	void ResolveBindings();

	/** Retrieves the structs accessible for binding from this object. */
	virtual void GetAccessibleStructs(const UObject* TargetOuterObject, TArray<FBindableStructDesc>& OutStructDescs) const;

#if WITH_EDITOR
	/** Accessor for the editor module to modify bindings directly. */
	FScriptablePropertyBindings& GetPropertyBindings() { return PropertyBindings; }
	const FScriptablePropertyBindings& GetPropertyBindings() const { return PropertyBindings; }
#endif

	/** Register this object. */
	void Register(UObject* Owner);

	/** Unregister this object. */
	void Unregister();

	/**
	 * Registers an object with a specific world.
	 * @param InWorld - The world to register the object with.
	 */
	void RegisterObjectWithWorld(UWorld* InWorld);

protected:
	/** Called when an object is registered, after Scene is set. */
	virtual void OnRegister() {}

	/** Called when a object is unregistered. */
	virtual void OnUnregister() {}

public:
	UFUNCTION(BlueprintCallable, Category = ScriptableObject)
	FORCEINLINE_DEBUGGABLE UObject* GetOwner() const { return OwnerPrivate; }

	UFUNCTION(BlueprintCallable, Category = ScriptableObject)
	FORCEINLINE_DEBUGGABLE AActor* GetOwnerActor() const { return GetOwner<AActor>(); }

	/** Templated version of GetOwner(), will return nullptr if cast fails */
	template<class T> T* GetOwner() const { return Cast<T>(GetOwner()); }

	/** Getter for the cached world pointer */
	virtual UWorld* GetWorld() const override final { return (WorldPrivate ? WorldPrivate : GetWorld_Uncached()); }

public:
	virtual void Tick(float DeltaTime);

	/**
	 * Set up a tick function for a object in the standard way.
	 * Don't tick if this is a "NeverTick" object.
	 * @param	TickFunction - structure holding the specific tick function
	 * @return  true if this object met the criteria for actually being ticked.
	 */
	bool SetupTickFunction(struct FTickFunction* TickFunction);

protected:
	/**
	 * Virtual call chain to register all tick functions
	 * @param bRegister - true to register, false, to unregister
	 */
	virtual void RegisterTickFunctions(bool bRegister);

	/** Event called every frame if tick is enabled */
	UFUNCTION(BlueprintImplementableEvent, Category = Tick, meta = (DisplayName = "Tick"))
	void ReceiveTick(float DeltaSeconds);
};

/** Helper function for executing task tick functions */
template<typename ExecuteTickLambda>
void FScriptableObjectTickFunction::ExecuteTickHelper(UScriptableObject* Target, bool bTickInEditor, float DeltaTime, ELevelTick TickType, const ExecuteTickLambda& ExecuteTickFunc)
{
	if (IsValid(Target))
	{
		FScopeCycleCounterUObject TaskScope(Target);

		if (Target->IsRegistered() && Target->IsReadyToTick())
		{
			AActor* MyOwner = Target->GetOwner<AActor>();
			if (TickType != LEVELTICK_ViewportsOnly || bTickInEditor ||
					(MyOwner && MyOwner->ShouldTickIfViewportsOnly()))
			{
				const float TimeDilation = (MyOwner ? MyOwner->CustomTimeDilation : 1.f);
				ExecuteTickFunc(DeltaTime * TimeDilation);
			}
		}
	}
}