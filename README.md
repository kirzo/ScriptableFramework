<p align="center">
  <img src="https://kirzo.dev/content/images/plugins/ScriptableFramework_banner.jpg" alt="ScriptableFramework Banner" width="512">
</p>

# Scriptable Framework for Unreal Engine

**Scriptable Framework** is a data-driven logic system for Unreal Engine. It empowers designers and programmers to embed modular logic sequences (**Actions**) and validation rules (**Requirements**) directly into Actors, Data Assets, or any `UObject`, without the overhead of creating unique Blueprint graphs for every specific interaction.

The system relies on a robust **Runtime Binding System** that allows properties within tasks and conditions to be dynamically bound to context variables defined by the host object, ensuring strict type safety and validation at editor time.

---

## 🌟 Key Features

### Embeddable Logic
Add `FScriptableAction` or `FScriptableRequirement` properties to your classes to instantly expose a logic editor in the **Details Panel**.

### Modular Architecture

- **Actions**  
  Sequences of Tasks (e.g., *Play Sound*, *Spawn Actor*, *Wait*).

- **Requirements**  
  Lists of Conditions (e.g., *Is Alive?*, *Has Ammo?*) evaluated with **AND / OR** logic.

### Typed Context Definition
Define the **Signature** (inputs) of your logic containers via C++.  
The editor uses this information to provide only valid binding options.

### Advanced Data Binding

- Bind properties inside your nodes to the defined **Context** variables.
- **Editor Validation** automatically detects:
  - Missing context parameters
  - Type mismatches (e.g., binding a `float` to a `bool`)
- **Deep Access**: supports binding to nested struct properties.

### No-Graph Editor
A clean, vertical list interface using heavily customized **Detail Views**.  
It feels like editing standard properties but behaves like a logic flow.

---

## 📦 Dependencies

- **Unreal Engine 5.5+**

### Installation
1.  Clone or download this repository.
2.  Copy the `ScriptableFramework` folder into your project's `Plugins/` directory (create the folder if it doesn't exist).
3.  Regenerate project files and compile.

---

## 🚀 Core Concepts

### 1. The Containers (The Core)

The framework revolves around two main structs that you embed in your classes:

- **`FScriptableAction`**  
  Holds and executes a sequence of `UScriptableTask`.

- **`FScriptableRequirement`**  
  Holds and evaluates a set of `UScriptableCondition`.

---

### 2. The Nodes

- **Task (`UScriptableTask`)**  
  The atomic unit of work. Executes logic.

- **Condition (`UScriptableCondition`)**  
  The atomic unit of logic. Checks a state and returns a boolean.

  Both `UScriptableTask` and `UScriptableCondition` are blueprintable.

---

### 3. The Assets (Convenience)

- **ScriptableActionAsset**
- **ScriptableRequirementAsset**

Data Assets that wrap the containers.  
Useful for defining reusable logic shared across multiple objects, although the system is designed primarily for containers embedded directly in Actors.

---

## 🕹️ Usage Guide

### Action Execution Modes
Control how tasks flow within an Action.

- **Sequence**: Executes tasks one by one. The next task begins only after the previous one finishes.

- **Parallel**: Fires all tasks simultaneously. The Action finishes when all tasks are complete.

### Task Lifecycle
Control the persistence and repetition of individual tasks within an Action

- **Once**: The task executes exactly one time and marks itself as "Completed." Even if the parent Action is triggered multiple times, this task will be skipped in subsequent runs until it is explicitly Reset.

- **Loop**: Upon finishing, the task immediately runs again. You can specify a fixed number of iterations (e.g., "Run 3 times") or set it to loop indefinitely.

<p align="center">
    <img src="https://kirzo.dev/content/images/plugins/ScriptableFramework/ScriptableFrameworkTasks.gif" width="768">
</p>

### Logic Gates (AND / OR)
Determine how a Requirement validates its list of conditions.

- **AND**: All conditions must be true for the Requirement to pass.

- **OR**: The Requirement passes if at least one condition is true.

### Inverting Requirements and Conditions (NOT)
Every requirement and condition has a built-in "Not" property. This allows you to instantly invert the logic (e.g., turning "Is Alive" into "Is Dead") directly in the editor without writing new C++ classes.

### Nested Requirements
Create complex logic trees by adding Sub-Requirements. This allows you to mix logic gates within a single check.

Example: Has Key **AND** (Is Door Unlocked **OR** Can Pick Lock)

<p align="center">
    <img src="https://kirzo.dev/content/images/plugins/ScriptableFramework/ScriptableFrameworkConditions.gif" width="768">
</p>

---

## 💻 C++ Integration Guide

To use the framework, add the containers as properties to your Actor (or any `UObject`).  
You define the **Context** (the variables available for binding in the editor).

At runtime, you are responsible for providing the actual data to the Context before executing an Action or evaluating a Requirement. The framework then resolves all bindings and runs the configured logic or conditions.

---

### Example: Embedding Logic in an Actor

