// Copyright 2026 kirzo

#include "ScriptableFrameworkEd/Validation/ScriptableGraphValidator.h"
#include "ScriptableNodes/ScriptableGraph.h"
#include "ScriptableNodes/ScriptableGraphConnection.h"
#include "ScriptableNodes/ScriptableNode.h"
#include "ScriptableNodes/ScriptableNode_ReceiveEvent.h"
#include "ScriptableNodes/ScriptableNode_GoTo.h"
#include "ScriptableNodes/ScriptableNode_Exit.h"
#include "ScriptableNodes/ScriptableNode_Finish.h"
#include "ScriptableNodes/ScriptableNode_Task.h"
#include "ScriptableTasks/ScriptableTask.h"
#include "ScriptableTasks/ScriptableTask_RunGraph.h"
#include "ScriptableTasks/ScriptableTask_SetLocal.h"
#include "Core/KzNamedVariant.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "ScriptableGraphValidator"

namespace
{
	const FName GValidatorId("ScriptableGraph");

	/** Readable label for messages: the inner task's class name for Task wrappers, else the node's class display name. */
	FString GetNodeLabel(const UScriptableNode* Node)
	{
		if (!Node) return TEXT("<null>");
		if (const UScriptableNode_Task* TaskNode = Cast<UScriptableNode_Task>(Node))
		{
			if (TaskNode->Task) return TaskNode->Task->GetClass()->GetDisplayNameText().ToString();
		}
		return Node->GetClass()->GetDisplayNameText().ToString();
	}

	/**
	 * Recursive DFS that flags cycles. Nodes on the current path live in InStack; fully explored ones
	 * in Visited (shared across seeds so each node is walked once). A back-edge to an InStack node closes
	 * a cycle, reported once per closing node with the path slice that forms the loop.
	 */
	void DetectCycleDFS(const FGuid& NodeId, const TMultiMap<FGuid, FGuid>& Adjacency, const TMap<FGuid, const UScriptableNode*>& NodesByGuid,
		TSet<FGuid>& Visited, TSet<FGuid>& InStack, TArray<FGuid>& PathStack, TSet<FGuid>& ReportedCycles, TArray<FKzValidationIssue>& OutIssues)
	{
		Visited.Add(NodeId);
		InStack.Add(NodeId);
		PathStack.Add(NodeId);

		TArray<FGuid> Targets;
		Adjacency.MultiFind(NodeId, Targets);
		for (const FGuid& Target : Targets)
		{
			if (InStack.Contains(Target))
			{
				if (ReportedCycles.Contains(Target)) continue;
				ReportedCycles.Add(Target);

				// Build "Closing -> ... -> Current -> Closing" from the path slice starting at the closing node.
				FString PathStr;
				const int32 StartIdx = PathStack.Find(Target);
				for (int32 i = StartIdx; i < PathStack.Num(); ++i)
				{
					PathStr += GetNodeLabel(NodesByGuid.FindRef(PathStack[i]));
					PathStr += TEXT(" \u2192 ");
				}
				PathStr += GetNodeLabel(NodesByGuid.FindRef(Target));

				OutIssues.Add(FKzValidationIssue::WithContextId(EKzValidationSeverity::Error,
					FText::Format(LOCTEXT("Cycle", "Cycle detected at node '{0}' (path: {1})."),
						FText::FromString(GetNodeLabel(NodesByGuid.FindRef(Target))), FText::FromString(PathStr)),
					GValidatorId, Target));
			}
			else if (!Visited.Contains(Target))
			{
				DetectCycleDFS(Target, Adjacency, NodesByGuid, Visited, InStack, PathStack, ReportedCycles, OutIssues);
			}
		}

		InStack.Remove(NodeId);
		PathStack.Pop();
	}

	/**
	 * DFS over assets following RunGraph references. Returns true if Current eventually reaches Origin,
	 * collecting the asset chain (excluding Origin) in OutPath. Null/unloaded GraphAssets are skipped.
	 */
	bool RunGraphReachesOrigin(const UScriptableGraph* Current, const UScriptableGraph* Origin, TSet<const UScriptableGraph*>& Visited, TArray<const UScriptableGraph*>& OutPath)
	{
		if (!Current) return false;
		if (Current == Origin) return true;
		if (Visited.Contains(Current)) return false;

		Visited.Add(Current);
		OutPath.Add(Current);

		for (const TObjectPtr<UScriptableNode>& Node : Current->Nodes)
		{
			const UScriptableNode_Task* TaskNode = Cast<UScriptableNode_Task>(Node);
			if (!TaskNode) continue;
			const UScriptableTask_RunGraph* RunGraph = Cast<UScriptableTask_RunGraph>(TaskNode->Task);
			if (!RunGraph || !RunGraph->GraphAsset) continue;

			if (RunGraphReachesOrigin(RunGraph->GraphAsset, Origin, Visited, OutPath))
			{
				return true;
			}
		}

		OutPath.Pop();
		return false;
	}
}

