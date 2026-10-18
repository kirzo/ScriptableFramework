// Copyright 2026 kirzo

#include "ScriptableObject.h"
#include "ScriptableContainer.h"
#include "ScriptablePropertyUtilities.h"
#include "ScriptableRuntimeData.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/SecureHash.h"
#include "UObject/ObjectSaveContext.h"

DEFINE_LOG_CATEGORY(LogScriptableObject);

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

UScriptableObject::UScriptableObject()
{
	bCanEverTick = false;
	PrimaryObjectTick.bCanEverTick = true;
}

void UScriptableObject::PostInitProperties()
{
	Super::PostInitProperties();

	// If this is a new object and doesn't have an ID, generate one.
	// Note: When Instancing (DuplicateObject), the ID is copied from the template,
	// so this check will be skipped, preserving the original ID.
	if (!BindingID.IsValid())
	{
		BindingID = FGuid::NewGuid();
	}
}

void UScriptableObject::PostLoad()
{
	Super::PostLoad();

	// Ensure legacy objects loaded from disk get an ID assigned.
	if (!BindingID.IsValid())
	{
		BindingID = FGuid::NewGuid();
	}
}

void UScriptableObject::PostEditImport()
{
	Super::PostEditImport();
	BindingID = FGuid::NewGuid();
}

#if WITH_EDITOR
void UScriptableObject::BakeAutoBindings()
{
	// No world = design-time owner (CDO/archetype, or an asset subobject like a graph task) -> bake.
	// With a world: never at runtime (PIE/game); an editor-level instance bakes only if its owning
	// container is instance-editable, otherwise it inherits the template's bindings.
	if (UWorld* World = GetWorld())
	{
		if (World->IsGameWorld())
		{
			return;
		}
		if (!FScriptablePropertyUtilities::IsOwningContainerInstanceEditable(this))
		{
			return;
		}
	}

	bool bChanged = false;

	// 1. Purge existing automatic bindings to prevent stale data
	GetPropertyBindings().ClearAutoBindings();

	// 2. Gather all accessible external contexts
	TArray<FPropertyBindingBindableStructDescriptor> AccessibleStructs;
	FScriptablePropertyUtilities::GatherAccessibleStructs(this, AccessibleStructs);

	// 3. Iterate through all properties to rebuild auto-bindings
	for (TFieldIterator<FProperty> It(GetClass()); It; ++It)
	{
		const FProperty* Prop = *It;

		if (FScriptablePropertyUtilities::IsPropertyBindableContext(Prop))
		{
			FPropertyBindingPath TargetPath;
			TargetPath.SetStructID(GetBindingID());
			TargetPath.AddPathSegment(Prop->GetFName());

			// If it does NOT have a manual override
			if (!GetPropertyBindings().HasManualPropertyBinding(TargetPath))
			{
				FPropertyBindingPath AutoPath;
				if (FScriptablePropertyUtilities::FindAutoBindingPath(Prop, AccessibleStructs, AutoPath))
				{
					// Inject the discovered binding explicitly marking it as Automatic
					GetPropertyBindings().AddPropertyBinding(AutoPath, TargetPath, true);
					bChanged = true;
				}
			}
		}
	}

	// Flag the object as modified so the serialization system knows it needs saving
	if (bChanged)
	{
		Modify();
	}
}

void UScriptableObject::PreSave(FObjectPreSaveContext SaveContext)
{
	Super::PreSave(SaveContext);
	BakeAutoBindings();
}

void UScriptableObject::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	BakeAutoBindings();
}

