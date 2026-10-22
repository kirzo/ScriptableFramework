// Copyright 2026 kirzo

#include "ScriptableNodes/ScriptableGraph.h"
#include "ScriptableNodes/ScriptableGraphInstance.h"
#include "ScriptableNodes/ScriptableGraphSubsystem.h"
#include "ScriptableNodes/ScriptableNode.h"
#include "ScriptableNodes/ScriptableNode_Entry.h"
#include "ScriptableNodes/ScriptableNode_SubGraph.h"
#include "ScriptableNodes/ScriptableNode_Task.h"
#include "ScriptableTasks/ScriptableTask.h"
#include "ScriptableTasks/ScriptableTask_SetLocal.h"
#include "Bindings/ScriptablePropertyBindings.h"
#include "ScriptableRuntimeData.h"
#include "Core/KzBagOps.h"

#if WITH_EDITOR
UScriptableGraph::FOnLaunchBlockedByCompile UScriptableGraph::OnLaunchBlockedByCompile;
UScriptableGraph::FOnPostLoaded UScriptableGraph::OnPostLoaded;
#endif

UScriptableGraph::UScriptableGraph()
{
}

UScriptableGraphInstance* UScriptableGraph::Run(UScriptableGraph* Graph, UObject* Owner, const FScriptableContext& InContext, FName Id)
{
	if (!Graph || !Owner) return nullptr;

	// Owner doubles as both the world context (to resolve the subsystem) and the graph's runtime owner.
	return UScriptableGraphSubsystem::RunGraph(Owner, Graph, Owner, InContext, Id);
}

void UScriptableGraph::PostInitProperties()
{
	Super::PostInitProperties();

	// Skip CDOs, archetypes and objects being loaded. Only freshly constructed live assets get an Entry here.
	if (HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad | RF_NeedPostLoad)) return;

	EnsureEntryNode();
}

void UScriptableGraph::PostLoad()
{
	Super::PostLoad();

	// Heal assets that lost their entry node (legacy data, external edits, etc.).
	EnsureEntryNode();
	if (IsContextBagOutOfSync())
	{
		RebuildContextBag();
	}
	PruneOrphanConnections();

#if WITH_EDITOR
	SnapshotContextNames();
	SnapshotLocalsNames();
	RebuildLocalsBagShape();
	OnPostLoaded.Broadcast(this);
#endif
}

#if WITH_EDITOR
void UScriptableGraph::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	const FName MemberName = (PropertyChangedEvent.MemberProperty != nullptr) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	// Every edit of Context or Locals reaches here, whichever path it takes, so renames and rebuilds all happen here.
	const bool bContextEdit = (PropertyName == GET_MEMBER_NAME_CHECKED(UScriptableObjectAsset, Context) || MemberName == GET_MEMBER_NAME_CHECKED(UScriptableObjectAsset, Context));
	if (bContextEdit)
	{
		DetectAndApplyContextRename();
		SnapshotContextNames();
		RebuildContextBag();
		RebakeNodeAutoBindings();
	}

	const bool bLocalsEdit = (PropertyName == GET_MEMBER_NAME_CHECKED(UScriptableGraph, Locals) || MemberName == GET_MEMBER_NAME_CHECKED(UScriptableGraph, Locals));
	if (bLocalsEdit)
	{
		DetectAndApplyLocalsRename();
		SnapshotLocalsNames();
		RebuildLocalsBagShape();
	}
}

void UScriptableGraph::RebuildLocalsBagShape()
{
	LocalsBagShape.Reset();
	if (Locals.IsEmpty()) return;

	TArray<FPropertyBagPropertyDesc> Descs;
	Descs.Reserve(Locals.Num());
	for (const FKzNamedVariant& Var : Locals)
	{
		// IsValid checks both Name (non-None) and Value.Type (non-None). Half-configured entries get skipped.
		if (!Var.IsValid()) continue;
		Descs.Add(Var.ToPropertyDesc());
	}
	if (Descs.Num() > 0)
	{
		LocalsBagShape.AddProperties(Descs);
	}
}

void UScriptableGraph::SnapshotContextNames()
{
	PreviousContextNames.Reset();
	PreviousContextNames.Reserve(Context.Num());
	for (const FKzParamDef& Param : Context)
	{
		PreviousContextNames.Add(Param.Name);
	}
}