```cpp
// .h
UCLASS()
class MYPROJECT_API AMyActor : public AActor, public IAbilitySystemInterface
{
    GENERATED_BODY()
    
public:
    // A standard property we want to expose to the logic
    UPROPERTY(EditAnywhere, Category = "MyActor")
    float Health = 100.0f;

    // A logic sequence (e.g., what happens when this actor spawns)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MyActor")
    FScriptableAction OnSpawnAction;

    // A logic sequence (e.g., what happens when this actor dies)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MyActor")
    FScriptableAction OnDeathAction;

    // A condition check (e.g., can this actor be interacted with?)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MyActor")
    FScriptableRequirement InteractionRequirement;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

public:
    AMyActor();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystemComponent; }

    // Runtime execution
    void OnSpawnActor();
    void OnKillActor();
    bool CanInteract();
};

// .cpp
AMyActor::AMyActor()
{
    PrimaryActorTick.bCanEverTick = true;

    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

    // Initialize Context
    OnSpawnAction.AddContextProperty<float>(TEXT("Health"));
    OnSpawnAction.AddContextProperty<FVector>(TEXT("Location"));
    OnSpawnAction.AddContextProperty<AMyActor*>(TEXT("Owner"));
    OnSpawnAction.AddContextProperty<APawn*>(TEXT("Instigator"));

    OnDeathAction.AddContextProperty<float>(TEXT("Health"));
    OnDeathAction.AddContextProperty<FVector>(TEXT("Location"));
    OnDeathAction.AddContextProperty<AMyActor*>(TEXT("Owner"));
    OnDeathAction.AddContextProperty<APawn*>(TEXT("Instigator"));

    InteractionRequirement.AddContextProperty<float>(TEXT("Health"));
    InteractionRequirement.AddContextProperty<FVector>(TEXT("Location"));
    InteractionRequirement.AddContextProperty<AMyActor*>(TEXT("Owner"));
    InteractionRequirement.AddContextProperty<APawn*>(TEXT("Instigator"));
}

void AMyActor::OnSpawnActor()
{
    // 1. Pass runtime data to the context
    OnSpawnAction.SetContextProperty<float>(TEXT("Health"), Health);
    OnSpawnAction.SetContextProperty<FVector>(TEXT("Location"), GetActorLocation());
    OnSpawnAction.SetContextProperty<AMyActor*>(TEXT("Owner"), this);
    OnSpawnAction.SetContextProperty<APawn*>(TEXT("Instigator"), GetInstigator());

    // 2. Run the action
    FScriptableAction::RunAction(this, OnSpawnAction);
}

void AMyActor::OnKillActor()
{
    // 1. Pass runtime data to the context
    OnDeathAction.SetContextProperty<float>(TEXT("Health"), Health);
    OnDeathAction.SetContextProperty<FVector>(TEXT("Location"), GetActorLocation());
    OnDeathAction.SetContextProperty<AMyActor*>(TEXT("Owner"), this);
    OnDeathAction.SetContextProperty<APawn*>(TEXT("Instigator"), GetInstigator());

    // 2. Run the action
    FScriptableAction::RunAction(this, OnDeathAction);
}

bool AMyActor::CanInteract()
{
    // 1. Pass runtime data
    InteractionRequirement.SetContextProperty<float>(TEXT("Health"), Health);
    InteractionRequirement.SetContextProperty<FVector>(TEXT("Location"), GetActorLocation());
    InteractionRequirement.SetContextProperty<AMyActor*>(TEXT("Owner"), this);
    InteractionRequirement.SetContextProperty<APawn*>(TEXT("Instigator"), GetInstigator());

    // 2. Evaluate
    return FScriptableRequirement::EvaluateRequirement(this, InteractionRequirement);
}
```

<p align="center">
    <img src="https://kirzo.dev/content/images/plugins/ScriptableFramework/ScriptableFrameworkDemo.gif" width="768">
</p>

## 🛠 Extending the Framework

### Creating Custom Tasks

To extend the system with new behavior, create new Tasks by inheriting from `UScriptableTask`. The `Context` is used internally to resolve bindings before `BeginTask()` is called, so your task simply uses its own properties.

```cpp
// .h
/** Grants a Gameplay Ability. */
UCLASS(DisplayName = "Give Gameplay Ability", meta = (TaskCategory = "Gameplay"))
class UScriptableTask_GiveAbility : public UScriptableTask
{
    GENERATED_BODY()

protected:
    /** The Gameplay Ability Class to grant */
    UPROPERTY(EditAnywhere, Category = "Ability")
    TSubclassOf<class UGameplayAbility> AbilityClass;

    /** Target Ability System Component */
    UPROPERTY(EditAnywhere, Category = "Ability")
    UAbilitySystemComponent* TargetASC = nullptr;

    /** The level of the ability to grant */
    UPROPERTY(EditAnywhere, Category = "Ability", meta = (ClampMin = "1"))
    int32 AbilityLevel = 1;

    /**
     * If true, the ability will be removed from the ASC when this task resets.
     * If false, the ability remains granted permanently (until manually removed).
     */
    UPROPERTY(EditAnywhere, Category = "Ability")
    bool bRemoveOnReset = true;

    /**
     * If true, tries to activate the ability immediately after granting it.
     * Note: The ability must be instanced or instanced per actor for this to work reliably via task.
     */
    UPROPERTY(EditAnywhere, Category = "Ability")
    bool bTryActivateImmediately = false;

    /** Handle to the granted ability, used to remove it later */
    FGameplayAbilitySpecHandle GrantedHandle;

private:
    /** Helper to retrieve ASC */
    UAbilitySystemComponent* GetAbilitySystemComponent() const;

protected:
    virtual void BeginTask() override;
    virtual void ResetTask() override;
};

//.cpp
UAbilitySystemComponent* UScriptableTask_GiveAbility::GetAbilitySystemComponent() const
{
    // Check if TargetASC is already valid (bound via editor)
    return TargetASC ? TargetASC : UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner<AActor>());
}

void UScriptableTask_GiveAbility::BeginTask()
{
    if (!IsValid(AbilityClass))
    {
        // No ability specified, fail the task
        Finish();
        return;
    }

    UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
    if (!ASC)
    {
        Finish();
        return;
    }

    // Create the Spec for the ability
    FGameplayAbilitySpec Spec(AbilityClass, AbilityLevel, INDEX_NONE, GetOwner());

    // Grant the ability and store the handle
    GrantedHandle = ASC->GiveAbility(Spec);

    if (bTryActivateImmediately && GrantedHandle.IsValid())
    {
        ASC->TryActivateAbility(GrantedHandle);
    }

    Finish(); // Call Finish() to notify owner that we are done.
}

void UScriptableTask_GiveAbility::ResetTask()
{
    if (bRemoveOnReset && GrantedHandle.IsValid())
    {
        if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
        {
            // Remove the specific ability instance/spec we added
            ASC->ClearAbility(GrantedHandle);
        }
    }

    // Reset handle
    GrantedHandle = FGameplayAbilitySpecHandle();
}
```

### Creating Custom Conditions

To add new validation logic, create Conditions by inheriting from `UScriptableCondition`.

Conditions are pure logic checks that:
- Read data from their bound properties
- Evaluate a state
- Return a boolean result

They are designed to be simple and reusable.

```cpp
//.h
/** Checks if has a specific Gameplay Ability granted. */
UCLASS(DisplayName = "Has Gameplay Ability", meta = (ConditionCategory = "Gameplay"))
class UScriptableCondition_HasAbility : public UScriptableCondition
{
    GENERATED_BODY()

protected:
    /** The Gameplay Ability Class to check for */
    UPROPERTY(EditAnywhere, Category = "Ability")
    TSubclassOf<UGameplayAbility> AbilityClass;

    /** Target Ability System Component */
    UPROPERTY(EditAnywhere, Category = "Ability")
    UAbilitySystemComponent* TargetASC = nullptr;

    /** 
     * If true, checks if the ability is currently ACTIVE.
     * If false, just checks if the actor HAS the ability granted (even if on cooldown or idle).
     */
    UPROPERTY(EditAnywhere, Category = "Ability")
    bool bMustBeActive = false;

protected:
    virtual bool Evaluate_Implementation() const override;
};

//.cpp
bool UScriptableCondition_HasAbility::Evaluate_Implementation() const
{
    if (!IsValid(AbilityClass))
    {
        return false;
    }

    // Get ASC directly
    UAbilitySystemComponent* ASC = TargetASC ? TargetASC : UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner<AActor>());
    if (!ASC)
    {
        return false;
    }

    // Find the Ability Spec
    const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(AbilityClass);

    if (!Spec)
    {
        // Actor does not have this ability granted
        return false;
    }

    // Check if it's currently active
    return !bMustBeActive || Spec->IsActive();
}
```

## Organization & Filtering

To keep your editor clean, you can organize your custom nodes into submenus in the picker using metadata.

```cpp
// This will appear under "Gameplay" in the dropdown
UCLASS(DisplayName = "Give Gameplay Ability", meta = (TaskCategory = "Gameplay"))
class UScriptableTask_GiveAbility : public UScriptableTask { ... }

// This will appear under "Gameplay > Health"
UCLASS(DisplayName = "Is Health Low", meta = (ConditionCategory = "Gameplay|Health"))
class UMyCondition_IsHealthLow : public UScriptableCondition { ... };
```

You can also restrict which categories are allowed for a specific property. This is useful if you want a Requirement field to only accept specific types of conditions (e.g., only **"Gameplay"** conditions).

```cpp
// Only Tasks with Category="Gameplay" (or subcategories) will be selectable here
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = MyActor, meta = (TaskCategories = "Gameplay"))
FScriptableAction GameplayAction;

// Only Conditions with Category="Gameplay" (or subcategories) will be selectable here
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = MyActor, meta = (ConditionCategories = "Gameplay"))
FScriptableRequirement GameplayRequirement;
```

<p align="center">
	<img src="https://kirzo.dev/content/images/plugins/ScriptableFramework/category_filter.jpg" width="512">
</p>
