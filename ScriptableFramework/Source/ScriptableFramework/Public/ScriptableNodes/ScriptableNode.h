// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableObject.h"
#include "ScriptableNode.generated.h"

class UScriptableNode;
struct FScriptableRequirement;

DECLARE_MULTICAST_DELEGATE_TwoParams(FScriptableNodePinFiredNative, UScriptableNode* /*Node*/, FName /*OutputName*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FScriptableNodeInactiveNative, UScriptableNode* /*Node*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FScriptableNodeRequestEventNative, FName /*EventName*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FScriptableNodeRequestFinishGraphNative, FName /*OutputName*/);

/** Per-node runtime trace verbosity. Off = silent. Log = input activations. Verbose = inputs + output fires. */
UENUM()
enum class EScriptableTraceLevel : uint8
{
	Off     UMETA(DisplayName = "Off"),
	Log     UMETA(DisplayName = "Log"),
	Verbose UMETA(DisplayName = "Verbose"),
};

/**
 * Base class for any node inside a UScriptableGraph: defines the pin-state model and activation protocol.
 * Subclasses declare inputs/outputs and implement ProcessInput; the base manages pin sets, fires delegates,
 * and notifies the runner on inactivity.
 */
UCLASS(Abstract, DefaultToInstanced, EditInlineNew, Blueprintable, BlueprintType, HideCategories = (Hidden), CollapseCategories)
class SCRIPTABLEFRAMEWORK_API UScriptableNode : public UScriptableObject
{
	GENERATED_BODY()

public:
	/** Canonical name of the implicit Stop input, declared by subclasses that support cancellation. */
	static const FName StopInputName;

	/** Canonical name of the implicit Stopped output, auto-generated when Stop is declared as an input. */
	static const FName StoppedOutputName;

	/** Nodes are not expected to tick by themselves. */
	virtual bool CanEverTick() const final override { return false; }

	/** Returns the declared input pins of this node. */
	virtual TArray<FName> GetInputPins() const { return {}; }

	/** Returns the outputs the subclass explicitly declares. */
	virtual TArray<FName> GetDeclaredOutputPins() const { return {}; }

	/*** Returns the effective output pins of this node. */
	TArray<FName> GetOutputPins() const;

	/** Returns true if the given input is ready to be processed right now. */
	virtual bool CanProcessInput(FName InputName) const { return true; }

	/** Runner entry point: marks InputName active and, if CanProcessInput, invokes ProcessInput. */
	void ActivateInput(FName InputName);

	/** Returns true if any input or output pin is currently active. */
	bool IsNodeActive() const { return !ActiveInputPins.IsEmpty() || !ActiveOutputPins.IsEmpty(); }

	/** Read-only view of the currently active input pins. */
	const TSet<FName>& GetActiveInputPins() const { return ActiveInputPins; }

	/** Read-only view of the currently active output pins. */
	const TSet<FName>& GetActiveOutputPins() const { return ActiveOutputPins; }

	/** Fired when an output pin is fired and propagated. The runner subscribes to follow wires. */
	FScriptableNodePinFiredNative OnPinFiredNative;

#if WITH_EDITOR
	/** Broadcast at the top of ActivateInput so the editor module can check breakpoints and pause PIE. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnNodeActivatedEditor, UScriptableNode* /*Node*/);
	static FOnNodeActivatedEditor OnNodeActivatedEditor;
#endif

	/** Fired when the node has no active pins left. The runner subscribes to detect node completion. */
	FScriptableNodeInactiveNative OnNodeInactiveNative;

	/** Fired when the node asks the runner to dispatch a named event (e.g. a Go To node). The runner forwards it to FireEvent. */
	FScriptableNodeRequestEventNative OnRequestEventNative;

	/**
	 * Asks the runner to complete the graph through a specific output (Finish node).
	 * Runner stops active nodes and fires Exit's matching output.
	 */
	FScriptableNodeRequestFinishGraphNative OnRequestFinishGraphNative;

	/** Releases in-flight resources without propagating downstream. Called on hard-cancel; override to cancel internal work silently. */
	virtual void Teardown() {}

	/** Returns the inner object that carries this node's bindable data (e.g. the hosted task), or null if the node exposes none. The graph registers proxies so a node's Input can read a sibling node's Output. */
	virtual UScriptableObject* GetBindingProxy() const { return nullptr; }

	/**
	 * Runtime trace verbosity for this node. Off by default. Authors flip it on the asset to follow a
	 * specific node's flow in the output log without recompiling.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Debug", meta = (NoBinding))
	EScriptableTraceLevel TraceLevel = EScriptableTraceLevel::Off;

	/** Max-verbosity of (own TraceLevel, owning Graph's DefaultNodeTraceLevel, scriptable.TraceLevel CVar). */
	EScriptableTraceLevel GetEffectiveTraceLevel() const;

	/** Short label used by trace logs. Base strips the "ScriptableNode_" prefix; subclasses override to add context (e.g. the wrapped task class for Task wrappers). */
	virtual FString GetTraceLabel() const;

	/** Walks the outer chain to find the source UScriptableGraph asset. Works at edit time (asset is the direct outer) and at runtime (outer chain reaches UScriptableGraphInstance whose Asset points back). */
	const class UScriptableGraph* FindOwningAsset() const;

protected:
	/** Reaction to a consumed input (default no-op). Subclasses must MarkInputInactive on the input they consume. */
	virtual void ProcessInput(FName InputName) {}

	/** Marks the given input as inactive. Subclasses call this when they consume an input. */
	void MarkInputInactive(FName InputName);

	/** Marks the given output as active. Subclasses call this when they intend to fire an output later. */
	void MarkOutputActive(FName OutputName);

	/** Marks the given output as inactive without firing. Use for cleanup paths (e.g. Stop invalidating pending outputs). */
	void MarkOutputInactive(FName OutputName);

	/** Broadcasts the pin-fired delegate (runner propagates), then marks the output inactive and notifies if now idle. */
	void FireOutput(FName OutputName);

	/** Helper to deactivate every currently active output without firing. Used in cancellation paths. */
	void DeactivateAllOutputs();

	/** Registers a requirement hosted by this node against the graph scope: Context, Locals and the other nodes' Outputs, as the editor offers them. Unregister it in OnUnregister. */
	void RegisterRequirement(FScriptableRequirement& Requirement);

private:
	/** Checks whether the node just became inactive and broadcasts OnNodeInactiveNative if so. */
	void NotifyIfInactive();

	UPROPERTY(Transient)
	TSet<FName> ActiveInputPins;

	UPROPERTY(Transient)
	TSet<FName> ActiveOutputPins;

	/** Defers OnNodeInactiveNative while > 0. Wraps ProcessInput so the transient zero-pin state mid-transition (input consumed, outputs not yet armed) isn't seen as "inactive" — otherwise latent nodes get evicted before their first output fires and the graph completes early. */
	int32 InactiveNotificationsSuppressed = 0;
};