void UScriptableGraph::DetectAndApplyContextRename()
{
	// Single-entry rename detection: same length + exactly one slot whose Name flipped.
	// Add/remove or multi-edit changes the length or yields multiple diffs, so skip.
	if (Context.Num() != PreviousContextNames.Num()) return;

	int32 DiffIndex = INDEX_NONE;
	int32 DiffCount = 0;
	for (int32 i = 0; i < Context.Num(); ++i)
	{
		if (Context[i].Name != PreviousContextNames[i])
		{
			DiffIndex = i;
			++DiffCount;
		}
	}
	if (DiffCount != 1 || DiffIndex == INDEX_NONE) return;

	const FName OldName = PreviousContextNames[DiffIndex];
	const FName NewName = Context[DiffIndex].Name;
	if (OldName.IsNone() || NewName.IsNone() || OldName == NewName) return;

	RedirectBindings(ScriptableBindingSources::ContextStructID, OldName, NewName);
}

namespace
{
	/** Rewrites the first segment of paths whose SourceID matches the expected one. Context uses an empty FGuid, Locals uses ScriptableBindingSources::LocalsStructID. */
	void RedirectInHolder(UScriptableObject* Holder, const FGuid& ExpectedSourceID, FName OldName, FName NewName)
	{
		if (!Holder) return;
		FScriptablePropertyBindings& Bindings = Holder->GetPropertyBindings();
		bool bAnyChanged = false;
		for (FScriptablePropertyBinding& B : Bindings.Bindings)
		{
			if (B.SourceID != ExpectedSourceID) continue;
			TArrayView<FPropertyBindingPathSegment> Segments = B.SourcePath.GetMutableSegments();
			if (Segments.Num() > 0 && Segments[0].GetName() == OldName)
			{
				Segments[0].SetName(NewName);
				bAnyChanged = true;
			}
		}
		if (bAnyChanged) Holder->Modify();
	}
}

void UScriptableGraph::RedirectBindings(const FGuid& ExpectedSourceID, FName OldName, FName NewName)
{
	for (const TObjectPtr<UScriptableNode>& Node : Nodes)
	{
		if (!Node) continue;
		RedirectInHolder(Node, ExpectedSourceID, OldName, NewName);

		// Task wrappers expose Task-level bindings on the inner UScriptableTask; those reference the same sources.
		if (UScriptableNode_Task* TaskWrapper = Cast<UScriptableNode_Task>(Node))
		{
			RedirectInHolder(TaskWrapper->Task, ExpectedSourceID, OldName, NewName);
		}
	}
}

void UScriptableGraph::RebakeNodeAutoBindings()
{
	// Context changed: re-discover auto-bindings on every node and its inner task.
	for (const TObjectPtr<UScriptableNode>& Node : Nodes)
	{
		if (!Node) continue;
		Node->BakeAutoBindings();
		if (UScriptableNode_Task* TaskWrapper = Cast<UScriptableNode_Task>(Node))
		{
			if (TaskWrapper->Task) { TaskWrapper->Task->BakeAutoBindings(); }
		}
	}
}

void UScriptableGraph::SnapshotLocalsNames()
{
	PreviousLocalsNames.Reset();
	PreviousLocalsNames.Reserve(Locals.Num());
	for (const FKzNamedVariant& Var : Locals)
	{
		PreviousLocalsNames.Add(Var.GetName());
	}
}

void UScriptableGraph::DetectAndApplyLocalsRename()
{
	if (Locals.Num() != PreviousLocalsNames.Num()) return;

	int32 DiffIndex = INDEX_NONE;
	int32 DiffCount = 0;
	for (int32 i = 0; i < Locals.Num(); ++i)
	{
		if (Locals[i].GetName() != PreviousLocalsNames[i])
		{
			DiffIndex = i;
			++DiffCount;
		}
	}
	if (DiffCount != 1 || DiffIndex == INDEX_NONE) return;

	const FName OldName = PreviousLocalsNames[DiffIndex];
	const FName NewName = Locals[DiffIndex].GetName();
	if (OldName.IsNone() || NewName.IsNone() || OldName == NewName) return;

	RedirectBindings(ScriptableBindingSources::LocalsStructID, OldName, NewName);
	RedirectSetLocalVarNames(OldName, NewName);
}