void UScriptableObject::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	// Detect changes in Arrays (Remove or Clear)
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayRemove || PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayClear)
	{
		const FProperty* ArrayProperty = nullptr;

		// Usually, the property associated with the event in an Array change is the Array property itself.
		if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->IsA<FArrayProperty>())
		{
			ArrayProperty = PropertyChangedEvent.Property;
		}

		if (ArrayProperty)
		{
			const FName ArrayName = ArrayProperty->GetFName();

			if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayRemove)
			{
				// Unreal stores the removed index in the event object using the property name
				const int32 RemovedIndex = PropertyChangedEvent.GetArrayIndex(ArrayName.ToString());
				PropertyBindings.HandleArrayElementRemoved(ArrayName, RemovedIndex);
			}
			else if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayClear)
			{
				PropertyBindings.HandleArrayClear(ArrayName);
			}
		}
	}

	BakeAutoBindings();
}

void UScriptableObject::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	// A PIE duplicate inherits the original's bindings and may not be parented to its world yet here,
	// so the world gate in BakeAutoBindings can't catch it — skip explicitly. Editor duplicates rebake.
	if (!bDuplicateForPIE)
	{
		BakeAutoBindings();
	}
}

FText UScriptableObject::GetDisplayTitle() const
{
	return GetClass()->GetDisplayNameText();
}

bool UScriptableObject::GetBindingDisplayText(FName PropertyName, FString& OutText, bool bChopPrefix) const
{
	FPropertyBindingPath TargetPath;
	// Set the ID of this object instance so the binding system can find the correct entry
	TargetPath.SetStructID(GetBindingID());
	TargetPath.AddPathSegment(PropertyName);

	const FPropertyBindingPath* SourcePath = GetPropertyBindings().GetPropertyBinding(TargetPath);

	// If no explicit binding is cached yet (e.g., node just dropped in the graph),
	// attempt to dynamically discover the automatic binding.
	if (!SourcePath)
	{
		if (const FProperty* Prop = GetClass()->FindPropertyByName(PropertyName))
		{
			// Build the accessible structs array for external context search
			TArray<FPropertyBindingBindableStructDescriptor> AccessibleStructs;
			FScriptablePropertyUtilities::GatherAccessibleStructs(this, AccessibleStructs);
			
			FPropertyBindingPath DiscoveredPath;
			if (FScriptablePropertyUtilities::FindAutoBindingPath(Prop, AccessibleStructs, DiscoveredPath))
			{
				SourcePath = &DiscoveredPath;
			}
		}
	}

	// Check if there is a binding for this property
	if (SourcePath)
	{
		FString FullPath = SourcePath->ToString();

		if (bChopPrefix)
		{
			int32 LastDotIndex;
			if (FullPath.FindLastChar('.', LastDotIndex))
			{
				// Return only the substring after the last dot
				OutText = FullPath.RightChop(LastDotIndex + 1);
			}
			else
			{
				OutText = FullPath;
			}
		}
		else
		{
			OutText = FullPath;
		}

		int32 LastUnderscoreIndex;
		if (OutText.FindLastChar('_', LastUnderscoreIndex))
		{
			if (OutText.RightChop(LastUnderscoreIndex + 1).IsNumeric())
			{
				OutText = OutText.Left(LastUnderscoreIndex);
			}
		}

		return true;
	}

	return false;
}
#endif

UWorld* UScriptableObject::GetWorld_Uncached() const
{
	// The CDO has no world. Return early WITHOUT delegating to any base GetWorld(): the editor's
	// ImplementsGetWorld() (used to hide the BlueprintCallable WorldContext pin) runs on the CDO and
	// detects our override by watching a thread-local flag that UObject::GetWorld() clears. Climbing
	// to an outer here would hit that base implementation and make the editor think we lack a world.
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	// 1. Attempt to get the world from the explicit Owner (if registered)
	UObject* MyOwner = GetOwner();
	if (MyOwner && !MyOwner->HasAnyFlags(RF_ClassDefaultObject))
	{
		if (UWorld* OwnerWorld = MyOwner->GetWorld())
		{
			return OwnerWorld;
		}
	}

	// 2. Fallback: Climb up the Outer hierarchy
	UObject* CurrentOuter = GetOuter();
	while (CurrentOuter)
	{
		if (CurrentOuter->HasAnyFlags(RF_ClassDefaultObject))
		{
			return nullptr;
		}

		// If by any chance the Outer is the World itself
		if (UWorld* AsWorld = Cast<UWorld>(CurrentOuter))
		{
			return AsWorld;
		}

		// Ask the current Outer for its world.
		if (UWorld* OuterWorld = CurrentOuter->GetWorld())
		{
			return OuterWorld;
		}

		// Keep climbing up the hierarchy
		CurrentOuter = CurrentOuter->GetOuter();
	}

	return nullptr;
}