FName UScriptableGraphValidator::GetValidatorId_Implementation() const
{
	return GValidatorId;
}

bool UScriptableGraphValidator::CanValidate_Implementation(const UObject* Asset) const
{
	return Asset && Asset->IsA<UScriptableGraph>();
}

void UScriptableGraphValidator::Validate_Implementation(const UObject* Asset, TArray<FKzValidationIssue>& OutIssues) const
{
	const UScriptableGraph* Graph = Cast<UScriptableGraph>(Asset);
	if (!Graph) return;

	// --- Node-by-GUID lookup. Duplicate IDs corrupt connection routing (error); first occurrence wins. ---
	TMap<FGuid, const UScriptableNode*> NodesByGuid;
	NodesByGuid.Reserve(Graph->Nodes.Num());
	for (const TObjectPtr<UScriptableNode>& Node : Graph->Nodes)
	{
		if (!Node) continue;
		const FGuid Id = Node->GetBindingID();
		if (!Id.IsValid()) continue;

		if (const UScriptableNode** Existing = NodesByGuid.Find(Id))
		{
			OutIssues.Add(FKzValidationIssue::WithContextId(EKzValidationSeverity::Error,
				FText::Format(LOCTEXT("DuplicateId", "Node '{0}' shares its ID with '{1}' (duplicate node ID corrupts connection routing)."),
					FText::FromString(GetNodeLabel(Node)), FText::FromString(GetNodeLabel(*Existing))),
				GValidatorId, Id));
			continue;
		}
		NodesByGuid.Add(Id, Node);
	}

	// --- Entry points: EntryNodeID + every ReceiveEvent. Also flag empty + duplicate EventNames. ---
	TArray<FGuid> Seeds;
	if (Graph->EntryNodeID.IsValid() && NodesByGuid.Contains(Graph->EntryNodeID))
	{
		Seeds.AddUnique(Graph->EntryNodeID);
	}
	TMap<FName, TArray<FGuid>> EventNameToNodeIds;
	for (const TObjectPtr<UScriptableNode>& Node : Graph->Nodes)
	{
		const UScriptableNode_ReceiveEvent* Receive = Cast<UScriptableNode_ReceiveEvent>(Node);
		if (!Receive) continue;

		const FGuid Id = Receive->GetBindingID();
		if (Id.IsValid()) Seeds.AddUnique(Id);

		if (Receive->EventName.IsNone())
		{
			OutIssues.Add(FKzValidationIssue::WithContextId(EKzValidationSeverity::Warning,
				LOCTEXT("EmptyEventName", "ReceiveEvent node has empty EventName and will never fire."),
				GValidatorId, Id));
			continue;
		}

		EventNameToNodeIds.FindOrAdd(Receive->EventName).Add(Id);
	}

	/**
	 * Duplicate event names are a legitimate fan-out pattern (FireEvent broadcasts to every match in
	 * parallel by design), so surface as Info — visible in the panel so accidental dups are noticed,
	 * but not enough to flip bLastCompileFailed or paint WARNING! on the nodes.
	 */
	for (const TPair<FName, TArray<FGuid>>& Pair : EventNameToNodeIds)
	{
		if (Pair.Value.Num() <= 1) continue;
		for (const FGuid& Id : Pair.Value)
		{
			OutIssues.Add(FKzValidationIssue::WithContextId(EKzValidationSeverity::Info,
				FText::Format(LOCTEXT("DuplicateEventName", "Multiple ReceiveEvent nodes declare '{0}'. FireEvent will trigger them in parallel."), FText::FromName(Pair.Key)),
				GValidatorId, Id));
		}
	}

	// The Exit node is an internal entry point (the runner fires its outputs directly), so seed it too.
	// This keeps the Exit and its cleanup sub-flow out of the orphan/reachability check below.
	for (const TObjectPtr<UScriptableNode>& Node : Graph->Nodes)
	{
		if (Cast<UScriptableNode_Exit>(Node))
		{
			const FGuid Id = Node->GetBindingID();
			if (Id.IsValid()) Seeds.AddUnique(Id);
		}
	}

	// --- Connection validation + forward adjacency (From -> To) used by reachability and cycle checks. ---
	TMultiMap<FGuid, FGuid> Adjacency;
	for (const FScriptableGraphConnection& Conn : Graph->Connections)
	{
		const UScriptableNode* FromNode = NodesByGuid.FindRef(Conn.From.NodeID);
		const UScriptableNode* ToNode = NodesByGuid.FindRef(Conn.To.NodeID);

		// Missing node endpoint: one issue per connection (From wins when both are missing).
		if (!FromNode || !ToNode)
		{
			const FGuid MissingId = !FromNode ? Conn.From.NodeID : Conn.To.NodeID;
			OutIssues.Add(FKzValidationIssue::WithContextId(EKzValidationSeverity::Error,
				FText::Format(LOCTEXT("MissingNode", "Connection references missing node {0}."),
					FText::FromString(MissingId.ToString(EGuidFormats::DigitsWithHyphens))),
				GValidatorId, MissingId));
			continue;
		}

		// Both nodes exist: they form a forward edge regardless of pin correctness.
		Adjacency.Add(Conn.From.NodeID, Conn.To.NodeID);

		const TArray<FName> FromOutputs = FromNode->GetOutputPins();
		const TArray<FName> FromInputs = FromNode->GetInputPins();
		const TArray<FName> ToInputs = ToNode->GetInputPins();
		const TArray<FName> ToOutputs = ToNode->GetOutputPins();

		const bool bFromIsOutput = FromOutputs.Contains(Conn.From.PinName);
		const bool bToIsInput = ToInputs.Contains(Conn.To.PinName);

		// Inverted wire: From lands on an input pin, or To lands on an output pin. One issue, keyed to From.
		if ((!bFromIsOutput && FromInputs.Contains(Conn.From.PinName)) || (!bToIsInput && ToOutputs.Contains(Conn.To.PinName)))
		{
			OutIssues.Add(FKzValidationIssue::WithContextId(EKzValidationSeverity::Error,
				FText::Format(LOCTEXT("Inverted", "Connection has inverted direction on node '{0}'."), FText::FromString(GetNodeLabel(FromNode))),
				GValidatorId, Conn.From.NodeID));
			continue;
		}

		// Otherwise, flag any pin that simply does not exist on its node.
		if (!bFromIsOutput)
		{
			OutIssues.Add(FKzValidationIssue::WithContextId(EKzValidationSeverity::Error,
				FText::Format(LOCTEXT("MissingPinFrom", "Connection references missing pin '{0}' on node '{1}'."),
					FText::FromName(Conn.From.PinName), FText::FromString(GetNodeLabel(FromNode))),
				GValidatorId, Conn.From.NodeID));
		}
		if (!bToIsInput)
		{
			OutIssues.Add(FKzValidationIssue::WithContextId(EKzValidationSeverity::Error,
				FText::Format(LOCTEXT("MissingPinTo", "Connection references missing pin '{0}' on node '{1}'."),
					FText::FromName(Conn.To.PinName), FText::FromString(GetNodeLabel(ToNode))),
				GValidatorId, Conn.To.NodeID));
		}
	}

	// --- Reachability: BFS from the seed set. Any node never reached is an orphan (warning). ---
	TSet<FGuid> Reachable(Seeds);
	TArray<FGuid> Queue = Seeds;
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		TArray<FGuid> Targets;
		Adjacency.MultiFind(Queue[Head], Targets);
		for (const FGuid& Target : Targets)
		{
			bool bAlready = false;
			Reachable.Add(Target, &bAlready);
			if (!bAlready) Queue.Add(Target);
		}
	}

	for (const TObjectPtr<UScriptableNode>& Node : Graph->Nodes)
	{
		if (!Node) continue;
		const FGuid Id = Node->GetBindingID();
		if (!Id.IsValid() || Reachable.Contains(Id)) continue;

		OutIssues.Add(FKzValidationIssue::WithContextId(EKzValidationSeverity::Warning,
			FText::Format(LOCTEXT("Orphan", "Node '{0}' is not reachable from Entry or any ReceiveEvent."), FText::FromString(GetNodeLabel(Node))),
			GValidatorId, Id));
	}

	// --- Cycle detection: DFS from every entry point. ---
	{
		TSet<FGuid> Visited;
		TSet<FGuid> InStack;
		TArray<FGuid> PathStack;
		TSet<FGuid> ReportedCycles;
		const int32 SeedCount = Seeds.Num();
		for (int32 i = 0; i < SeedCount; ++i)
		{
			if (!Visited.Contains(Seeds[i]))
			{
				DetectCycleDFS(Seeds[i], Adjacency, NodesByGuid, Visited, InStack, PathStack, ReportedCycles, OutIssues);
			}
		}
	}

	// --- RunGraph recursion: direct self-reference (error) and indirect cross-asset cycle (warning). ---
	for (const TObjectPtr<UScriptableNode>& Node : Graph->Nodes)
	{
		const UScriptableNode_Task* TaskNode = Cast<UScriptableNode_Task>(Node);
		if (!TaskNode) continue;
		const UScriptableTask_RunGraph* RunGraph = Cast<UScriptableTask_RunGraph>(TaskNode->Task);
		if (!RunGraph) continue;

		const FGuid Id = Node->GetBindingID();

		if (RunGraph->GraphAsset == Graph)
		{
			OutIssues.Add(FKzValidationIssue::WithContextId(EKzValidationSeverity::Error,
				FText::Format(LOCTEXT("RunGraphSelf", "RunGraph node '{0}' references the asset itself (infinite recursion)."), FText::FromString(GetNodeLabel(Node))),
				GValidatorId, Id));
			continue;
		}

		if (!RunGraph->GraphAsset) continue;

		// Does following this reference (between assets) lead back to us?
		TSet<const UScriptableGraph*> Visited;
		TArray<const UScriptableGraph*> PathAssets;
		if (RunGraphReachesOrigin(RunGraph->GraphAsset, Graph, Visited, PathAssets))
		{
			FString PathStr = Graph->GetName();
			for (const UScriptableGraph* PathAsset : PathAssets)
			{
				PathStr += TEXT(" \u2192 ");
				PathStr += PathAsset ? PathAsset->GetName() : TEXT("?");
			}
			PathStr += TEXT(" \u2192 ");
			PathStr += Graph->GetName();

			OutIssues.Add(FKzValidationIssue::WithContextId(EKzValidationSeverity::Warning,
				FText::Format(LOCTEXT("RunGraphIndirect", "RunGraph node '{0}' may form indirect cycle: {1}."), FText::FromString(GetNodeLabel(Node)), FText::FromString(PathStr)),
				GValidatorId, Id));
		}
	}

	// --- Exit nodes: at most one per graph. Flag every Exit past the first. ---
	{
		bool bSeenExit = false;
		for (const TObjectPtr<UScriptableNode>& Node : Graph->Nodes)
		{
			if (!Cast<UScriptableNode_Exit>(Node)) continue;

			if (bSeenExit)
			{
				OutIssues.Add(FKzValidationIssue::WithContextId(EKzValidationSeverity::Error,
					LOCTEXT("MultipleExit", "Graph has multiple Exit nodes; only one is allowed."),
					GValidatorId, Node->GetBindingID()));
			}
			bSeenExit = true;
		}
	}

	// --- Go To targets: every Go To must point at an event some ReceiveEvent declares. ---
	{
		// Collect the named events declared in this graph.
		TSet<FName> DeclaredEvents;
		for (const TObjectPtr<UScriptableNode>& Node : Graph->Nodes)
		{
			const UScriptableNode_ReceiveEvent* Receive = Cast<UScriptableNode_ReceiveEvent>(Node);
			if (Receive && !Receive->EventName.IsNone())
			{
				DeclaredEvents.Add(Receive->EventName);
			}
		}

		for (const TObjectPtr<UScriptableNode>& Node : Graph->Nodes)
		{
			const UScriptableNode_GoTo* GoTo = Cast<UScriptableNode_GoTo>(Node);
			if (!GoTo) continue;

			const FGuid Id = Node->GetBindingID();

			if (GoTo->TargetEvent.IsNone())
			{
				OutIssues.Add(FKzValidationIssue::WithContextId(EKzValidationSeverity::Warning,
					FText::Format(LOCTEXT("GoToUnset", "Go To node '{0}' has no target event set."), FText::FromString(GetNodeLabel(Node))),
					GValidatorId, Id));
			}
			else if (!DeclaredEvents.Contains(GoTo->TargetEvent))
			{
				FKzValidationIssue Issue = FKzValidationIssue::WithContextId(EKzValidationSeverity::Error,
					FText::Format(LOCTEXT("GoToMissing", "Go To node '{0}' targets event '{1}', which no ReceiveEvent node declares."),
						FText::FromString(GetNodeLabel(Node)), FText::FromName(GoTo->TargetEvent)),
					GValidatorId, Id);

				TWeakObjectPtr<UScriptableNode_GoTo> WeakGoTo(const_cast<UScriptableNode_GoTo*>(GoTo));
				Issue.QuickFixLabel = LOCTEXT("ClearTarget", "Clear");
				Issue.QuickFix = [WeakGoTo]()
				{
					UScriptableNode_GoTo* G = WeakGoTo.Get();
					if (!G) return;
					const FScopedTransaction Tx(LOCTEXT("QF_ClearGoToTarget", "Clear orphan GoTo target"));
					G->Modify();
					G->TargetEvent = NAME_None;
					G->PostEditChange();
				};
				OutIssues.Add(MoveTemp(Issue));
			}
		}
	}

	// --- Cross-node Output bindings: warn when the bound source node is not a static exec-ancestor. ---
	// These bindings resolve at activation time, so the source must already have run. Static ancestry
	// can't see event-driven paths, so this is a warning (not an error): the author may know the order
	// holds, and the runtime no-ops on an unset value.
	{
		// Map every node's binding proxy (e.g. its task) back to the node that hosts it.
		TMap<FGuid, FGuid> ProxyToNode;
		for (const TObjectPtr<UScriptableNode>& Node : Graph->Nodes)
		{
			if (!Node) continue;
			if (const UScriptableObject* Proxy = Node->GetBindingProxy())
			{
				if (Proxy->GetBindingID().IsValid()) ProxyToNode.Add(Proxy->GetBindingID(), Node->GetBindingID());
			}
		}

		// Reverse the forward edges (To -> From) so we can walk a reader's ancestors backward.
		TMultiMap<FGuid, FGuid> ReverseAdjacency;
		for (const TPair<FGuid, FGuid>& Edge : Adjacency)
		{
			ReverseAdjacency.Add(Edge.Value, Edge.Key);
		}

		for (const TObjectPtr<UScriptableNode>& Node : Graph->Nodes)
		{
			if (!Node) continue;
			const UScriptableObject* Proxy = Node->GetBindingProxy();
			if (!Proxy) continue;

			const FGuid ReaderId = Node->GetBindingID();

			// Collect the source node GUIDs this reader binds to, keeping only cross-node sources.
			TSet<FGuid> RequiredSources;
			for (const FScriptablePropertyBinding& Binding : Proxy->GetPropertyBindings().Bindings)
			{
				if (!Binding.SourceID.IsValid()) continue; // Context binding.
				const FGuid* SourceNodeId = ProxyToNode.Find(Binding.SourceID);
				if (!SourceNodeId || *SourceNodeId == ReaderId) continue; // Unknown source or self.
				RequiredSources.Add(*SourceNodeId);
			}
			if (RequiredSources.IsEmpty()) continue;

			// Backward BFS to gather every exec-ancestor of the reader.
			TSet<FGuid> Ancestors;
			TArray<FGuid> AncestorQueue = { ReaderId };
			for (int32 Head = 0; Head < AncestorQueue.Num(); ++Head)
			{
				TArray<FGuid> Parents;
				ReverseAdjacency.MultiFind(AncestorQueue[Head], Parents);
				for (const FGuid& Parent : Parents)
				{
					bool bAlready = false;
					Ancestors.Add(Parent, &bAlready);
					if (!bAlready) AncestorQueue.Add(Parent);
				}
			}

			for (const FGuid& SourceId : RequiredSources)
			{
				if (!Ancestors.Contains(SourceId))
				{
					OutIssues.Add(FKzValidationIssue::WithContextId(EKzValidationSeverity::Info,
						FText::Format(LOCTEXT("CrossNodeOrder", "Node '{0}' reads an Output of '{1}', which is not upstream of it; the value may be unset when '{0}' runs."),
							FText::FromString(GetNodeLabel(Node)), FText::FromString(GetNodeLabel(NodesByGuid.FindRef(SourceId)))),
						GValidatorId, ReaderId));
				}
			}
		}
	}

	// --- Asset Outputs: no None/empty entries, no duplicates. The UPROPERTY has NoElementDuplicate
	// but stale assets / external edits can still slip past it; warn explicitly here so the user sees
	// the issue surfaced in the validation tab. ---
	{
		TWeakObjectPtr<UScriptableGraph> WeakGraph(const_cast<UScriptableGraph*>(Graph));
		auto MakeRemoveOutputFix = [WeakGraph](int32 IndexToRemove) -> TFunction<void()>
		{
			return [WeakGraph, IndexToRemove]()
			{
				UScriptableGraph* G = WeakGraph.Get();
				if (!G || !G->Outputs.IsValidIndex(IndexToRemove)) return;
				const FScopedTransaction Tx(LOCTEXT("QF_RemoveOutput", "Remove Graph.Outputs entry"));
				G->Modify();
				G->Outputs.RemoveAt(IndexToRemove);
				/** Targeted event so the editor knows to reconstruct Exit pins and mark dirty. */
				if (FProperty* Prop = FindFProperty<FProperty>(UScriptableGraph::StaticClass(), GET_MEMBER_NAME_CHECKED(UScriptableGraph, Outputs)))
				{
					FPropertyChangedEvent Event(Prop, EPropertyChangeType::ArrayRemove);
					G->PostEditChangeProperty(Event);
				}
				else
				{
					G->PostEditChange();
				}
			};
		};

		TSet<FName> SeenOutputs;
		for (int32 Index = 0; Index < Graph->Outputs.Num(); ++Index)
		{
			const FName& Output = Graph->Outputs[Index];

			if (Output.IsNone())
			{
				FKzValidationIssue Issue(EKzValidationSeverity::Error,
					FText::Format(LOCTEXT("OutputNone", "Graph.Outputs entry #{0} is None or empty."), Index),
					GValidatorId);
				Issue.QuickFixLabel = LOCTEXT("RemoveEntry", "Remove");
				Issue.QuickFix = MakeRemoveOutputFix(Index);
				OutIssues.Add(MoveTemp(Issue));
				continue;
			}

			if (SeenOutputs.Contains(Output))
			{
				FKzValidationIssue Issue(EKzValidationSeverity::Error,
					FText::Format(LOCTEXT("OutputDuplicate", "Graph.Outputs declares '{0}' more than once."), FText::FromName(Output)),
					GValidatorId);
				Issue.QuickFixLabel = LOCTEXT("DedupeEntry", "Dedupe");
				Issue.QuickFix = MakeRemoveOutputFix(Index);
				OutIssues.Add(MoveTemp(Issue));
				continue;
			}

			SeenOutputs.Add(Output);
		}
	}

	// --- Finish must not be wired downstream of Exit. Exit's outputs already are the graph's
	// completion paths; feeding one back into a Finish would loop into HandleNodeRequestFinishGraph,
	// which short-circuits via bExitTriggered — silently dead wire that confuses the author. ---
	{
		TSet<FGuid> FinishNodeIds;
		for (const TObjectPtr<UScriptableNode>& Node : Graph->Nodes)
		{
			if (Cast<UScriptableNode_Finish>(Node))
			{
				FinishNodeIds.Add(Node->GetBindingID());
			}
		}

		if (!FinishNodeIds.IsEmpty())
		{
			for (const FScriptableGraphConnection& Conn : Graph->Connections)
			{
				if (!FinishNodeIds.Contains(Conn.To.NodeID)) continue;

				const UScriptableNode* FromNode = NodesByGuid.FindRef(Conn.From.NodeID);
				if (!Cast<UScriptableNode_Exit>(FromNode)) continue;

				OutIssues.Add(FKzValidationIssue::WithContextId(EKzValidationSeverity::Error,
					FText::Format(LOCTEXT("FinishFromExit", "Finish node is wired from Exit's '{0}' output. Exit already represents completion; this connection has no effect."),
						FText::FromName(Conn.From.PinName)),
					GValidatorId, Conn.To.NodeID));
			}
		}
	}

	// --- Finish.OutputName must reference a name still declared in Graph.Outputs. The picker only
	// shows current entries, but renaming/removing an Output leaves previously-set names orphaned
	// in the asset; the runtime silently falls back to "Finished" instead of the author's pick. ---
	{
		TSet<FName> DeclaredOutputs;
		for (const FName& Output : Graph->Outputs)
		{
			if (!Output.IsNone()) DeclaredOutputs.Add(Output);
		}

		for (const TObjectPtr<UScriptableNode>& Node : Graph->Nodes)
		{
			const UScriptableNode_Finish* Finish = Cast<UScriptableNode_Finish>(Node);
			if (!Finish || Finish->OutputName.IsNone()) continue;
			if (DeclaredOutputs.Contains(Finish->OutputName)) continue;

			FKzValidationIssue Issue = FKzValidationIssue::WithContextId(EKzValidationSeverity::Error,
				FText::Format(LOCTEXT("FinishOrphanOutput", "Finish node targets '{0}', which is no longer declared in Graph.Outputs."),
					FText::FromName(Finish->OutputName)),
				GValidatorId, Node->GetBindingID());

			TWeakObjectPtr<UScriptableNode_Finish> WeakFinish(const_cast<UScriptableNode_Finish*>(Finish));
			Issue.QuickFixLabel = LOCTEXT("ClearOrphan", "Clear");
			Issue.QuickFix = [WeakFinish]()
			{
				UScriptableNode_Finish* F = WeakFinish.Get();
				if (!F) return;
				const FScopedTransaction Tx(LOCTEXT("QF_ClearFinishOrphan", "Clear orphan Finish output"));
				F->Modify();
				F->OutputName = NAME_None;
				F->PostEditChange();
			};
			OutIssues.Add(MoveTemp(Issue));
		}
	}

	// --- Context: no entries with empty Name or invalid Type. Both leave the runtime bag without a usable
	// slot for that param and silently break bindings that target it. ---
	{
		TWeakObjectPtr<UScriptableGraph> WeakGraph(const_cast<UScriptableGraph*>(Graph));
		auto MakeRemoveContextFix = [WeakGraph](int32 IndexToRemove) -> TFunction<void()>
		{
			return [WeakGraph, IndexToRemove]()
			{
				UScriptableGraph* G = WeakGraph.Get();
				if (!G || !G->Context.IsValidIndex(IndexToRemove)) return;
				const FScopedTransaction Tx(LOCTEXT("QF_RemoveContextEntry", "Remove Context entry"));
				G->Modify();
				G->Context.RemoveAt(IndexToRemove);
				if (FProperty* Prop = FindFProperty<FProperty>(UScriptableGraph::StaticClass(), GET_MEMBER_NAME_CHECKED(UScriptableGraph, Context)))
				{
					FPropertyChangedEvent Event(Prop, EPropertyChangeType::ArrayRemove);
					G->PostEditChangeProperty(Event);
				}
				else
				{
					G->PostEditChange();
				}
			};
		};

		for (int32 Index = 0; Index < Graph->Context.Num(); ++Index)
		{
			const FKzParamDef& Entry = Graph->Context[Index];

			if (Entry.Name.IsNone())
			{
				FKzValidationIssue Issue(EKzValidationSeverity::Error,
					FText::Format(LOCTEXT("ContextNoneName", "Context entry #{0} has no Name set."), Index),
					GValidatorId);
				Issue.QuickFixLabel = LOCTEXT("RemoveEntry", "Remove");
				Issue.QuickFix = MakeRemoveContextFix(Index);
				OutIssues.Add(MoveTemp(Issue));
				continue;
			}

			if (!Entry.Type.IsValid())
			{
				FKzValidationIssue Issue(EKzValidationSeverity::Error,
					FText::Format(LOCTEXT("ContextInvalidType", "Context entry '{0}' has no Type set."), FText::FromName(Entry.Name)),
					GValidatorId);
				Issue.QuickFixLabel = LOCTEXT("RemoveEntry", "Remove");
				Issue.QuickFix = MakeRemoveContextFix(Index);
				OutIssues.Add(MoveTemp(Issue));
			}
		}
	}

	// --- Locals: parallel checks to Context. Empty Name or invalid (None) Type leave SetLocal tasks unable
	// to write a real slot at runtime, and the binding picker won't list the entry as a source. ---
	{
		TWeakObjectPtr<UScriptableGraph> WeakGraph(const_cast<UScriptableGraph*>(Graph));
		auto MakeRemoveLocalFix = [WeakGraph](int32 IndexToRemove) -> TFunction<void()>
		{
			return [WeakGraph, IndexToRemove]()
			{
				UScriptableGraph* G = WeakGraph.Get();
				if (!G || !G->Locals.IsValidIndex(IndexToRemove)) return;
				const FScopedTransaction Tx(LOCTEXT("QF_RemoveLocalEntry", "Remove Locals entry"));
				G->Modify();
				G->Locals.RemoveAt(IndexToRemove);
				if (FProperty* Prop = FindFProperty<FProperty>(UScriptableGraph::StaticClass(), GET_MEMBER_NAME_CHECKED(UScriptableGraph, Locals)))
				{
					FPropertyChangedEvent Event(Prop, EPropertyChangeType::ArrayRemove);
					G->PostEditChangeProperty(Event);
				}
				else
				{
					G->PostEditChange();
				}
			};
		};

		for (int32 Index = 0; Index < Graph->Locals.Num(); ++Index)
		{
			const FKzNamedVariant& Entry = Graph->Locals[Index];

			if (Entry.GetName().IsNone())
			{
				FKzValidationIssue Issue(EKzValidationSeverity::Error,
					FText::Format(LOCTEXT("LocalNoneName", "Locals entry #{0} has no Name set."), Index),
					GValidatorId);
				Issue.QuickFixLabel = LOCTEXT("RemoveEntry", "Remove");
				Issue.QuickFix = MakeRemoveLocalFix(Index);
				OutIssues.Add(MoveTemp(Issue));
				continue;
			}

			if (!Entry.GetValue().IsValid())
			{
				FKzValidationIssue Issue(EKzValidationSeverity::Error,
					FText::Format(LOCTEXT("LocalInvalidType", "Locals entry '{0}' has no Type set."), FText::FromName(Entry.GetName())),
					GValidatorId);
				Issue.QuickFixLabel = LOCTEXT("RemoveEntry", "Remove");
				Issue.QuickFix = MakeRemoveLocalFix(Index);
				OutIssues.Add(MoveTemp(Issue));
			}
		}
	}

	// --- Set Local tasks: VarName must reference an existing entry in Asset->Locals. Empty VarName warns
	// (probably mid-edit); orphan VarName errors with a Clear quick-fix (silently no-ops at runtime otherwise). ---
	{
		TSet<FName> DeclaredLocals;
		for (const FKzNamedVariant& Var : Graph->Locals)
		{
			if (!Var.GetName().IsNone()) DeclaredLocals.Add(Var.GetName());
		}

		for (const TObjectPtr<UScriptableNode>& Node : Graph->Nodes)
		{
			const UScriptableNode_Task* TaskNode = Cast<UScriptableNode_Task>(Node);
			if (!TaskNode) continue;

			const UScriptableTask_SetLocal* SetLocal = Cast<UScriptableTask_SetLocal>(TaskNode->Task);
			if (!SetLocal) continue;

			const FGuid Id = Node->GetBindingID();

			if (SetLocal->VarName.IsNone())
			{
				OutIssues.Add(FKzValidationIssue::WithContextId(EKzValidationSeverity::Warning,
					FText::Format(LOCTEXT("SetLocalUnset", "Set Local node '{0}' has no VarName set and will no-op at runtime."),
						FText::FromString(GetNodeLabel(Node))),
					GValidatorId, Id));
				continue;
			}

			if (!DeclaredLocals.Contains(SetLocal->VarName))
			{
				FKzValidationIssue Issue = FKzValidationIssue::WithContextId(EKzValidationSeverity::Error,
					FText::Format(LOCTEXT("SetLocalOrphan", "Set Local node '{0}' targets '{1}', which is not declared in Locals."),
						FText::FromString(GetNodeLabel(Node)), FText::FromName(SetLocal->VarName)),
					GValidatorId, Id);

				TWeakObjectPtr<UScriptableTask_SetLocal> WeakSetLocal(const_cast<UScriptableTask_SetLocal*>(SetLocal));
				Issue.QuickFixLabel = LOCTEXT("ClearOrphan", "Clear");
				Issue.QuickFix = [WeakSetLocal]()
				{
					UScriptableTask_SetLocal* S = WeakSetLocal.Get();
					if (!S) return;
					const FScopedTransaction Tx(LOCTEXT("QF_ClearSetLocalOrphan", "Clear orphan Set Local VarName"));
					S->Modify();
					S->VarName = NAME_None;
					// PostEditChange runs the task's hook to reset Value (no matching local found).
					S->PostEditChange();
				};
				OutIssues.Add(MoveTemp(Issue));
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE