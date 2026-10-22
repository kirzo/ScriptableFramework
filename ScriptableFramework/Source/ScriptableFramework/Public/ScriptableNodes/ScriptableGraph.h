// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableObjectAsset.h"
#include "ScriptableNodes/ScriptableGraphConnection.h"
#include "ScriptableNodes/ScriptableNode.h"
#include "Core/KzNamedVariant.h"
#include "Core/KzParamDef.h"
#include "StructUtils/PropertyBag.h"
#include "ScriptableGraph.generated.h"

class UScriptableNode;
class UScriptableGraphInstance;
class UEdGraph;
struct FScriptableContext;

/**
 * Asset defining a reusable scriptable graph: a network of UScriptableNode.
 * Stores the nodes, the central connection list, the (always present) Entry node ID, and the context shape.
 */
UCLASS(BlueprintType, Const)
class SCRIPTABLEFRAMEWORK_API UScriptableGraph final : public UScriptableObjectAsset
{
	GENERATED_BODY()

public:
	UScriptableGraph();

	/**
	 * Launches a runtime instance of this graph and returns it. Plain C++ entry point used by the
	 * async "Run Scriptable Graph" node (UAsyncRunScriptableGraph); not exposed to Blueprint directly.
	 */
	static UScriptableGraphInstance* Run(UScriptableGraph* Graph, UObject* Owner, const FScriptableContext& InContext, FName Id = NAME_None);

	/** All nodes living in this graph. Instanced so editor-created nodes are owned by the asset. */
	UPROPERTY(Instanced)
	TArray<TObjectPtr<UScriptableNode>> Nodes;

	/** Flat list of all wires in the graph. */
	UPROPERTY()
	TArray<FScriptableGraphConnection> Connections;

	/** Persistent ID of the Entry node. Set automatically on creation and validated/repaired on load. */
	UPROPERTY()
	FGuid EntryNodeID;

	/** What whoever runs the graph passes in. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TArray<FKzParamDef> Context;

	/** Per-instance mutable state. Graph-scoped variables seeded with each entry's default value at Launch. */
	UPROPERTY(EditAnywhere, Category = "Local Variables")
	TArray<FKzNamedVariant> Locals;

	/**
	 * User-declared completion outputs. Appended after Exit's built-in "Finished"/"Cancelled" pins
	 * and mirrored by SubGraph nodes referencing this asset. Order = pin order.
	 */
	UPROPERTY(EditAnywhere, Category = "Outputs", meta = (NoElementDuplicate))
	TArray<FName> Outputs;

	/** Asset-wide floor for node trace verbosity. Each node's effective level is the max of its own TraceLevel, this default, and the scriptable.TraceLevel CVar. */
	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	EScriptableTraceLevel DefaultNodeTraceLevel = EScriptableTraceLevel::Off;

	/**
	 * BindingIDs of nodes that carry a breakpoint, mapped to their enabled flag (true = pause on hit,
	 * false = the breakpoint still exists but is ignored). UE 5.7's BP-centric FKismetDebugUtilities API
	 * doesn't apply to us so we store directly. Presence in the map = has breakpoint.
	 */
	UPROPERTY()
	TMap<FGuid, bool> Breakpoints;

	//~ UObject interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End of UObject interface

public:
	//~ UScriptableObjectAsset interface
	/** Keeps the transient bag in sync with the declared shape so binding discovery always sees the context (e.g. during a save re-bake), then returns it. */
	virtual FInstancedPropertyBag* GetContext() override
	{
		if (IsContextBagOutOfSync()) { RebuildContextBag(); }
		return &ContextBag;
	}
	//~ End of UScriptableObjectAsset interface

#if WITH_EDITOR
	/** Returns the cached design-time shape of the Locals bag. Used by binding discovery for tasks parented to this asset. */
	const FInstancedPropertyBag* GetLocalsShape() const { return &LocalsBagShape; }
#endif

private:
	/** Backing bag holding the declared context shape. Values are not stored at asset level. */
	UPROPERTY(Transient)
	FInstancedPropertyBag ContextBag;

#if WITH_EDITORONLY_DATA
	/** Design-time mirror of Locals' shape used by binding discovery. Rebuilt on PostLoad and on every Locals edit. */
	UPROPERTY(Transient)
	FInstancedPropertyBag LocalsBagShape;
#endif

	/** Creates the Entry node if missing and registers its BindingID in EntryNodeID. */
	void EnsureEntryNode();

	/** Returns true if ContextBag's shape doesn't match Context's declared params. */
	bool IsContextBagOutOfSync() const;

	/** Rebuilds the transient ContextBag from the persisted Context array. Idempotent. */
	void RebuildContextBag();

#if WITH_EDITOR
	/** Rebuilds LocalsBagShape from the persisted Locals array. Idempotent. */
	void RebuildLocalsBagShape();
#endif

	/** Drops connections whose endpoint node or pin no longer exists; marks the package dirty if any were removed. Called from PostLoad. */
	void PruneOrphanConnections();

#if WITH_EDITOR
	/** Re-bakes auto-bindings on every node/task so context edits are picked up without a save. */
	void RebakeNodeAutoBindings();
#endif

public:
#if WITH_EDITORONLY_DATA
	/** Editor-only visual representation of the graph. Holds node positions, wires, comments, etc. */
	UPROPERTY()
	TObjectPtr<UEdGraph> EdGraph;

	/**
	 * Result of the last compile pass. Set by the editor's Compile action (and on editor open).
	 * Runtime Launch refuses to start the graph while this is true (editor-only gate).
	 */
	UPROPERTY()
	bool bLastCompileFailed = false;

	/** Broadcast by Launch when it refuses to start a graph because bLastCompileFailed is true. Editor module observes this to surface a post-PIE dialog. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnLaunchBlockedByCompile, UScriptableGraph* /*Asset*/);
	static FOnLaunchBlockedByCompile OnLaunchBlockedByCompile;

	/** Broadcast from PostLoad in editor builds so the editor module can defer a compile refresh until validators are reachable. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPostLoaded, UScriptableGraph* /*Asset*/);
	static FOnPostLoaded OnPostLoaded;
#endif
};