void UScriptableObject::OnWorldBeginTearDown(UWorld* InWorld)
{
	if (IsRegistered() && InWorld == WorldPrivate)
	{
		Unregister();
	}
}

// -------------------------------------------------------------------
//  Registration & Lifecycle
// -------------------------------------------------------------------

void UScriptableObject::Register(UObject* Owner)
{
	OwnerPrivate = Owner;
	UWorld* MyOwnerWorld = (OwnerPrivate ? OwnerPrivate->GetWorld() : nullptr);
	if (ensure(MyOwnerWorld))
	{
		RegisterObjectWithWorld(MyOwnerWorld);

		if (IsRegistered())
		{
			OnRegister();
		}
	}
}

void UScriptableObject::Unregister()
{
	FScopeCycleCounterUObject ObjectScope(this);

	// Do nothing if not registered
	if (!IsRegistered())
	{
		UE_LOG(LogScriptableObject, Log, TEXT("Unregister: (%s) not registered. Aborting."), *GetPathName());
		return;
	}

	RegisterTickFunctions(false);

	// If registered, should have a world
	checkf(WorldPrivate != nullptr, TEXT("%s"), *GetFullName());

	WorldPrivate = nullptr;
	bRegistered = false;

	ContextRef = nullptr;
	BindingsMapRef = nullptr;

	OnUnregister();
}

void UScriptableObject::RegisterObjectWithWorld(UWorld* InWorld)
{
	checkf(!IsUnreachable(), TEXT("%s"), *GetFullName());

	if (!IsValid(this))
	{
		UE_LOG(LogScriptableObject, Log, TEXT("RegisterObjectWithWorld: (%s) Trying to register with IsValid() == false. Aborting."), *GetPathName());
		return;
	}

	// If the object was already registered, do nothing
	if (IsRegistered())
	{
		UE_LOG(LogScriptableObject, Log, TEXT("RegisterObjectWithWorld: (%s) Already registered. Aborting."), *GetPathName());
		return;
	}

	if (InWorld == nullptr)
	{
		UE_LOG(LogScriptableObject, Log, TEXT("RegisterObjectWithWorld: (%s) NULL InWorld specified. Aborting."), *GetPathName());
		return;
	}

	// If not registered, should not have a scene
	checkf(WorldPrivate == nullptr, TEXT("%s"), *GetFullName());

	UObject* MyOwner = GetOwner();

	if (MyOwner && MyOwner->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists))
	{
		UE_LOG(LogScriptableObject, Log, TEXT("RegisterObjectWithWorld: Owner belongs to a DEADCLASS"));
		return;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Can only register with an Actor if we are created within one
	if (MyOwner)
	{
		checkf(!MyOwner->IsUnreachable(), TEXT("%s"), *GetFullName());
		// can happen with undo because the owner will be restored "next"
		// checkf(IsValid(MyOwner), TEXT("%s"), *GetFullName());

		if (InWorld != MyOwner->GetWorld())
		{
			// The only time you should specify a scene that is not Owner->GetWorld() is when you don't have an Actor
			UE_LOG(LogScriptableObject, Log, TEXT("RegisterObjectWithWorld: (%s) Specifying a world, but an Owner Actor found, and InWorld is not GetOwner()->GetWorld()"), *GetPathName());
		}
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	WorldPrivate = InWorld;

	// Once per object: requirements register on every evaluation, and Remove scans the whole global list.
	if (!OnWorldBeginTearDownHandle.IsValid())
	{
		OnWorldBeginTearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &UScriptableObject::OnWorldBeginTearDown);
	}

	bRegistered = true;
}