void UScriptableGraph::RedirectSetLocalVarNames(FName OldName, FName NewName)
{
	for (const TObjectPtr<UScriptableNode>& Node : Nodes)
	{
		UScriptableNode_Task* Wrapper = Cast<UScriptableNode_Task>(Node);
		if (!Wrapper) continue;
		UScriptableTask_SetLocal* SetLocal = Cast<UScriptableTask_SetLocal>(Wrapper->Task);
		if (!SetLocal || SetLocal->VarName != OldName) continue;

		SetLocal->Modify();
		SetLocal->VarName = NewName;
	}
}
#endif

void UScriptableGraph::EnsureEntryNode()
{
	// Validate the cached EntryNodeID: it must point to a real Entry node still living in Nodes.
	const bool bEntryIsValid = EntryNodeID.IsValid() && Nodes.ContainsByPredicate([this](const TObjectPtr<UScriptableNode>& Node)
		{
			return Node && Node->GetBindingID() == EntryNodeID && Node->IsA<UScriptableNode_Entry>();
		});

	if (bEntryIsValid) return;

	// Create a fresh Entry, owned by this asset.
	UScriptableNode_Entry* Entry = NewObject<UScriptableNode_Entry>(this, NAME_None, RF_Transactional);
	if (!Entry) return;

	Nodes.Add(Entry);
	EntryNodeID = Entry->GetBindingID();

#if WITH_EDITOR
	// Flag the asset dirty so the auto-repair persists.
	Modify();
#endif
}

bool UScriptableGraph::IsContextBagOutOfSync() const
{
	const UPropertyBag* BagStruct = ContextBag.GetPropertyBagStruct();
	const int32 BagPropertyCount = BagStruct ? BagStruct->GetPropertyDescs().Num() : 0;
	if (BagPropertyCount != Context.Num()) return true;

	// Names + types must match for the bag to be considered in sync. We don't deep-compare values.
	if (BagStruct)
	{
		for (const FKzParamDef& Param : Context)
		{
			const FPropertyBagPropertyDesc* Found = BagStruct->FindPropertyDescByName(Param.Name);
			if (!Found) return true;
			// Could also compare types here; for now matching by name is enough.
		}
	}

	return false;
}

void UScriptableGraph::RebuildContextBag()
{
	ContextBag.Reset();
	for (const FKzParamDef& Param : Context)
	{
		KzBagOps::AddProperty(ContextBag, Param);
	}
}

void UScriptableGraph::PruneOrphanConnections()
{
	if (Connections.IsEmpty()) return;

	// GUID -> node lookup for cross-checks. Node IDs are stable but pin names aren't (GetOutputPins can
	// change after edits), so validate both the endpoint node and the pin name's current presence.
	TMap<FGuid, UScriptableNode*> NodesByGuid;
	NodesByGuid.Reserve(Nodes.Num());
	for (const TObjectPtr<UScriptableNode>& Node : Nodes)
	{
		if (Node) NodesByGuid.Add(Node->GetBindingID(), Node);
	}

	auto IsValidEndpoint = [&NodesByGuid](const FScriptableGraphPinRef& Ref, bool bExpectOutput) -> bool
		{
			UScriptableNode* const* Found = NodesByGuid.Find(Ref.NodeID);
			if (!Found || !*Found) return false;
			const UScriptableNode* Node = *Found;

			// SubGraph's pin set is driven by a foreign asset; keeping connections to vanished
			// pins lets the editor render them as orphans on reload so the user can decide.
			if (Node->IsA<UScriptableNode_SubGraph>()) return true;
			const TArray<FName> Pins = bExpectOutput ? Node->GetOutputPins() : Node->GetInputPins();
			return Pins.Contains(Ref.PinName);
		};

	const int32 OriginalCount = Connections.Num();
	Connections.RemoveAll([&IsValidEndpoint](const FScriptableGraphConnection& Conn)
		{
			return !IsValidEndpoint(Conn.From, /*bExpectOutput*/ true) || !IsValidEndpoint(Conn.To, /*bExpectOutput*/ false);
		});

	if (Connections.Num() != OriginalCount)
	{
		// Asset was effectively modified; mark dirty so the next save persists the cleanup.
		MarkPackageDirty();
	}
}