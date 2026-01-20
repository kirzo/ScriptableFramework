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

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;

	/**
	 * Returns a user-friendly title of this condition.
	 * Used by the Editor to display the condition in lists (e.g. "Health > 0").
	 */
	virtual FText GetDisplayTitle() const;

	/** 
	 * Helper to get the display string of a binding for a specific property.
	 * @param PropertyName The name of the property on this object to check.
	 * @param OutText The resulting string if a binding exists (e.g., "Health" or "Context.Target.Health").
	 * @param bChopPrefix If true, removes the path prefix and keeps only the leaf variable name (e.g., "Health").
	 * @return True if the property is bound, false otherwise.
	 */
	bool GetBindingDisplayText(FName PropertyName, FString& OutText, bool bChopPrefix = true) const;
#endif

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

	/** Injects the shared data from the owning container. */
	virtual void InitRuntimeData(const FInstancedPropertyBag* InContext, const TMap<FGuid, TObjectPtr<UScriptableObject>>* InBindingMap);

	/** Propagates the runtime data to a child object. */
	void PropagateRuntimeData(UScriptableObject* Child) const;

	/** Resolves and applies bindings (copies data from sources to this object). */
	void ResolveBindings();

	const FInstancedPropertyBag* GetContext() const { return ContextRef; }

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
	/** Input data (Context) available for this object and its children. */
	const FInstancedPropertyBag* ContextRef = nullptr;

	/** Reference to the Action's Binding Source Map. */
	const TMap<FGuid, TObjectPtr<UScriptableObject>>* BindingsMapRef = nullptr;

	/** Unique identifier for bindings. Persists across duplication. */
	UPROPERTY(DuplicateTransient, meta = (NoBinding))
	FGuid BindingID;
	
	/** Data bindings definition. */
	UPROPERTY(meta = (NoBinding))
	FScriptablePropertyBindings PropertyBindings;

	/** Cached pointers */
	UObject* OwnerPrivate = nullptr;
	UWorld* WorldPrivate = nullptr;
	FDelegateHandle OnWorldBeginTearDownHandle;

	/** Internal helpers */
	UWorld* GetWorld_Uncached() const;
	void OnWorldBeginTearDown(UWorld* InWorld);
};