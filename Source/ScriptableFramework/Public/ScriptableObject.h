// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "StructUtils/PropertyBag.h"
#include "ScriptableObjectTypes.h"
#include "PropertyBindingPath.h"
#include "Bindings/ScriptablePropertyBindings.h"
#include "Utils/PropertyBagHelpers.h"
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
	/**
	 * Unique identifier for this object instance.
	 * Used to resolve property bindings at runtime.
	 */
	UPROPERTY()
	FGuid BindingID;

	/** Data bindings definition (which property copies from where). */
	UPROPERTY()
	FScriptablePropertyBindings PropertyBindings;

	/**
	 * Map for O(1) runtime lookup of tasks by their persistent ID.
	 * Only populated on the Root object of the hierarchy.
	 * Transient because it is rebuilt dynamically via Register().
	 */
	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UScriptableObject>> BindingSourceMap;

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

	virtual void PostInitProperties() override;
	virtual void PostLoad() override;

	FORCEINLINE bool IsEnabled() const { return bEnabled; }

	/** See if this component is currently registered */
	FORCEINLINE bool virtual IsRegistered() const { return bRegistered; }

	FORCEINLINE virtual bool CanEverTick() const { return bCanEverTick; }
	FORCEINLINE virtual bool IsReadyToTick() const { return true; }

	/** Returns the persistent binding ID. */
	FGuid GetBindingID() const { return BindingID; }

	/** Registers a object into the binding map. */
	void RegisterBindingSource(const FGuid& InID, UScriptableObject* InSource);

	/** Unregisters a task from the binding map. */
	void UnregisterBindingSource(const FGuid& InID);

	/** Finds a registered task by its persistent ID. */
	UScriptableObject* FindBindingSource(const FGuid& InID);

	/** Finds the root object of the hierarchy. */
	UFUNCTION(BlueprintCallable, Category = ScriptableObject)
	UScriptableObject* GetRoot() const;

	/** Returns the mutable reference to the Context property bag. */
	FInstancedPropertyBag& GetContext() { return Context; }
	const FInstancedPropertyBag& GetContext() const { return Context; }

	/**
	 * Checks if the Context has a specific property.
	 */
	bool HasContextProperty(const FName& Name) const
	{
		return Context.FindPropertyDescByName(Name) != nullptr;
	}

	/**
	 * Adds a property definition to the Context without setting a specific value.
	 * Usage: Task->AddContextProperty<float>(TEXT("Health"));
	 */
	template <typename T>
	void AddContextProperty(const FName& Name)
	{
		ScriptablePropertyBag::Add<T>(Context, Name);
	}

	/**
	 * Sets a value in the Context Property Bag.
	 * Automatically adds the property if it doesn't exist.
	 * Usage: Task->SetContextProperty(TEXT("Health"), 100.0f);
	 */
	template <typename T>
	void SetContextProperty(const FName& Name, const T& Value)
	{
		ScriptablePropertyBag::Set(Context, Name, Value);
	}

	/**
	 * Retrieves a value from the Context Property Bag.
	 * Returns a default-constructed value if the property fails or doesn't exist.
	 * Usage: float Health = Task->GetContextProperty<float>(TEXT("Health"));
	 */
	template <typename T>
	T GetContextProperty(const FName& Name) const
	{
		auto Result = ScriptablePropertyBag::Get<T>(Context, Name);
		if (Result.HasValue())
		{
			return Result.GetValue();
		}
		return T();
	}

	/** Resolves and applies bindings (copies data from the context to the properties). */
	void ResolveBindings();

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