// -------------------------------------------------------------------
//  Hierarchy & Ownership
// -------------------------------------------------------------------

UScriptableObject* UScriptableObject::GetRoot() const
{
	UObject* Top = const_cast<UObject*>(Cast<UObject>(this));
	for (;;)
	{
		UObject* CurrentOuter = Top->GetOuter();
		if (!IsValid(CurrentOuter) || !CurrentOuter->IsA<UScriptableObject>())
		{
			return Cast<UScriptableObject>(Top);
		}
		Top = CurrentOuter;
	}
}

// -------------------------------------------------------------------
//  Ticking System
// -------------------------------------------------------------------

void UScriptableObject::Tick(float DeltaTime)
{
	ReceiveTick(DeltaTime);
}

bool UScriptableObject::SetupTickFunction(FTickFunction* TickFunction)
{
	if (CanEverTick() && TickFunction->bCanEverTick && !IsTemplate() && IsRegistered())
	{
		ULevel* Level = ToRawPtr(GetWorld()->PersistentLevel);
		TickFunction->SetTickFunctionEnable(TickFunction->bStartWithTickEnabled || TickFunction->IsTickFunctionEnabled());
		TickFunction->RegisterTickFunction(Level);
		return true;
	}
	return false;
}

void UScriptableObject::RegisterTickFunctions(bool bRegister)
{
	if (bRegister)
	{
		if (SetupTickFunction(&PrimaryObjectTick))
		{
			PrimaryObjectTick.Target = this;
		}
	}
	else
	{
		if (PrimaryObjectTick.IsTickFunctionRegistered())
		{
			PrimaryObjectTick.UnRegisterTickFunction();
		}
	}
}

void FScriptableObjectTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FScriptableObjectTickFunction::ExecuteTick);
	ExecuteTickHelper(Target, /*Target->bTickInEditor*/false, DeltaTime, TickType, [this, TickType](float DilatedTime)
	{
		Target->Tick(DilatedTime);
	});
}

FString FScriptableObjectTickFunction::DiagnosticMessage()
{
	return Target->GetFullName() + TEXT("[Tick]");
}

FName FScriptableObjectTickFunction::DiagnosticContext(bool bDetailed)
{
	if (bDetailed)
	{
		UObject* OwningObject = Target->GetOwner();
		FString OwnerClassName = OwningObject ? OwningObject->GetClass()->GetName() : TEXT("None");
		// Format is "Class/OwningObjectClass/ObjectName"
		FString ContextString = FString::Printf(TEXT("%s/%s/%s"), *Target->GetClass()->GetName(), *OwnerClassName, *Target->GetName());
		return FName(*ContextString);
	}
	else
	{
		return Target->GetClass()->GetFName();
	}
}

// -------------------------------------------------------------------
//  Data Binding & Context
// -------------------------------------------------------------------

void UScriptableObject::InitRuntimeData(const FScriptableRuntimeData& InData)
{
	ContextRef = InData.Context;
	LocalsRef = InData.Locals;
	BindingsMapRef = InData.BindingsMap;
}

void UScriptableObject::PropagateRuntimeData(UScriptableObject* Child) const
{
	if (!Child) return;

	FScriptableRuntimeData Data;
	Data.Context = ContextRef;
	Data.Locals = LocalsRef;
	Data.BindingsMap = BindingsMapRef;
	Child->InitRuntimeData(Data);
}

void UScriptableObject::ResolveBindings()
{
	PreResolveBindings();
	PropertyBindings.ResolveBindings(this);
}

UScriptableObject* UScriptableObject::FindBindingSource(const FGuid& InID)
{
	if (BindingsMapRef)
	{
		if (const TObjectPtr<UScriptableObject>* Found = BindingsMapRef->Find(InID))
		{
			return Found->Get();
		}
	}
	return nullptr;
}