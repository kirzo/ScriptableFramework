// Copyright 2026 kirzo

#include "ScriptableObject.h"
#include "Misc/SecureHash.h"

DEFINE_LOG_CATEGORY(LogScriptableObject);

UE_DISABLE_OPTIMIZATION

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

UWorld* UScriptableObject::GetWorld_Uncached() const
{
	UWorld* MyWorld = nullptr;

	UObject* MyOwner = GetOwner();
	// If we don't have a world yet, it may be because we haven't gotten registered yet, but we can try to look at our owner
	if (MyOwner && !MyOwner->HasAnyFlags(RF_ClassDefaultObject))
	{
		MyWorld = MyOwner->GetWorld();
	}

	if (MyWorld == nullptr)
	{
		// As a fallback check the outer of this object for a world.
		MyWorld = Cast<UWorld>(GetOuter());
	}

	return MyWorld;
}

void UScriptableObject::OnWorldBeginTearDown(UWorld* InWorld)
{
	if (InWorld == GetWorld())
	{
		Unregister();
	}
}

void UScriptableObject::RegisterBindingSource(const FGuid& InID, UScriptableObject* InSource)
{
	if (InID.IsValid() && InSource)
	{
		// If I am the Root, I store it. If I am a child, this function shouldn't be called directly on me
		// generally, but strictly speaking GetRoot() returns 'this' if I am the root.
		BindingSourceMap.Add(InID, InSource);
	}
}

void UScriptableObject::UnregisterBindingSource(const FGuid& InID)
{
	if (InID.IsValid())
	{
		BindingSourceMap.Remove(InID);
	}
}

UScriptableObject* UScriptableObject::FindBindingSource(const FGuid& InID)
{
	if (const TObjectPtr<UScriptableObject>* Found = BindingSourceMap.Find(InID))
	{
		return Found->Get();
	}
	return nullptr;
}

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

void UScriptableObject::ResolveBindings()
{
	PropertyBindings.ResolveBindings(this);
}

void UScriptableObject::Register(UObject* Owner)
{
	OwnerPrivate = Owner;
	UWorld* MyOwnerWorld = (OwnerPrivate ? OwnerPrivate->GetWorld() : nullptr);
	if (ensure(MyOwnerWorld))
	{
		RegisterObjectWithWorld(MyOwnerWorld);

		if (IsRegistered())
		{
			// Before calling OnRegister, we ensure this object is discoverable by the Root.
			if (BindingID.IsValid())
			{
				if (UScriptableObject* Root = GetRoot())
				{
					Root->RegisterBindingSource(BindingID, this);
				}
			}

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

	if (BindingID.IsValid())
	{
		if (UScriptableObject* Root = GetRoot())
		{
			Root->UnregisterBindingSource(BindingID);
		}
	}

	FWorldDelegates::OnWorldBeginTearDown.Remove(OnWorldBeginTearDownHandle);
	RegisterTickFunctions(false);

	// If registered, should have a world
	checkf(WorldPrivate != nullptr, TEXT("%s"), *GetFullName());

	WorldPrivate = nullptr;
	bRegistered = false;

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

	OnWorldBeginTearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &UScriptableObject::OnWorldBeginTearDown);

	bRegistered = true;
}

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

UE_ENABLE_OPTIMIZATION