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

public:
	UScriptableObject();

	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual UWorld* GetWorld() const override final { return (WorldPrivate ? WorldPrivate : GetWorld_Uncached()); }

	// -------------------------------------------------------------------
	//  Registration & Lifecycle
	// -------------------------------------------------------------------
public:
	/** Register this object with an owner and world. */
	void Register(UObject* Owner);

	/** Unregister this object. */
	void Unregister();

	/** Registers an object with a specific world manually. */
	void RegisterObjectWithWorld(UWorld* InWorld);

	/** See if this object is currently registered. */
	FORCEINLINE virtual bool IsRegistered() const { return bRegistered; }

	/** Returns true if the object is enabled. */
	FORCEINLINE bool IsEnabled() const { return bEnabled; }

protected:
	/** Called when an object is registered. Override to initialize logic. */
	virtual void OnRegister() {}

	/** Called when a object is unregistered. Override to cleanup logic. */
	virtual void OnUnregister() {}

	// -------------------------------------------------------------------
	//  Hierarchy & Ownership
	// -------------------------------------------------------------------
public:
	UFUNCTION(BlueprintCallable, Category = ScriptableObject)
	UScriptableObject* GetRoot() const;

	UFUNCTION(BlueprintCallable, Category = ScriptableObject)
	FORCEINLINE_DEBUGGABLE UObject* GetOwner() const { return OwnerPrivate; }

	UFUNCTION(BlueprintCallable, Category = ScriptableObject)
	FORCEINLINE_DEBUGGABLE AActor* GetOwnerActor() const { return GetOwner<AActor>(); }

	/** Templated version of GetOwner(), will return nullptr if cast fails */
	template<class T>
	T* GetOwner() const { return Cast<T>(GetOwner()); }

	// -------------------------------------------------------------------
	//  Ticking System
	// -------------------------------------------------------------------
public:
	virtual void Tick(float DeltaTime);

	/** Set up a tick function. Returns true if criteria met. */
	bool SetupTickFunction(struct FTickFunction* TickFunction);

	FORCEINLINE virtual bool CanEverTick() const { return bCanEverTick; }
	FORCEINLINE virtual bool IsReadyToTick() const { return true; }

protected:
	/** Virtual call chain to register all tick functions */
	virtual void RegisterTickFunctions(bool bRegister);

	/** Event called every frame if tick is enabled */
	UFUNCTION(BlueprintImplementableEvent, Category = Tick, meta = (DisplayName = "Tick"))
	void ReceiveTick(float DeltaSeconds);

	// -------------------------------------------------------------------
	//  Data Binding & Context
	// -------------------------------------------------------------------
public:
	/** Returns the persistent binding ID. */
	FGuid GetBindingID() const { return BindingID; }

	/** Resolves and applies bindings (copies data from sources to this object). */
	void ResolveBindings();

	// --- Context Accessors ---

	FInstancedPropertyBag& GetContext() { return Context; }
	const FInstancedPropertyBag& GetContext() const { return Context; }

	bool HasContextProperty(const FName& Name) const
	{
		return Context.FindPropertyDescByName(Name) != nullptr;
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

	// --- Runtime Binding Resolution (Root Only Logic) ---

	/** Registers a object into the O(1) binding map. */
	void RegisterBindingSource(const FGuid& InID, UScriptableObject* InSource);

	/** Unregisters a task from the binding map. */
	void UnregisterBindingSource(const FGuid& InID);

	/** Finds a registered task by its persistent ID. */
	UScriptableObject* FindBindingSource(const FGuid& InID);

#if WITH_EDITOR
	/** Accessor for the editor module to modify bindings directly. */
	FScriptablePropertyBindings& GetPropertyBindings() { return PropertyBindings; }
	const FScriptablePropertyBindings& GetPropertyBindings() const { return PropertyBindings; }
#endif

	// -------------------------------------------------------------------
	//  Member Variables
	// -------------------------------------------------------------------
protected:
	/** Configuration: Enabled state */
	UPROPERTY(EditAnywhere, Category = Hidden, meta = (NoBinding))
	uint8 bEnabled : 1 = true;

	/** Configuration: Tick capability */
	UPROPERTY(EditDefaultsOnly, Category = Tick, meta = (NoBinding))
	uint8 bCanEverTick : 1 = false;

	/** Runtime: Registration state */
	uint8 bRegistered : 1 = false;

	/** Main tick function for the object */
	UPROPERTY(EditDefaultsOnly, Category = Tick, meta = (ShowOnlyInnerProperties, NoBinding))
	FScriptableObjectTickFunction PrimaryObjectTick;

private:
	/** Unique identifier for bindings. Persists across duplication. */
	UPROPERTY()
	FGuid BindingID;

	/** Input data (Context) available for this object and its children. */
	UPROPERTY(EditAnywhere, Category = Hidden, meta = (NoBinding))
	FInstancedPropertyBag Context;

	/** Data bindings definition. */
	UPROPERTY()
	FScriptablePropertyBindings PropertyBindings;

	/**
	 * Lookup Map for Runtime Bindings.
	 * Only populated on the Root object. Transient.
	 */
	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UScriptableObject>> BindingSourceMap;

	/** Cached pointers */
	UObject* OwnerPrivate = nullptr;
	UWorld* WorldPrivate = nullptr;
	FDelegateHandle OnWorldBeginTearDownHandle;

	/** Internal helpers */
	UWorld* GetWorld_Uncached() const;
	void OnWorldBeginTearDown(UWorld* InWorld);
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