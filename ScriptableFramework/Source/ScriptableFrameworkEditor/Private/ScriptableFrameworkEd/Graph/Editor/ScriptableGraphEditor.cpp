// Copyright 2026 kirzo

#include "ScriptableFrameworkEd/Graph/Editor/ScriptableGraphEditor.h"
#include "ScriptableFrameworkEd/Graph/Schema/ScriptableEdGraph.h"
#include "ScriptableFrameworkEd/Graph/ScriptableEdGraphNodeRegistry.h"
#include "ScriptableFrameworkEd/Graph/ScriptableEdGraphSchema.h"
#include "ScriptableFrameworkEd/Graph/ScriptableEdGraphNode.h"
#include "ScriptableFrameworkEd/Graph/ScriptableEdGraphNode_Native.h"
#include "ScriptableFrameworkEd/Graph/ScriptableGraphCommands.h"
#include "ScriptableNodes/ScriptableNode_Entry.h"
#include "ScriptableNodes/ScriptableNode_Sequence.h"
#include "ScriptableNodes/ScriptableNode_AND.h"
#include "ScriptableNodes/ScriptableNode_OR.h"
#include "ScriptableNodes/ScriptableNode_Branch.h"
#include "ScriptableNodes/ScriptableNode_GoTo.h"
#include "ScriptableNodes/ScriptableNode_ReceiveEvent.h"
#include "ScriptableNodes/ScriptableNode_Task.h"
#include "ScriptableNodes/ScriptableNode_Exit.h"
#include "SGraphPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"
#include "ScriptableFrameworkEd/Graph/ScriptableGraphEditorHelpers.h"
#include "ScriptableFrameworkEd/Customization/Widgets/SScriptableDefinitionsEditor.h"
#include "ScriptableFrameworkEd/Customization/Widgets/SScriptableTypePicker.h"
#include "ScriptableFrameworkEditorHelpers.h"
#include "ScriptableNodes/ScriptableGraph.h"
#include "ScriptableNodes/ScriptableNode.h"
#include "ScriptableTasks/ScriptableTask.h"
#include "ScriptableTasks/ScriptableActionAsset.h"
#include "ScriptableTasks/ScriptableTask_RunGraph.h"
#include "Engine/Blueprint.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"

#include "GraphEditor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "GraphEditorActions.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Utils/KzEditorUtils.h"
#include "Bindings/ScriptablePropertyBindings.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/ObjectSaveContext.h"
#include "Widgets/SKzValidationPanel.h"
#include "Validation/KzAssetValidationUtils.h"
#include "Core/KzValidationTypes.h"
#include "Logging/TokenizedMessage.h"
#include "ScriptableFrameworkEd/ScriptableGraphEditorSettings.h"
#include "ScriptableFrameworkEd/Debug/ScriptableDebugRegistry.h"
#include "ScriptableNodes/ScriptableGraphSubsystem.h"
#include "ScriptableNodes/ScriptableGraphInstance.h"
#include "Engine/Engine.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ScriptableNodes/ScriptableNode_ReceiveEvent.h"
#include "ScriptableNodes/ScriptableNode_SubGraph.h"
#include "ScriptableNodes/ScriptableNode_Finish.h"
#include "ScriptableNodes/ScriptableGraphConnection.h"
#include "Factories/ScriptableGraphAssetFactory.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "ScriptableGraphEditor"

// ---------------------------------------------------------------------------------------------
// Spawn-by-shortcut input processor: tracks which letter keys (S/B/G/E/A) are held, and on
// LMB-down over the graph editor, drops the matching node at the click position.
// ---------------------------------------------------------------------------------------------

class FScriptableGraphSpawnInputProcessor : public IInputProcessor
{
public:
	explicit FScriptableGraphSpawnInputProcessor(FScriptableGraphEditor* InOwner)
		: Owner(InOwner)
	{
		KeyToNodeClass.Add(EKeys::S, UScriptableNode_Sequence::StaticClass());
		KeyToNodeClass.Add(EKeys::B, UScriptableNode_Branch::StaticClass());
		KeyToNodeClass.Add(EKeys::G, UScriptableNode_GoTo::StaticClass());
		KeyToNodeClass.Add(EKeys::E, UScriptableNode_ReceiveEvent::StaticClass());
		KeyToNodeClass.Add(EKeys::A, UScriptableNode_AND::StaticClass());
		KeyToNodeClass.Add(EKeys::O, UScriptableNode_OR::StaticClass());
	}

	virtual void Tick(const float, FSlateApplication& SlateApp, TSharedRef<ICursor>) override
	{
		// If the app loses focus while a tracked key is held, the OS keeps the keydown but the keyup
		// is delivered to whichever window was focused on release — never to us. Without this purge
		// the stale key would silently arm the spawner forever, swallowing every subsequent LMB.
		if (!PressedKeys.IsEmpty() && !SlateApp.IsActive())
		{
			PressedKeys.Reset();
		}
	}

	virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& KeyEvent) override
	{
		// Only track keys we care about — and don't track when a text-input widget owns focus,
		// otherwise typing in a rename / details field would arm the spawner.
		if (!KeyToNodeClass.Contains(KeyEvent.GetKey())) return false;
		if (IsTextInputFocused()) return false;

		PressedKeys.Add(KeyEvent.GetKey());
		return false; // Pre-processor must not consume; downstream widgets still receive the event.
	}

	virtual bool HandleKeyUpEvent(FSlateApplication&, const FKeyEvent& KeyEvent) override
	{
		PressedKeys.Remove(KeyEvent.GetKey());
		return false;
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		if (!Owner || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) return false;
		if (PressedKeys.IsEmpty()) return false;

		// Resolve which held key wins (first match by registration order).
		UClass* NodeClass = nullptr;
		for (const TPair<FKey, UClass*>& Pair : KeyToNodeClass)
		{
			if (PressedKeys.Contains(Pair.Key))
			{
				NodeClass = Pair.Value;
				break;
			}
		}
		if (!NodeClass) return false;

		// Only intercept when the click landed inside our graph panel AND there is no interactive
		// widget (Add pin button, pin, node body) between the leaf and the panel. Walking the path
		// from leaf to root catches the case where the leaf is an inner SImage / STextBlock of an
		// SButton, where a strict leaf == panel check would still bypass the button.
		TSharedPtr<SGraphEditor> GraphEditor = Owner->GetGraphEditorWidget();
		if (!GraphEditor.IsValid()) return false;

		SGraphPanel* Panel = GraphEditor->GetGraphPanel();
		if (!Panel) return false;

		const FVector2f ScreenPos = MouseEvent.GetScreenSpacePosition();
		FWidgetPath WidgetPath = SlateApp.LocateWindowUnderMouse(ScreenPos, SlateApp.GetInteractiveTopLevelWindows());
		if (!WidgetPath.IsValid()) return false;

		const SWidget* PanelAsWidget = static_cast<const SWidget*>(Panel);
		bool bReachedPanel = false;
		for (int32 Idx = WidgetPath.Widgets.Num() - 1; Idx >= 0; --Idx)
		{
			const SWidget& W = WidgetPath.Widgets[Idx].Widget.Get();
			if (&W == PanelAsWidget) { bReachedPanel = true; break; }

			// Anything interactive between the leaf and the panel means the user is clicking that
			// thing (button, pin, node header, comment, etc.) — don't steal the click.
			const FName TypeName = W.GetType();
			if (TypeName == TEXT("SButton") || TypeName == TEXT("SCheckBox") ||
				TypeName == TEXT("SHyperlink") || TypeName == TEXT("SEditableText") ||
				TypeName == TEXT("SMultiLineEditableText") || TypeName == TEXT("SInlineEditableTextBlock") ||
				TypeName == TEXT("SGraphPin") || TypeName == TEXT("SGraphNode") ||
				TypeName == TEXT("SGraphNodeComment") || TypeName == TEXT("SGraphNodeKnot"))
			{
				return false;
			}
		}

		if (!bReachedPanel) return false;

		Owner->OnSpawnNativeNodeAtCursor(NodeClass);
		return true; // Consume so the click doesn't also start a pan / box-select.
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("ScriptableGraphSpawn"); }

private:
	bool IsTextInputFocused() const
	{
		const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetKeyboardFocusedWidget();
		if (!Focused.IsValid()) return false;

		// Any standard editable text widget. Covers SEditableText, SMultiLineEditableText, and the
		// inline rename block which forwards focus to its inner SEditableText.
		const FName Type = Focused->GetType();
		return Type == TEXT("SEditableText") || Type == TEXT("SMultiLineEditableText");
	}

	FScriptableGraphEditor* Owner = nullptr;
	TMap<FKey, UClass*> KeyToNodeClass;
	TSet<FKey> PressedKeys;
};

namespace
{
	// Clipboard-header prefixes. Start with ';' so the engine's T3D text factory skips them as comments.
	constexpr const TCHAR* PositionHeaderPrefix = TEXT(";SF_POS:");
	constexpr const TCHAR* ConnectionHeaderPrefix = TEXT(";SF_CONN:");

	struct FCopiedPositionEntry
	{
		FGuid OldNodeID;
		FVector2f Offset; // relative to selection centroid
	};

	struct FCopiedConnectionEntry
	{
		FGuid OldFromNodeID;
		FName FromPinName;
		FGuid OldToNodeID;
		FName ToPinName;
	};

	/** Builds the clipboard header (positions + internal connections) for a selection. */
	FString BuildClipboardHeader(const TArray<UScriptableEdGraphNode*>& SelectedEdNodes, const UScriptableGraph& Asset)
	{
		if (SelectedEdNodes.IsEmpty()) return FString();

		// Centroid: anchor relative to which each node's offset is stored. Pasting later applies
		// (cursor - centroid) to every offset, preserving the original group layout.
		FVector2f Centroid(0.f, 0.f);
		for (const UScriptableEdGraphNode* EdNode : SelectedEdNodes)
		{
			Centroid.X += EdNode->NodePosX;
			Centroid.Y += EdNode->NodePosY;
		}
		Centroid /= static_cast<float>(SelectedEdNodes.Num());

		// Snapshot the runtime IDs in the selection: connections crossing the boundary are dropped.
		TSet<FGuid> SelectedRuntimeIDs;
		SelectedRuntimeIDs.Reserve(SelectedEdNodes.Num());
		for (const UScriptableEdGraphNode* EdNode : SelectedEdNodes)
		{
			if (const UScriptableNode* Runtime = EdNode->GetRuntimeNode())
			{
				SelectedRuntimeIDs.Add(Runtime->GetBindingID());
			}
		}

		FString Header;

		for (const UScriptableEdGraphNode* EdNode : SelectedEdNodes)
		{
			const UScriptableNode* Runtime = EdNode->GetRuntimeNode();
			if (!Runtime) continue;

			const FVector2f Offset(EdNode->NodePosX - Centroid.X, EdNode->NodePosY - Centroid.Y);
			Header += FString::Printf(TEXT("%s%s|%f|%f\n"), PositionHeaderPrefix, *Runtime->GetBindingID().ToString(EGuidFormats::DigitsWithHyphens), Offset.X, Offset.Y);
		}

		for (const FScriptableGraphConnection& Conn : Asset.Connections)
		{
			if (!SelectedRuntimeIDs.Contains(Conn.From.NodeID) || !SelectedRuntimeIDs.Contains(Conn.To.NodeID)) continue;

			Header += FString::Printf(TEXT("%s%s|%s|%s|%s\n"),
				ConnectionHeaderPrefix,
				*Conn.From.NodeID.ToString(EGuidFormats::DigitsWithHyphens),
				*Conn.From.PinName.ToString(),
				*Conn.To.NodeID.ToString(EGuidFormats::DigitsWithHyphens),
				*Conn.To.PinName.ToString());
		}

		return Header;
	}

	/** Splits the clipboard text into header (positions + connections) and the T3D body. */
	void ParseClipboardHeader(const FString& FullText, TMap<FGuid, FVector2f>& OutOffsets, TArray<FCopiedConnectionEntry>& OutConnections, FString& OutBody)
	{
		OutOffsets.Reset();
		OutConnections.Reset();
		OutBody.Reset();

		TArray<FString> Lines;
		FullText.ParseIntoArrayLines(Lines, /*bCullEmpty*/ false);

		FString Body;
		Body.Reserve(FullText.Len());

		for (const FString& Line : Lines)
		{
			if (Line.StartsWith(PositionHeaderPrefix))
			{
				const FString Payload = Line.Mid(FCString::Strlen(PositionHeaderPrefix));
				TArray<FString> Parts;
				Payload.ParseIntoArray(Parts, TEXT("|"));
				if (Parts.Num() == 3)
				{
					FGuid Guid;
					if (FGuid::Parse(Parts[0], Guid))
					{
						OutOffsets.Add(Guid, FVector2f(FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2])));
					}
				}
			}
			else if (Line.StartsWith(ConnectionHeaderPrefix))
			{
				const FString Payload = Line.Mid(FCString::Strlen(ConnectionHeaderPrefix));
				TArray<FString> Parts;
				Payload.ParseIntoArray(Parts, TEXT("|"));
				if (Parts.Num() == 4)
				{
					FCopiedConnectionEntry Entry;
					if (FGuid::Parse(Parts[0], Entry.OldFromNodeID) && FGuid::Parse(Parts[2], Entry.OldToNodeID))
					{
						Entry.FromPinName = FName(*Parts[1]);
						Entry.ToPinName = FName(*Parts[3]);
						OutConnections.Add(Entry);
					}
				}
			}
			else
			{
				Body += Line;
				Body += TEXT("\n");
			}
		}

		OutBody = MoveTemp(Body);
	}

	/** Compares the ed-node's current pin set (names + directions) against what the runtime node declares. Returns true if anything differs, prompting a full Reconstruct; false if they're already in sync, in which case we leave the ed-node alone to avoid spurious dirty-flagging at editor open time. */
	bool ArePinsOutOfSyncWithRuntime(const UScriptableEdGraphNode* EdNode, const UScriptableNode* RuntimeNode)
	{
		if (!EdNode || !RuntimeNode) return false;

		const TArray<FName> RuntimeInputs = RuntimeNode->GetInputPins();
		const TArray<FName> RuntimeOutputs = RuntimeNode->GetOutputPins();

		int32 InputCount = 0;
		int32 OutputCount = 0;
		for (const UEdGraphPin* Pin : EdNode->Pins)
		{
			if (!Pin) continue;
			if (Pin->Direction == EGPD_Input)
			{
				if (!RuntimeInputs.Contains(Pin->PinName)) return true;
				++InputCount;
			}
			else
			{
				if (!RuntimeOutputs.Contains(Pin->PinName)) return true;
				++OutputCount;
			}
		}

		return InputCount != RuntimeInputs.Num() || OutputCount != RuntimeOutputs.Num();
	}
}

const FName FScriptableGraphEditor::GraphTabId(TEXT("ScriptableGraphEditor_Graph"));
const FName FScriptableGraphEditor::AssetDetailsTabId(TEXT("ScriptableGraphEditor_AssetDetails"));
const FName FScriptableGraphEditor::NodeDetailsTabId(TEXT("ScriptableGraphEditor_NodeDetails"));
const FName FScriptableGraphEditor::PaletteTabId(TEXT("ScriptableGraphEditor_Palette"));
const FName FScriptableGraphEditor::ValidationTabId(TEXT("ScriptableGraphEditor_Validation"));
const FName FScriptableGraphEditor::SearchTabId(TEXT("ScriptableGraphEditor_Search"));

static const FName ScriptableGraphEditorAppId(TEXT("ScriptableGraphEditorApp"));

void FScriptableGraphEditor::CreateEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, const TArray<UObject*>& InObjects)
{
	for (UObject* Object : InObjects)
	{
		if (UScriptableGraph* Graph = Cast<UScriptableGraph>(Object))
		{
			const TSharedRef<FScriptableGraphEditor> Editor = MakeShared<FScriptableGraphEditor>();
			Editor->Initialize(Mode, InitToolkitHost, Graph);
		}
	}
}

FScriptableGraphEditor::~FScriptableGraphEditor()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(OnObjectPropertyChangedHandle);
	FCoreUObjectDelegates::OnObjectTransacted.Remove(OnObjectTransactedHandle);
	UPackage::PackageSavedWithContextEvent.Remove(OnPackageSavedHandle);

	if (GraphChangedHandle.IsValid())
	{
		if (UScriptableGraph* Graph = EditedGraph.Get())
		{
			if (Graph->EdGraph)
			{
				Graph->EdGraph->RemoveOnGraphChangedHandler(GraphChangedHandle);
			}
		}
		GraphChangedHandle.Reset();
	}

	if (SpawnInputProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(SpawnInputProcessor);
	}
	SpawnInputProcessor.Reset();
}

void FScriptableGraphEditor::Initialize(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UScriptableGraph* InGraph)
{
	EditedGraph = InGraph;

	// Create the details views up-front so RegisterTabSpawners can hand them out.
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs ViewArgs;
	ViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	ViewArgs.bHideSelectionTip = true;

	AssetDetailsView = PropertyEditorModule.CreateDetailView(ViewArgs);
	NodeDetailsView = PropertyEditorModule.CreateDetailView(ViewArgs);

	if (AssetDetailsView.IsValid())
	{
		// Context, Locals and Outputs are edited in the table the details sit under.
		AssetDetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& PropertyAndParent)
			{
				const FName Name = PropertyAndParent.Property.GetFName();
				return PropertyAndParent.Property.GetOwnerClass() != UScriptableGraph::StaticClass() || (Name != GET_MEMBER_NAME_CHECKED(UScriptableGraph, Context) && Name != GET_MEMBER_NAME_CHECKED(UScriptableGraph, Locals) && Name != GET_MEMBER_NAME_CHECKED(UScriptableGraph, Outputs));
			}));
		AssetDetailsView->SetObject(InGraph);
	}

	if (NodeDetailsView.IsValid())
	{
		NodeDetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& PropertyAndParent)
			{
				return PropertyAndParent.Property.GetMetaData(TEXT("Category")) != TEXT("Tick");
			}));
	}

	BindGraphCommands();

	// Quick-spawn shortcuts: hold S/B/G/E/A and left-click on the graph to drop a node at the cursor.
	if (FSlateApplication::IsInitialized())
	{
		SpawnInputProcessor = MakeShared<FScriptableGraphSpawnInputProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(SpawnInputProcessor);
	}

	OnObjectPropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &FScriptableGraphEditor::OnRuntimeNodePropertyChanged);

	OnObjectTransactedHandle = FCoreUObjectDelegates::OnObjectTransacted.AddSP(
		StaticCastSharedRef<FScriptableGraphEditor>(AsShared()),
		&FScriptableGraphEditor::OnObjectTransacted);

	OnPackageSavedHandle = UPackage::PackageSavedWithContextEvent.AddSP(this, &FScriptableGraphEditor::OnPackageSaved);

	// Ensure the asset has a visual UEdGraph, then mirror its runtime Nodes onto it.
	InitEdGraph();
	ReconstructEdGraphFromAsset();

	if (InGraph && InGraph->EdGraph)
	{
		GraphChangedHandle = InGraph->EdGraph->AddOnGraphChangedHandler(
			FOnGraphChanged::FDelegate::CreateSP(this, &FScriptableGraphEditor::HandleGraphChanged));
	}

	// Default three-pane layout: details left | graph center | node details right.
	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("ScriptableGraphEditor_Layout_v4")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split
			(
				// Left column: Asset Details on top, Palette below — same split BP uses.
				FTabManager::NewSplitter()
				->SetSizeCoefficient(0.2f)
				->SetOrientation(Orient_Vertical)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.6f)
					->AddTab(AssetDetailsTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.4f)
					->AddTab(PaletteTabId, ETabState::OpenedTab)
				)
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetSizeCoefficient(0.6f)
				->SetOrientation(Orient_Vertical)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.75f)
					->AddTab(GraphTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.25f)
					->AddTab(SearchTabId, ETabState::ClosedTab)
				)
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetSizeCoefficient(0.2f)
				->SetOrientation(Orient_Vertical)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.6f)
					->AddTab(NodeDetailsTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.4f)
					->AddTab(ValidationTabId, ETabState::OpenedTab)
				)
			)
		);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, ScriptableGraphEditorAppId, Layout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, InGraph);

	ExtendToolbar();
	RegenerateMenusAndToolbars();

	/** Open-time compile: refreshes bLastCompileFailed from current state without bumping the package dirty flag. */
	RunCompile(/*bMarkPackageDirty*/ false);
}

FName FScriptableGraphEditor::GetToolkitFName() const
{
	return FName("ScriptableGraphEditor");
}

FText FScriptableGraphEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Scriptable Graph Editor");
}

FString FScriptableGraphEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "ScriptableGraph ").ToString();
}

FLinearColor FScriptableGraphEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor::White;
}

void FScriptableGraphEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	const TSharedRef<FWorkspaceItem> Category = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu", "Scriptable Graph Editor"));

	InTabManager->RegisterTabSpawner(GraphTabId, FOnSpawnTab::CreateSP(this, &FScriptableGraphEditor::SpawnTab_Graph))
		.SetDisplayName(LOCTEXT("GraphTab", "Graph"))
		.SetGroup(Category)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"));

	InTabManager->RegisterTabSpawner(AssetDetailsTabId, FOnSpawnTab::CreateSP(this, &FScriptableGraphEditor::SpawnTab_AssetDetails))
		.SetDisplayName(LOCTEXT("AssetDetailsTab", "Graph Details"))
		.SetGroup(Category)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(NodeDetailsTabId, FOnSpawnTab::CreateSP(this, &FScriptableGraphEditor::SpawnTab_NodeDetails))
		.SetDisplayName(LOCTEXT("NodeDetailsTab", "Node Details"))
		.SetGroup(Category)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Properties"));

	InTabManager->RegisterTabSpawner(PaletteTabId, FOnSpawnTab::CreateSP(this, &FScriptableGraphEditor::SpawnTab_Palette))
		.SetDisplayName(LOCTEXT("PaletteTabLabel", "Palette"))
		.SetGroup(Category)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Kismet.Tabs.Palette"));

	InTabManager->RegisterTabSpawner(ValidationTabId, FOnSpawnTab::CreateSP(this, &FScriptableGraphEditor::SpawnTab_Validation))
		.SetDisplayName(LOCTEXT("ValidationTabLabel", "Validation"))
		.SetGroup(Category)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.WarningWithColor"));

	InTabManager->RegisterTabSpawner(SearchTabId, FOnSpawnTab::CreateSP(this, &FScriptableGraphEditor::SpawnTab_Search))
		.SetDisplayName(LOCTEXT("SearchTabLabel", "Search"))
		.SetGroup(Category)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Search"));
}

void FScriptableGraphEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(GraphTabId);
	InTabManager->UnregisterTabSpawner(AssetDetailsTabId);
	InTabManager->UnregisterTabSpawner(NodeDetailsTabId);
	InTabManager->UnregisterTabSpawner(PaletteTabId);
	InTabManager->UnregisterTabSpawner(ValidationTabId);
	InTabManager->UnregisterTabSpawner(SearchTabId);
}

TSharedRef<SDockTab> FScriptableGraphEditor::SpawnTab_Graph(const FSpawnTabArgs& Args)
{
	UEdGraph* EdGraph = EditedGraph.IsValid() ? EditedGraph->EdGraph : nullptr;

	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnCreateActionMenuAtLocation = SGraphEditor::FOnCreateActionMenuAtLocation::CreateSP(this, &FScriptableGraphEditor::OnCreateNodeMenu);
	InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FScriptableGraphEditor::OnGraphSelectionChanged);
	// SGraphNode routes title commits through this event (it has no built-in persistence); without it
	// editable titles like comment boxes revert on commit. Forward to the node's OnRenameNode.
	InEvents.OnTextCommitted = FOnNodeTextCommitted::CreateSP(this, &FScriptableGraphEditor::OnNodeTitleCommitted);
	// Double-click opens the asset a node points to (a Blueprint task, an Action asset, or a sub-graph).
	InEvents.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &FScriptableGraphEditor::OnNodeDoubleClicked);

	FGraphAppearanceInfo AppearanceInfo;
	AppearanceInfo.CornerText = LOCTEXT("ScriptableGraphAppearanceCornerText", "SCRIPTABLE GRAPH");

	SAssignNew(GraphEditorWidget, SGraphEditor)
		.AdditionalCommands(GetToolkitCommands())
		.GraphToEdit(EdGraph)
		.GraphEvents(InEvents)
		.Appearance(AppearanceInfo);

	return SNew(SDockTab)
		.Label(LOCTEXT("GraphTab", "Graph"))
		[
			GraphEditorWidget.ToSharedRef()
		];
}

TSharedRef<SDockTab> FScriptableGraphEditor::SpawnTab_AssetDetails(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("AssetDetailsTab", "Graph Details"))
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)
			+ SSplitter::Slot()
			.Value(0.75f)
			[
				SNew(SScriptableDefinitionsEditor, MakeShared<FScriptableGraphDefinitionsSource>(EditedGraph.Get()))
				.Tab(SScriptableDefinitionsEditor::ETab::Context)
			]
			+ SSplitter::Slot()
			.Value(0.25f)
			[
				AssetDetailsView.IsValid() ? AssetDetailsView.ToSharedRef() : SNullWidget::NullWidget
			]
		];
}

TSharedRef<SDockTab> FScriptableGraphEditor::SpawnTab_NodeDetails(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("NodeDetailsTab", "Node Details"))
		[
			NodeDetailsView.IsValid() ? NodeDetailsView.ToSharedRef() : SNullWidget::NullWidget
		];
}

TSharedRef<SDockTab> FScriptableGraphEditor::SpawnTab_Palette(const FSpawnTabArgs& Args)
{
	TSharedRef<SScriptableTypeSelector> Selector = SNew(SScriptableTypeSelector)
		.BaseClass(UScriptableTask::StaticClass())
		.ClassCategoryMeta(ScriptableFrameworkEditor::MD_TaskCategory)
		.AdditionalBaseClass(UScriptableNode::StaticClass())
		.AdditionalClassCategoryMeta(ScriptableFrameworkEditor::MD_NodeCategory)
		.BaseClassRootCategory(LOCTEXT("PaletteScriptableTasks", "Scriptable Tasks"))
		.AdditionalBaseClassRootCategory(LOCTEXT("PaletteNativeNodes", "Native Nodes"))
		.AdditionalBaseClassUserRootCategory(LOCTEXT("PaletteScriptableNodes", "Scriptable Nodes"))
		.ExcludedClasses({ UScriptableTask_RunGraph::StaticClass() })
		.EnableDragOut(true);

	return SNew(SDockTab)
		.Label(LOCTEXT("PaletteTabLabel", "Palette"))
		[
			Selector
		];
}

void FScriptableGraphEditor::InitEdGraph()
{
	UScriptableGraph* Graph = EditedGraph.Get();
	if (!Graph || Graph->EdGraph) return;

	UScriptableEdGraph* NewEdGraph = NewObject<UScriptableEdGraph>(Graph, UScriptableEdGraph::StaticClass(), NAME_None, RF_Transactional);
	NewEdGraph->Schema = UScriptableEdGraphSchema::StaticClass();
	Graph->EdGraph = NewEdGraph;
}

void FScriptableGraphEditor::ReconstructEdGraphFromAsset()
{
	BuildEdGraphForAsset(EditedGraph.Get());
}

void FScriptableGraphEditor::BuildEdGraphForAsset(UScriptableGraph* Graph)
{
	if (!Graph) return;

	// Ensure the ed-graph exists (matches InitEdGraph) so this works on revisions opened outside a toolkit.
	if (!Graph->EdGraph)
	{
		UScriptableEdGraph* NewEdGraph = NewObject<UScriptableEdGraph>(Graph, UScriptableEdGraph::StaticClass(), NAME_None, RF_Transactional);
		NewEdGraph->Schema = UScriptableEdGraphSchema::StaticClass();
		Graph->EdGraph = NewEdGraph;
	}

	// Rebuilding the visual graph is a derived-view sync, not a user edit, so it must not dirty the
	// asset. AddNode / MakeLinkTo call Modify() internally, so snapshot the package dirty state and
	// restore it at the end. Callers that made a real edit before us leave it dirty (snapshot is true).
	UPackage* Package = Graph->GetPackage();
	const bool bWasDirty = Package && Package->IsDirty();

	TSet<UScriptableNode*> AlreadyVisualized;
	TMap<FGuid, UScriptableEdGraphNode*> EdNodeByID;

	// 1. Sync visual pins and wipe visual links to prepare for a clean rebuild from the asset.
	for (UEdGraphNode* EdNode : Graph->EdGraph->Nodes)
	{
		if (UScriptableEdGraphNode* SfEdNode = Cast<UScriptableEdGraphNode>(EdNode))
		{
			if (UScriptableNode* RuntimeNode = SfEdNode->GetRuntimeNode())
			{
				// Only reconstruct pins if they're actually out of sync with the runtime.
				if (ArePinsOutOfSyncWithRuntime(SfEdNode, RuntimeNode))
				{
					SfEdNode->ReconstructNode();
				}

				AlreadyVisualized.Add(RuntimeNode);
				EdNodeByID.Add(RuntimeNode->GetBindingID(), SfEdNode);
			}

			// Manually break all visual links without triggering the schema
			for (UEdGraphPin* Pin : SfEdNode->Pins)
			{
				for (UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (Linked) Linked->LinkedTo.Remove(Pin);
				}
				Pin->LinkedTo.Empty();
			}
		}
	}

	// 2. Spawn ed-nodes for any runtime node not yet represented.
	for (const TObjectPtr<UScriptableNode>& RuntimeNode : Graph->Nodes)
	{
		if (!RuntimeNode || AlreadyVisualized.Contains(RuntimeNode)) continue;

		UClass* EdNodeClass = FScriptableEdGraphNodeRegistry::FindEdNodeClassFor(RuntimeNode);
		if (!EdNodeClass) EdNodeClass = UScriptableEdGraphNode_Native::StaticClass();

		UScriptableEdGraphNode* NewEdNode = NewObject<UScriptableEdGraphNode>(Graph->EdGraph, EdNodeClass, NAME_None, RF_Transactional);

		if (NewEdNode)
		{
			NewEdNode->SetRuntimeNode(RuntimeNode);
			NewEdNode->CreateNewGuid();
			NewEdNode->AllocateDefaultPins();
			Graph->EdGraph->AddNode(NewEdNode, /*bUserAction*/ false, /*bSelectNewNode*/ false);

			EdNodeByID.Add(RuntimeNode->GetBindingID(), NewEdNode);
		}
	}

	// 3. Re-link pins according to the persisted FScriptableGraphConnection list. Missing pins on a
	// SubGraph endpoint are recreated as orphans so wires that pointed at a now-vanished event/output
	// stay visible (red) instead of disappearing silently — same UX as Blueprint.
	auto ResurrectOrphanPin = [](UScriptableEdGraphNode* EdNode, FName PinName, EEdGraphPinDirection Direction) -> UEdGraphPin*
		{
			if (!EdNode || !EdNode->GetRuntimeNode()) return nullptr;
			if (!EdNode->GetRuntimeNode()->IsA<UScriptableNode_SubGraph>()) return nullptr;
			UEdGraphPin* Orphan = EdNode->CreatePin(Direction, UScriptableEdGraphNode::ScriptableExecPinCategory, PinName);
			if (Orphan) Orphan->bOrphanedPin = true;
			return Orphan;
		};

	for (const FScriptableGraphConnection& Conn : Graph->Connections)
	{
		UScriptableEdGraphNode* FromNode = EdNodeByID.FindRef(Conn.From.NodeID);
		UScriptableEdGraphNode* ToNode = EdNodeByID.FindRef(Conn.To.NodeID);
		if (!FromNode || !ToNode) continue;

		UEdGraphPin* FromPin = FromNode->FindPin(Conn.From.PinName, EGPD_Output);
		if (!FromPin) FromPin = ResurrectOrphanPin(FromNode, Conn.From.PinName, EGPD_Output);
		UEdGraphPin* ToPin = ToNode->FindPin(Conn.To.PinName, EGPD_Input);
		if (!ToPin) ToPin = ResurrectOrphanPin(ToNode, Conn.To.PinName, EGPD_Input);
		if (!FromPin || !ToPin) continue;

		FromPin->MakeLinkTo(ToPin);
	}

	Graph->EdGraph->NotifyGraphChanged();

	// Drop the dirtiness introduced purely by rebuilding the view (see snapshot above).
	if (Package && !bWasDirty)
	{
		Package->SetDirtyFlag(false);
	}
}

FActionMenuContent FScriptableGraphEditor::OnCreateNodeMenu(UEdGraph* InGraph, const FVector2f& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed)
{
	// Capture spawn context by value so the deferred picker callback can use it after the menu closes.
	const FVector2f CapturedLocation(InNodePosition);
	TArray<UEdGraphPin*> CapturedPins = InDraggedPins;

	FSimpleDelegate MenuClosedAdapter = FSimpleDelegate::CreateLambda([InOnMenuClosed]()
		{
			InOnMenuClosed.ExecuteIfBound();
		});

	TSharedRef<SScriptableTypeSelector> TypeSelector = SNew(SScriptableTypeSelector)
		.TitleText(LOCTEXT("ContextMenuTitle", "Select Node"))
		.BaseClass(UScriptableTask::StaticClass())
		.ClassCategoryMeta(ScriptableFrameworkEditor::MD_TaskCategory)
		.AdditionalBaseClass(UScriptableNode::StaticClass())
		.AdditionalClassCategoryMeta(ScriptableFrameworkEditor::MD_NodeCategory)
		.BaseClassRootCategory(LOCTEXT("PickerScriptableTasks", "Scriptable Tasks"))
		.AdditionalBaseClassRootCategory(LOCTEXT("PickerNativeNodes", "Native Nodes"))
		.AdditionalBaseClassUserRootCategory(LOCTEXT("PickerScriptableNodes", "Scriptable Nodes"))
		.ExcludedClasses({ UScriptableTask_RunGraph::StaticClass() })
		.OnNodeTypePicked(SScriptableTypeSelector::FOnNodeTypePicked::CreateSP(this, &FScriptableGraphEditor::OnNodeMenuTypePicked, InGraph, CapturedLocation, CapturedPins))
		.OnPickerClosed(MenuClosedAdapter);

	return FActionMenuContent(TypeSelector, TypeSelector->GetWidgetToFocusOnOpen());
}

void FScriptableGraphEditor::OnNodeMenuTypePicked(const UStruct* InStruct, const FAssetData& InAssetData, UEdGraph* InGraph, FVector2f InLocation, TArray<UEdGraphPin*> InDraggedPins)
{
	UEdGraphPin* FromPin = (InDraggedPins.Num() > 0) ? InDraggedPins[0] : nullptr;

	UEdGraphNode* SpawnedNode = nullptr;

	// Asset path: a UScriptableActionAsset spawns a Task wrapper around UScriptableTask_RunAsset
	// pointing at the picked asset. From there it behaves like any other task.
	if (InAssetData.IsValid())
	{
		UScriptableActionAsset* ActionAsset = Cast<UScriptableActionAsset>(InAssetData.GetAsset());
		if (ActionAsset)
		{
			SpawnedNode = ScriptableGraphEditorHelpers::SpawnTaskNode(InGraph, UScriptableTask_RunAsset::StaticClass(), InLocation, FromPin, /*bSelectNewNode*/ true);

			if (UScriptableEdGraphNode* SfEdNode = Cast<UScriptableEdGraphNode>(SpawnedNode))
			{
				if (UScriptableNode_Task* WrapperRuntime = Cast<UScriptableNode_Task>(SfEdNode->GetRuntimeNode()))
				{
					if (UScriptableTask_RunAsset* RunAssetTask = Cast<UScriptableTask_RunAsset>(WrapperRuntime->Task))
					{
						RunAssetTask->Modify();
						RunAssetTask->Asset = ActionAsset;

						// Refresh the node title so the canvas shows the asset name immediately rather
						// than the generic "Run Asset" default.
						SfEdNode->ReconstructNode();
					}
				}
			}
		}
	}
	else
	{
		const UClass* PickedClass = Cast<UClass>(InStruct);
		if (PickedClass)
		{
			if (PickedClass->IsChildOf(UScriptableTask::StaticClass()))
			{
				SpawnedNode = ScriptableGraphEditorHelpers::SpawnTaskNode(InGraph, const_cast<UClass*>(PickedClass), InLocation, FromPin, /*bSelectNewNode*/ true);
			}
			else if (PickedClass->IsChildOf(UScriptableNode::StaticClass()))
			{
				// Exit is unique per graph: refuse a second one (the validator also enforces this).
				bool bBlockedExit = false;
				if (PickedClass->IsChildOf(UScriptableNode_Exit::StaticClass()) && InGraph)
				{
					for (const UEdGraphNode* EdNode : InGraph->Nodes)
					{
						const UScriptableEdGraphNode* SfEd = Cast<UScriptableEdGraphNode>(EdNode);
						if (SfEd && SfEd->GetRuntimeNode() && SfEd->GetRuntimeNode()->IsA<UScriptableNode_Exit>())
						{
							FNotificationInfo Info(LOCTEXT("OneExit", "A graph can only have one Exit node."));
							Info.ExpireDuration = 3.0f;
							FSlateNotificationManager::Get().AddNotification(Info);
							bBlockedExit = true;
							break;
						}
					}
				}

				if (!bBlockedExit)
				{
					SpawnedNode = ScriptableGraphEditorHelpers::SpawnNativeNode(InGraph, const_cast<UClass*>(PickedClass), InLocation, FromPin, /*bSelectNewNode*/ true);
				}
			}
		}
	}

	// Dismiss the menu now that the spawn has executed. The picker's destructor will fire
	// OnPickerClosed → InOnMenuClosed afterwards, and SGraphPanel cleans up its drag-off state.
	FSlateApplication::Get().DismissAllMenus();

	if (SpawnedNode && GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->ClearSelectionSet();
		GraphEditorWidget->SetNodeSelection(SpawnedNode, true);
	}
}

void FScriptableGraphEditor::OnPaletteTypePicked(const UStruct* InStruct, const FAssetData& InAssetData)
{
	if (!GraphEditorWidget.IsValid()) return;

	UEdGraph* Graph = GraphEditorWidget->GetCurrentGraph();
	if (!Graph) return;

	const FVector2f Location = GraphEditorWidget->GetPasteLocation2f();

	// Reuse the same dispatch the right-click menu uses. Pass an empty pin list since there's no
	// drag-off context to wire from.
	OnNodeMenuTypePicked(InStruct, InAssetData, Graph, Location, TArray<UEdGraphPin*>());
}

// ---------------------------------------------------------------------------------------------
// Keyboard commands
// ---------------------------------------------------------------------------------------------

void FScriptableGraphEditor::BindGraphCommands()
{
	const TSharedRef<FUICommandList> Commands = GetToolkitCommands();
	const FGenericCommands& Generic = FGenericCommands::Get();

	Commands->MapAction(Generic.Delete,
		FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnDeleteSelected),
		FCanExecuteAction::CreateSP(this, &FScriptableGraphEditor::CanDelete));

	Commands->MapAction(FGraphEditorCommands::Get().DeleteAndReconnectNodes,
		FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnDeleteAndReconnectNodes),
		FCanExecuteAction::CreateSP(this, &FScriptableGraphEditor::CanDeleteAndReconnectNodes));

	Commands->MapAction(Generic.Copy,
		FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnCopySelected),
		FCanExecuteAction::CreateSP(this, &FScriptableGraphEditor::CanCopy));

	Commands->MapAction(Generic.Cut,
		FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnCutSelected),
		FCanExecuteAction::CreateSP(this, &FScriptableGraphEditor::CanCut));

	Commands->MapAction(Generic.Paste,
		FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnPasteAtCursor),
		FCanExecuteAction::CreateSP(this, &FScriptableGraphEditor::CanPaste));

	Commands->MapAction(Generic.Duplicate,
		FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnDuplicateSelected),
		FCanExecuteAction::CreateSP(this, &FScriptableGraphEditor::CanDuplicate));

	Commands->MapAction(Generic.SelectAll,
		FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnSelectAllNodes),
		FCanExecuteAction::CreateSP(this, &FScriptableGraphEditor::CanSelectAll));

	// Rename (F2): inline-edit the selected node's title (e.g. an Event node's name).
	Commands->MapAction(Generic.Rename,
		FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnRenameSelectedNode),
		FCanExecuteAction::CreateSP(this, &FScriptableGraphEditor::CanRenameSelectedNode));

	Commands->MapAction(FGraphEditorCommands::Get().CreateComment,
		FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnCreateComment));

	// Undo/Redo: forward to GEditor's transaction stack so the shortcuts work while focus is in the graph widget.
	Commands->MapAction(Generic.Undo,
		FExecuteAction::CreateLambda([]() { if (GEditor) GEditor->UndoTransaction(); }));

	Commands->MapAction(Generic.Redo,
		FExecuteAction::CreateLambda([]() { if (GEditor) GEditor->RedoTransaction(); }));

	// Pin context: Remove pin. Resolved against this command list (SGraphEditor was built with AdditionalCommands = GetToolkitCommands()).
	Commands->MapAction(FScriptableGraphCommands::Get().RemoveSequencePin,
		FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnRemoveSequencePin),
		FCanExecuteAction::CreateSP(this, &FScriptableGraphEditor::CanRemoveSequencePin));

	Commands->MapAction(FScriptableGraphCommands::Get().RemoveANDPin,
		FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnRemoveANDPin),
		FCanExecuteAction::CreateSP(this, &FScriptableGraphEditor::CanRemoveANDPin));

	Commands->MapAction(FScriptableGraphCommands::Get().RemoveORPin,
		FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnRemoveORPin),
		FCanExecuteAction::CreateSP(this, &FScriptableGraphEditor::CanRemoveORPin));

	const FScriptableGraphCommands& SfCommands = FScriptableGraphCommands::Get();
	const FCanExecuteAction CanAlign = FCanExecuteAction::CreateLambda([this]() { return GraphEditorWidget.IsValid() && GraphEditorWidget->GetSelectedNodes().Num() >= 2; });
	const FCanExecuteAction CanDistribute = FCanExecuteAction::CreateLambda([this]() { return GraphEditorWidget.IsValid() && GraphEditorWidget->GetSelectedNodes().Num() >= 3; });

	Commands->MapAction(SfCommands.AlignNodesTop,    FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnAlignNodesTop),    CanAlign);
	Commands->MapAction(SfCommands.AlignNodesBottom, FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnAlignNodesBottom), CanAlign);
	Commands->MapAction(SfCommands.AlignNodesLeft,   FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnAlignNodesLeft),   CanAlign);
	Commands->MapAction(SfCommands.AlignNodesRight,  FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnAlignNodesRight),  CanAlign);
	Commands->MapAction(SfCommands.AlignNodesMiddle, FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnAlignNodesMiddle), CanAlign);
	Commands->MapAction(SfCommands.AlignNodesCenter, FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnAlignNodesCenter), CanAlign);
	Commands->MapAction(SfCommands.DistributeNodesHorizontally, FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnDistributeNodesHorizontally), CanDistribute);
	Commands->MapAction(SfCommands.DistributeNodesVertically,   FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnDistributeNodesVertically),   CanDistribute);
	Commands->MapAction(SfCommands.ZoomToSelection,  FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnZoomToSelection),
		FCanExecuteAction::CreateLambda([this]() { return GraphEditorWidget.IsValid() && GraphEditorWidget->GetSelectedNodes().Num() > 0; }));

	Commands->MapAction(SfCommands.ToggleBreakpoint, FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnToggleBreakpoint),
		FCanExecuteAction::CreateLambda([this]() { return GraphEditorWidget.IsValid() && GraphEditorWidget->GetSelectedNodes().Num() > 0; }));

	Commands->MapAction(SfCommands.ConvertSelectionToSubGraph,
		FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnConvertSelectionToSubGraph),
		FCanExecuteAction::CreateSP(this, &FScriptableGraphEditor::CanConvertSelectionToSubGraph));

	Commands->MapAction(SfCommands.OpenSearch, FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnOpenSearch));
}

bool FScriptableGraphEditor::HasAnyNodesSelected() const
{
	return GraphEditorWidget.IsValid() && GraphEditorWidget->GetSelectedNodes().Num() > 0;
}

bool FScriptableGraphEditor::HasAnyDeletableSelected() const
{
	if (!GraphEditorWidget.IsValid()) return false;

	for (UObject* Selected : GraphEditorWidget->GetSelectedNodes())
	{
		if (const UEdGraphNode* Node = Cast<UEdGraphNode>(Selected))
		{
			if (Node->CanUserDeleteNode()) return true;
		}
	}
	return false;
}

bool FScriptableGraphEditor::HasAnyCopyableSelected() const
{
	if (!GraphEditorWidget.IsValid()) return false;

	for (UObject* Selected : GraphEditorWidget->GetSelectedNodes())
	{
		if (const UEdGraphNode* Node = Cast<UEdGraphNode>(Selected))
		{
			if (Node->CanDuplicateNode()) return true;
		}
	}
	return false;
}

bool FScriptableGraphEditor::CanPaste() const
{
	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);
	if (ClipboardContent.IsEmpty()) return false;

	// Mirrors FScriptableObjectCustomization::OnPasteNode: the clipboard text must parse into a UScriptableNode.
	TKzObjectTextFactory<UScriptableNode> Factory;
	return Factory.CanCreateObjectsFromText(ClipboardContent);
}

void FScriptableGraphEditor::OnDeleteSelected()
{
	if (!GraphEditorWidget.IsValid() || !EditedGraph.IsValid() || !EditedGraph->EdGraph) return;

	const FScopedTransaction Transaction(LOCTEXT("DeleteSelectedNodes", "Delete Selected Nodes"));

	UScriptableGraph* GraphAsset = EditedGraph.Get();
	UEdGraph* EdGraph = GraphAsset->EdGraph;

	GraphAsset->Modify();
	EdGraph->Modify();

	const FGraphPanelSelectionSet Selection = GraphEditorWidget->GetSelectedNodes();
	GraphEditorWidget->ClearSelectionSet();

	for (UObject* Selected : Selection)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(Selected);
		if (!Node || !Node->CanUserDeleteNode()) continue;

		// DestroyNode on UScriptableEdGraphNode also strips the runtime node and its wires
		// from the asset (Entry is exempted there).
		Node->Modify();
		Node->DestroyNode();
	}

	GraphEditorWidget->NotifyGraphChanged();
}

bool FScriptableGraphEditor::CanDeleteAndReconnectNodes() const
{
	if (!GraphEditorWidget.IsValid()) return false;
	return GraphEditorWidget->GetSelectedNodes().Num() > 0;
}

void FScriptableGraphEditor::OnDeleteAndReconnectNodes()
{
	if (!GraphEditorWidget.IsValid()) return;

	const FGraphPanelSelectionSet Selection = GraphEditorWidget->GetSelectedNodes();
	if (Selection.Num() == 0) return;

	UEdGraph* Graph = GraphEditorWidget->GetCurrentGraph();
	if (!Graph) return;

	const FScopedTransaction Transaction(LOCTEXT("DeleteAndReconnectTx", "Delete and Reconnect"));
	Graph->Modify();

	for (UObject* SelectedObj : Selection)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(SelectedObj);
		if (!Node || !Node->CanUserDeleteNode()) continue;

		for (UEdGraphPin* InputPin : Node->Pins)
		{
			if (!InputPin || InputPin->Direction != EGPD_Input) continue;

			for (UEdGraphPin* OutputPin : Node->Pins)
			{
				if (!OutputPin || OutputPin->Direction != EGPD_Output) continue;

				// Snapshot the peer arrays — we'll mutate LinkedTo while iterating otherwise.
				TArray<UEdGraphPin*> UpstreamPeers = InputPin->LinkedTo;
				TArray<UEdGraphPin*> DownstreamPeers = OutputPin->LinkedTo;

				for (UEdGraphPin* Up : UpstreamPeers)
				{
					for (UEdGraphPin* Down : DownstreamPeers)
					{
						if (!Up || !Down) continue;
						if (Up->GetOwningNode() == Node || Down->GetOwningNode() == Node) continue;

						// Route through the schema so Asset->Connections stays in sync (the schema
						// owns the persist-to-runtime step on TryCreateConnection).
						const UEdGraphSchema* Schema = Node->GetSchema();
						if (Schema)
						{
							Schema->TryCreateConnection(Up, Down);
						}
					}
				}
			}
		}

		Node->Modify();
		Node->DestroyNode();
	}

	Graph->NotifyGraphChanged();
}

void FScriptableGraphEditor::OnCopySelected()
{
	if (!GraphEditorWidget.IsValid() || !EditedGraph.IsValid()) return;

	// Single-node copies skip the header entirely so the clipboard stays interoperable with the
	// details-view clipboard (FScriptableObjectCustomization::OnCopyNode).
	TArray<UScriptableEdGraphNode*> SelectedEdNodes;
	TArray<UObject*> RuntimeNodes;
	for (UObject* Selected : GraphEditorWidget->GetSelectedNodes())
	{
		UScriptableEdGraphNode* SfNode = Cast<UScriptableEdGraphNode>(Selected);
		if (!SfNode || !SfNode->CanDuplicateNode()) continue;

		if (UScriptableNode* RuntimeNode = SfNode->GetRuntimeNode())
		{
			SelectedEdNodes.Add(SfNode);
			RuntimeNodes.Add(RuntimeNode);
		}
	}

	if (RuntimeNodes.IsEmpty()) return;

	FKzClipboardUtils::CopyObjectsToClipboard(RuntimeNodes);

	if (SelectedEdNodes.Num() > 1)
	{
		// Prepend our graph-specific header. T3D treats lines starting with ';' as comments, so
		// builds opening this clipboard outside the graph editor still parse correctly.
		FString CurrentClipboard;
		FPlatformApplicationMisc::ClipboardPaste(CurrentClipboard);

		const FString Header = BuildClipboardHeader(SelectedEdNodes, *EditedGraph.Get());
		const FString WithHeader = Header + CurrentClipboard;
		FPlatformApplicationMisc::ClipboardCopy(*WithHeader);
	}
}

void FScriptableGraphEditor::OnCutSelected()
{
	// Cut = Copy first eligible + Delete all eligible. Single transaction so undo is atomic.
	const FScopedTransaction Transaction(LOCTEXT("CutSelectedNodes", "Cut Selected Nodes"));
	OnCopySelected();
	OnDeleteSelected();
}

void FScriptableGraphEditor::OnPasteAtCursor()
{
	if (!GraphEditorWidget.IsValid() || !EditedGraph.IsValid() || !EditedGraph->EdGraph) return;

	UScriptableGraph* GraphAsset = EditedGraph.Get();
	UEdGraph* EdGraph = GraphAsset->EdGraph;

	// Peel our header (positions + connections) off the raw clipboard before handing the T3D body to
	// the factory. Single-node clipboards (from details views) have no header → cascade fallback.
	FString FullClipboardText;
	FPlatformApplicationMisc::ClipboardPaste(FullClipboardText);
	if (FullClipboardText.IsEmpty()) return;

	TMap<FGuid, FVector2f> OldNodeOffsets;
	TArray<FCopiedConnectionEntry> CopiedConnections;
	FString T3DBody;
	ParseClipboardHeader(FullClipboardText, OldNodeOffsets, CopiedConnections, T3DBody);

	// Run the body through the factory directly (parallel of FKzClipboardUtils::PasteObjectsFromClipboard,
	// but the buffer comes from the stripped text rather than the raw clipboard).
	TArray<UScriptableNode*> NewRuntimeNodes;
	{
		TKzObjectTextFactory<UScriptableNode> Factory;
		if (Factory.CanCreateObjectsFromText(T3DBody))
		{
			Factory.ProcessBuffer(GraphAsset, RF_Transactional, T3DBody);
			NewRuntimeNodes = MoveTemp(Factory.CreatedObjects);
		}
	}
	if (NewRuntimeNodes.IsEmpty()) return;

	// Entry is unique per graph (auto-repaired by EnsureEntryNode). Strip any Entry from the
	// paste payload and warn — never block the whole paste because of an Entry sneaking in.
	bool bRefusedAnEntry = false;
	NewRuntimeNodes.RemoveAll([&bRefusedAnEntry](UScriptableNode* Node)
		{
			if (Node && Node->IsA<UScriptableNode_Entry>())
			{
				bRefusedAnEntry = true;
				Node->ClearFlags(RF_Transactional);
				Node->MarkAsGarbage();
				return true;
			}
			return false;
		});

	if (bRefusedAnEntry)
	{
		FNotificationInfo Info(LOCTEXT("PasteEntryRefused", "Skipped Entry node(s) — graphs have exactly one."));
		Info.ExpireDuration = 3.0f;
		Info.Image = FAppStyle::GetBrush("Icons.Warning");
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	if (NewRuntimeNodes.IsEmpty()) return;

	const FScopedTransaction Transaction(LOCTEXT("PasteNodes", "Paste Nodes"));
	GraphAsset->Modify();
	EdGraph->Modify();

	const FVector2f BasePasteLocation = GraphEditorWidget->GetPasteLocation2f();
	const FVector2f CascadeStep(20.f, 20.f); // Fallback for clipboards without our header.

	// Build OldID -> new runtime map as we paste, to rewire connections afterwards. Walk NewRuntimeNodes
	// and the ;SF_POS order in parallel (same order CopyObjectsToClipboard exported them).
	TArray<FGuid> OldIDsInOrder;
	OldIDsInOrder.Reserve(OldNodeOffsets.Num());
	{
		// Re-parse the header in line order so OldIDsInOrder matches the body's export order.
		TArray<FString> Lines;
		FullClipboardText.ParseIntoArrayLines(Lines, /*bCullEmpty*/ false);
		for (const FString& Line : Lines)
		{
			if (!Line.StartsWith(PositionHeaderPrefix)) continue;
			TArray<FString> Parts;
			Line.Mid(FCString::Strlen(PositionHeaderPrefix)).ParseIntoArray(Parts, TEXT("|"));
			FGuid Guid;
			if (Parts.Num() >= 1 && FGuid::Parse(Parts[0], Guid))
			{
				OldIDsInOrder.Add(Guid);
			}
		}
	}

	TMap<FGuid, UScriptableNode*> OldIDToNewRuntime;
	OldIDToNewRuntime.Reserve(NewRuntimeNodes.Num());

	GraphEditorWidget->ClearSelectionSet();

	for (int32 i = 0; i < NewRuntimeNodes.Num(); ++i)
	{
		UScriptableNode* NewRuntimeNode = NewRuntimeNodes[i];

		// The deserialized runtime got a fresh BindingID, but its PropertyBindings still carry the
		// old one in TargetPath. Re-stamp them to the new instance (as in OnPasteNode).
		const FGuid NewGuid = NewRuntimeNode->GetBindingID();
		for (FScriptablePropertyBinding& Binding : NewRuntimeNode->GetPropertyBindings().Bindings)
		{
			Binding.TargetPath.SetStructID(NewGuid);
		}

		if (UScriptableNode_Task* TaskWrapper = Cast<UScriptableNode_Task>(NewRuntimeNode))
		{
			if (UScriptableTask* InnerTask = TaskWrapper->Task)
			{
				const FGuid InnerNewGuid = InnerTask->GetBindingID();
				for (FScriptablePropertyBinding& Binding : InnerTask->GetPropertyBindings().Bindings)
				{
					Binding.TargetPath.SetStructID(InnerNewGuid);
				}
			}
		}

		GraphAsset->Nodes.Add(NewRuntimeNode);

		// Position: use the header offset if available, otherwise fall back to cascade.
		FVector2f Location;
		if (OldIDsInOrder.IsValidIndex(i))
		{
			const FGuid OldID = OldIDsInOrder[i];
			OldIDToNewRuntime.Add(OldID, NewRuntimeNode);

			if (const FVector2f* Offset = OldNodeOffsets.Find(OldID))
			{
				Location = BasePasteLocation + (*Offset);
			}
			else
			{
				Location = BasePasteLocation + CascadeStep * static_cast<float>(i);
			}
		}
		else
		{
			Location = BasePasteLocation + CascadeStep * static_cast<float>(i);
		}

		UScriptableEdGraphNode* NewEdNode = ScriptableGraphEditorHelpers::SpawnEdNodeForRuntime(EdGraph, NewRuntimeNode, Location);

		if (NewEdNode)
		{
			GraphEditorWidget->SetNodeSelection(NewEdNode, true);
		}
	}

	// Recreate connections between freshly pasted nodes. Connections whose endpoints aren't both
	// in OldIDToNewRuntime (e.g. one side was an Entry we stripped) are silently dropped.
	for (const FCopiedConnectionEntry& Entry : CopiedConnections)
	{
		UScriptableNode** NewFromRuntime = OldIDToNewRuntime.Find(Entry.OldFromNodeID);
		UScriptableNode** NewToRuntime = OldIDToNewRuntime.Find(Entry.OldToNodeID);
		if (!NewFromRuntime || !NewToRuntime) continue;

		// Persist in the asset.
		FScriptableGraphConnection NewConn;
		NewConn.From.NodeID = (*NewFromRuntime)->GetBindingID();
		NewConn.From.PinName = Entry.FromPinName;
		NewConn.To.NodeID = (*NewToRuntime)->GetBindingID();
		NewConn.To.PinName = Entry.ToPinName;
		GraphAsset->Connections.Add(NewConn);

		// Mirror on the visual graph: find the ed-nodes wrapping these runtimes and link their pins.
		UScriptableEdGraphNode* FromEdNode = nullptr;
		UScriptableEdGraphNode* ToEdNode = nullptr;
		for (UEdGraphNode* EdNode : EdGraph->Nodes)
		{
			if (UScriptableEdGraphNode* SfEd = Cast<UScriptableEdGraphNode>(EdNode))
			{
				if (SfEd->GetRuntimeNode() == *NewFromRuntime) FromEdNode = SfEd;
				else if (SfEd->GetRuntimeNode() == *NewToRuntime) ToEdNode = SfEd;
				if (FromEdNode && ToEdNode) break;
			}
		}

		if (FromEdNode && ToEdNode)
		{
			UEdGraphPin* FromPin = FromEdNode->FindPin(Entry.FromPinName, EGPD_Output);
			UEdGraphPin* ToPin = ToEdNode->FindPin(Entry.ToPinName, EGPD_Input);
			if (FromPin && ToPin)
			{
				FromPin->MakeLinkTo(ToPin);
			}
		}
	}

	GraphEditorWidget->NotifyGraphChanged();
}

void FScriptableGraphEditor::OnDuplicateSelected()
{
	// Duplicate = Copy + Paste, single transaction.
	const FScopedTransaction Transaction(LOCTEXT("DuplicateNodes", "Duplicate Nodes"));
	OnCopySelected();
	OnPasteAtCursor();
}

void FScriptableGraphEditor::OnSelectAllNodes()
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->SelectAllNodes();
	}
}

void FScriptableGraphEditor::OnCreateComment()
{
	if (!GraphEditorWidget.IsValid()) return;

	UEdGraph* Graph = GraphEditorWidget->GetCurrentGraph();
	if (!Graph) return;

	// If nodes are selected, size the comment to wrap them; otherwise place it at the cursor with a default size.
	FSlateRect BoundsRect;
	const bool bSelectionHasBounds = GraphEditorWidget->GetBoundsForSelectedNodes(BoundsRect, /*Padding*/ 50.0f);

	const FScopedTransaction Transaction(LOCTEXT("CreateCommentTx", "Create Comment"));
	Graph->Modify();

	UEdGraphNode_Comment* CommentNode = NewObject<UEdGraphNode_Comment>(Graph);
	CommentNode->SetFlags(RF_Transactional);
	Graph->AddNode(CommentNode, /*bFromUI*/ true, /*bSelectNewNode*/ true);
	CommentNode->CreateNewGuid();
	CommentNode->PostPlacedNewNode();
	CommentNode->AllocateDefaultPins();

	if (bSelectionHasBounds)
	{
		// Wrap the selection: place top-left at the selection's bounds and size accordingly.
		CommentNode->NodePosX = BoundsRect.Left;
		CommentNode->NodePosY = BoundsRect.Top;
		CommentNode->NodeWidth = BoundsRect.Right - BoundsRect.Left;
		CommentNode->NodeHeight = BoundsRect.Bottom - BoundsRect.Top;
	}
	else
	{
		// Default placement at the paste location with stock dimensions matching BP's convention.
		const FVector2f PasteLocation = GraphEditorWidget->GetPasteLocation2f();
		CommentNode->NodePosX = PasteLocation.X;
		CommentNode->NodePosY = PasteLocation.Y;
		CommentNode->NodeWidth = 400;
		CommentNode->NodeHeight = 100;
	}

	// Select the new comment so the user can start typing the title immediately.
	GraphEditorWidget->ClearSelectionSet();
	GraphEditorWidget->SetNodeSelection(CommentNode, true);

	Graph->NotifyGraphChanged();
}

namespace
{
	/** Returns the node's rendered size when its slate widget is available; falls back to NodeWidth/NodeHeight (≈ 0 for non-comment nodes). */
	FVector2f GetNodeRenderedSize(const TSharedPtr<SGraphEditor>& GraphEditor, const UEdGraphNode* Node)
	{
		if (!Node) return FVector2f::ZeroVector;
		if (GraphEditor.IsValid())
		{
			if (SGraphPanel* Panel = GraphEditor->GetGraphPanel())
			{
				if (TSharedPtr<SGraphNode> Widget = Panel->GetNodeWidgetFromGuid(Node->NodeGuid))
				{
					return Widget->GetDesiredSize();
				}
			}
		}
		return FVector2f(static_cast<float>(Node->NodeWidth), static_cast<float>(Node->NodeHeight));
	}

	TArray<UEdGraphNode*> CollectSelectedNodes(const TSharedPtr<SGraphEditor>& GraphEditor)
	{
		TArray<UEdGraphNode*> Nodes;
		if (!GraphEditor.IsValid()) return Nodes;
		const FGraphPanelSelectionSet Selection = GraphEditor->GetSelectedNodes();
		for (UObject* Obj : Selection)
		{
			if (UEdGraphNode* Node = Cast<UEdGraphNode>(Obj)) Nodes.Add(Node);
		}
		return Nodes;
	}
}

void FScriptableGraphEditor::AlignNodesAxis(EScriptableAlignMode Mode, bool bVertical)
{
	const TArray<UEdGraphNode*> Nodes = CollectSelectedNodes(GraphEditorWidget);
	if (Nodes.Num() < 2) return;

	/** Compute selection bounds along the chosen axis. */
	float Min = TNumericLimits<float>::Max();
	float Max = TNumericLimits<float>::Lowest();
	for (UEdGraphNode* Node : Nodes)
	{
		const FVector2f Size = GetNodeRenderedSize(GraphEditorWidget, Node);
		const float NodeMin = bVertical ? static_cast<float>(Node->NodePosY) : static_cast<float>(Node->NodePosX);
		const float NodeMax = NodeMin + (bVertical ? Size.Y : Size.X);
		Min = FMath::Min(Min, NodeMin);
		Max = FMath::Max(Max, NodeMax);
	}

	const FScopedTransaction Transaction(LOCTEXT("AlignNodesTx", "Align Nodes"));
	for (UEdGraphNode* Node : Nodes)
	{
		const FVector2f Size = GetNodeRenderedSize(GraphEditorWidget, Node);
		const float NodeSize = bVertical ? Size.Y : Size.X;

		float NewPos = 0.f;
		switch (Mode)
		{
			case EScriptableAlignMode::Min:    NewPos = Min;                            break;
			case EScriptableAlignMode::Max:    NewPos = Max - NodeSize;                 break;
			case EScriptableAlignMode::Center: NewPos = (Min + Max) * 0.5f - NodeSize * 0.5f; break;
		}

		Node->Modify();
		if (bVertical) Node->NodePosY = FMath::RoundToInt(NewPos);
		else           Node->NodePosX = FMath::RoundToInt(NewPos);
	}

	if (UEdGraph* OwningGraph = Nodes[0]->GetGraph()) OwningGraph->NotifyGraphChanged();
}

void FScriptableGraphEditor::DistributeNodesAxis(bool bVertical)
{
	TArray<UEdGraphNode*> Nodes = CollectSelectedNodes(GraphEditorWidget);
	if (Nodes.Num() < 3) return; // distribute needs three: two anchors plus at least one to space.

	Nodes.Sort([bVertical](const UEdGraphNode& A, const UEdGraphNode& B)
	{
		return bVertical ? (A.NodePosY < B.NodePosY) : (A.NodePosX < B.NodePosX);
	});

	const float First = bVertical ? static_cast<float>(Nodes[0]->NodePosY) : static_cast<float>(Nodes[0]->NodePosX);
	const float Last  = bVertical ? static_cast<float>(Nodes.Last()->NodePosY) : static_cast<float>(Nodes.Last()->NodePosX);
	const float Step  = (Last - First) / static_cast<float>(Nodes.Num() - 1);

	const FScopedTransaction Transaction(LOCTEXT("DistributeNodesTx", "Distribute Nodes"));
	for (int32 i = 1; i < Nodes.Num() - 1; ++i)
	{
		Nodes[i]->Modify();
		const int32 NewPos = FMath::RoundToInt(First + Step * static_cast<float>(i));
		if (bVertical) Nodes[i]->NodePosY = NewPos;
		else           Nodes[i]->NodePosX = NewPos;
	}

	if (UEdGraph* OwningGraph = Nodes[0]->GetGraph()) OwningGraph->NotifyGraphChanged();
}

void FScriptableGraphEditor::OnZoomToSelection()
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->ZoomToFit(/*bOnlyTargetSelection*/ true);
	}
}

void FScriptableGraphEditor::OnToggleBreakpoint()
{
	UScriptableGraph* GraphAsset = EditedGraph.Get();
	if (!GraphAsset || !GraphEditorWidget.IsValid()) return;

	const FGraphPanelSelectionSet Selection = GraphEditorWidget->GetSelectedNodes();
	if (Selection.IsEmpty()) return;

	const FScopedTransaction Transaction(LOCTEXT("ToggleBreakpointTx", "Toggle Breakpoint"));
	GraphAsset->Modify();

	bool bChanged = false;
	for (UObject* Selected : Selection)
	{
		const UScriptableEdGraphNode* SfNode = Cast<UScriptableEdGraphNode>(Selected);
		if (!SfNode || !SfNode->GetRuntimeNode()) continue;
		const FGuid Id = SfNode->GetRuntimeNode()->GetBindingID();
		if (GraphAsset->Breakpoints.Contains(Id)) GraphAsset->Breakpoints.Remove(Id);
		else GraphAsset->Breakpoints.Add(Id, true);
		bChanged = true;
	}

	if (bChanged && GraphAsset->EdGraph)
	{
		GraphAsset->EdGraph->NotifyGraphChanged();
	}
}

FVector2f FScriptableGraphEditor::GetCursorGraphPosition() const
{
	if (!GraphEditorWidget.IsValid()) return FVector2f::ZeroVector;

	// Convert the live screen-space cursor into the graph panel's coordinate system. This is what
	// makes the BP-style shortcut feel right: pressing S drops the node where the mouse currently
	// hovers, not where the last context menu opened.
	if (SGraphPanel* Panel = GraphEditorWidget->GetGraphPanel())
	{
		const FGeometry& PanelGeo = Panel->GetTickSpaceGeometry();
		if (PanelGeo.GetLocalSize().X > 0.0f && PanelGeo.GetLocalSize().Y > 0.0f)
		{
			const FVector2f ScreenCursor = FSlateApplication::Get().GetCursorPos();
			const FVector2f LocalCursor = PanelGeo.AbsoluteToLocal(ScreenCursor);
			return FVector2f(Panel->PanelCoordToGraphCoord(LocalCursor));
		}
	}

	return GraphEditorWidget->GetPasteLocation2f();
}

void FScriptableGraphEditor::OnSpawnNativeNodeAtCursor(UClass* RuntimeNodeClass)
{
	if (!GraphEditorWidget.IsValid() || !RuntimeNodeClass) return;

	UEdGraph* Graph = GraphEditorWidget->GetCurrentGraph();
	if (!Graph) return;

	const FVector2f Location = GetCursorGraphPosition();

	UEdGraphNode* Spawned = ScriptableGraphEditorHelpers::SpawnNativeNode(Graph, RuntimeNodeClass, Location, /*FromPin*/ nullptr, /*bSelectNewNode*/ true);
	if (Spawned)
	{
		GraphEditorWidget->ClearSelectionSet();
		GraphEditorWidget->SetNodeSelection(Spawned, true);
	}
}

void FScriptableGraphEditor::OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged)
{
	if (!NodeBeingChanged) return;

	// Persist the edited title (e.g. a comment box's text). OnRenameNode is the node's hook for this.
	const FScopedTransaction Transaction(LOCTEXT("RenameNodeTx", "Rename Node"));
	NodeBeingChanged->Modify();
	NodeBeingChanged->OnRenameNode(NewText.ToString());

	/** OnRenameNode bypasses both NotifyGraphChanged and FCoreUObjectDelegates::OnObjectPropertyChanged. */
	MarkDirtySinceLastCompile();
}

void FScriptableGraphEditor::OnNodeDoubleClicked(UEdGraphNode* Node)
{
	// Resolve the asset this node points to: a Run Graph -> its graph, a Run Asset -> its action asset,
	// or a Blueprint-authored task -> its Blueprint. Native tasks resolve to nothing (no-op).
	UObject* AssetToOpen = nullptr;

	if (const UScriptableEdGraphNode* EdNode = Cast<UScriptableEdGraphNode>(Node))
	{
		if (const UScriptableNode_SubGraph* SubGraphNode = Cast<UScriptableNode_SubGraph>(EdNode->GetRuntimeNode()))
		{
			AssetToOpen = SubGraphNode->SubGraphAsset;
		}
		else if (const UScriptableNode_Task* TaskNode = Cast<UScriptableNode_Task>(EdNode->GetRuntimeNode()))
		{
			if (UScriptableTask* Task = TaskNode->Task)
			{
				if (const UScriptableTask_RunGraph* RunGraph = Cast<UScriptableTask_RunGraph>(Task))
				{
					AssetToOpen = RunGraph->GraphAsset;
				}
				else if (const UScriptableTask_RunAsset* RunAsset = Cast<UScriptableTask_RunAsset>(Task))
				{
					AssetToOpen = RunAsset->Asset;
				}
				else
				{
					AssetToOpen = Task->GetClass()->ClassGeneratedBy; // The UBlueprint for a BP task, else null.
				}
			}
		}
	}

	if (AssetToOpen && GEditor)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(AssetToOpen);
	}
}

void FScriptableGraphEditor::OnRenameSelectedNode()
{
	if (!GraphEditorWidget.IsValid()) return;

	for (UObject* Obj : GraphEditorWidget->GetSelectedNodes())
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(Obj);
		if (Node && Node->GetCanRenameNode())
		{
			// Requesting title visibility with bRequestRename triggers the node's inline edit field.
			GraphEditorWidget->IsNodeTitleVisible(Node, /*bRequestRename*/ true);
			break;
		}
	}
}

bool FScriptableGraphEditor::CanRenameSelectedNode() const
{
	if (!GraphEditorWidget.IsValid()) return false;

	const FGraphPanelSelectionSet Selected = GraphEditorWidget->GetSelectedNodes();
	if (Selected.Num() != 1) return false;

	for (UObject* Obj : Selected)
	{
		const UEdGraphNode* Node = Cast<UEdGraphNode>(Obj);
		if (Node && Node->GetCanRenameNode()) return true;
	}
	return false;
}

void FScriptableGraphEditor::OnRemoveSequencePin()
{
	if (!GraphEditorWidget.IsValid()) return;

	UEdGraphPin* Pin = GraphEditorWidget->GetGraphPinForMenu();
	if (!Pin || Pin->Direction != EGPD_Output) return;

	UScriptableEdGraphNode* SfEdNode = Cast<UScriptableEdGraphNode>(Pin->GetOwningNode());
	if (!SfEdNode) return;

	UScriptableNode_Sequence* Sequence = Cast<UScriptableNode_Sequence>(SfEdNode->GetRuntimeNode());
	if (!Sequence) return;

	// Parse the branch index from the pin name (pure numeric per MakeOutputName).
	const FString PinNameStr = Pin->PinName.ToString();
	int32 DigitStart = PinNameStr.Len();
	while (DigitStart > 0 && FChar::IsDigit(PinNameStr[DigitStart - 1]))
	{
		--DigitStart;
	}
	if (DigitStart == PinNameStr.Len()) return;

	const int32 BranchIndex = FCString::Atoi(*PinNameStr.Mid(DigitStart));
	if (BranchIndex < 0 || BranchIndex >= Sequence->OutputCount) return;

	const FScopedTransaction Transaction(LOCTEXT("RemoveSequencePinTx", "Remove Sequence Pin"));
	Sequence->RemoveOutputPinAt(BranchIndex);
	ReconstructEdGraphFromAsset();
}

bool FScriptableGraphEditor::CanRemoveSequencePin() const
{
	if (!GraphEditorWidget.IsValid()) return false;

	UEdGraphPin* Pin = GraphEditorWidget->GetGraphPinForMenu();
	if (!Pin || Pin->Direction != EGPD_Output) return false;

	UScriptableEdGraphNode* SfEdNode = Cast<UScriptableEdGraphNode>(Pin->GetOwningNode());
	if (!SfEdNode) return false;

	const UScriptableNode_Sequence* Sequence = Cast<UScriptableNode_Sequence>(SfEdNode->GetRuntimeNode());
	if (!Sequence) return false;

	return Sequence->OutputCount > UScriptableNode_Sequence::MinOutputCount;
}

void FScriptableGraphEditor::OnRemoveANDPin()
{
	if (!GraphEditorWidget.IsValid()) return;

	UEdGraphPin* Pin = GraphEditorWidget->GetGraphPinForMenu();
	if (!Pin || Pin->Direction != EGPD_Input) return;

	UScriptableEdGraphNode* SfEdNode = Cast<UScriptableEdGraphNode>(Pin->GetOwningNode());
	if (!SfEdNode) return;

	UScriptableNode_AND* AND = Cast<UScriptableNode_AND>(SfEdNode->GetRuntimeNode());
	if (!AND) return;

	// Parse the branch index from the pin name (pure numeric per MakeInputName).
	const FString PinNameStr = Pin->PinName.ToString();
	int32 DigitStart = PinNameStr.Len();
	while (DigitStart > 0 && FChar::IsDigit(PinNameStr[DigitStart - 1]))
	{
		--DigitStart;
	}
	if (DigitStart == PinNameStr.Len()) return;

	const int32 BranchIndex = FCString::Atoi(*PinNameStr.Mid(DigitStart));
	if (BranchIndex < 0 || BranchIndex >= AND->InputCount) return;

	const FScopedTransaction Transaction(LOCTEXT("RemoveANDPinTx", "Remove AND Pin"));
	AND->RemoveInputPinAt(BranchIndex);
	ReconstructEdGraphFromAsset();
}

bool FScriptableGraphEditor::CanRemoveANDPin() const
{
	if (!GraphEditorWidget.IsValid()) return false;

	UEdGraphPin* Pin = GraphEditorWidget->GetGraphPinForMenu();
	if (!Pin || Pin->Direction != EGPD_Input) return false;

	UScriptableEdGraphNode* SfEdNode = Cast<UScriptableEdGraphNode>(Pin->GetOwningNode());
	if (!SfEdNode) return false;

	const UScriptableNode_AND* AND = Cast<UScriptableNode_AND>(SfEdNode->GetRuntimeNode());
	if (!AND) return false;

	return AND->InputCount > UScriptableNode_AND::MinInputCount;
}

void FScriptableGraphEditor::OnRemoveORPin()
{
	if (!GraphEditorWidget.IsValid()) return;

	UEdGraphPin* Pin = GraphEditorWidget->GetGraphPinForMenu();
	if (!Pin || Pin->Direction != EGPD_Input) return;

	UScriptableEdGraphNode* SfEdNode = Cast<UScriptableEdGraphNode>(Pin->GetOwningNode());
	if (!SfEdNode) return;

	UScriptableNode_OR* OR = Cast<UScriptableNode_OR>(SfEdNode->GetRuntimeNode());
	if (!OR) return;

	// Parse the branch index from the pin name (pure numeric per MakeInputName).
	const FString PinNameStr = Pin->PinName.ToString();
	int32 DigitStart = PinNameStr.Len();
	while (DigitStart > 0 && FChar::IsDigit(PinNameStr[DigitStart - 1]))
	{
		--DigitStart;
	}
	if (DigitStart == PinNameStr.Len()) return;

	const int32 BranchIndex = FCString::Atoi(*PinNameStr.Mid(DigitStart));
	if (BranchIndex < 0 || BranchIndex >= OR->InputCount) return;

	const FScopedTransaction Transaction(LOCTEXT("RemoveORPinTx", "Remove OR Pin"));
	OR->RemoveInputPinAt(BranchIndex);
	ReconstructEdGraphFromAsset();
}

bool FScriptableGraphEditor::CanRemoveORPin() const
{
	if (!GraphEditorWidget.IsValid()) return false;

	UEdGraphPin* Pin = GraphEditorWidget->GetGraphPinForMenu();
	if (!Pin || Pin->Direction != EGPD_Input) return false;

	UScriptableEdGraphNode* SfEdNode = Cast<UScriptableEdGraphNode>(Pin->GetOwningNode());
	if (!SfEdNode) return false;

	const UScriptableNode_OR* OR = Cast<UScriptableNode_OR>(SfEdNode->GetRuntimeNode());
	if (!OR) return false;

	return OR->InputCount > UScriptableNode_OR::MinInputCount;
}

void FScriptableGraphEditor::OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event)
{
	// UE fires this for every transacted object globally; filter to our asset and to UndoRedo events.
	if (!Object) return;
	if (Object != EditedGraph.Get()) return;
	if (Event.GetEventType() != ETransactionObjectEventType::UndoRedo) return;

	// Rebuild the ed-graph from the asset's now-restored Nodes + Connections, wiping any stale state.
	ReconstructEdGraphFromAsset();
}

void FScriptableGraphEditor::OnRuntimeNodePropertyChanged(UObject* InObject, FPropertyChangedEvent& InEvent)
{
	UScriptableGraph* Graph = EditedGraph.Get();
	if (!Graph || !Graph->EdGraph || !InObject) return;

	// Graph.Outputs edits ripple to Exit ed-nodes (their pin set depends on it).
	if (InObject == Graph)
	{
		MarkDirtySinceLastCompile();

		const FName PropertyName = InEvent.Property ? InEvent.Property->GetFName() : NAME_None;
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UScriptableGraph, Outputs))
		{
			for (UEdGraphNode* EdNode : Graph->EdGraph->Nodes)
			{
				UScriptableEdGraphNode* SfEd = Cast<UScriptableEdGraphNode>(EdNode);
				if (!SfEd) continue;
				const UScriptableNode* Runtime = SfEd->GetRuntimeNode();
				if (Runtime && Runtime->IsA<UScriptableNode_Exit>())
				{
					SfEd->ReconstructNode();
				}
			}
		}
		return;
	}

	// Walk up the outer chain of the changed object until we find a UScriptableNode owned by the
	// edited graph. This catches both direct edits on the wrapper and edits on the inner task
	// (whose outer is the wrapper, whose outer is the graph asset).
	for (UObject* Cursor = InObject; Cursor; Cursor = Cursor->GetOuter())
	{
		UScriptableNode* Node = Cast<UScriptableNode>(Cursor);
		if (!Node || Node->GetOuter() != Graph) continue;

		MarkDirtySinceLastCompile();

		for (UEdGraphNode* EdNode : Graph->EdGraph->Nodes)
		{
			UScriptableEdGraphNode* SfEd = Cast<UScriptableEdGraphNode>(EdNode);
			if (SfEd && SfEd->GetRuntimeNode() == Node)
			{
				SfEd->ReconstructNode();
				return;
			}
		}
		return;
	}

	/** Cross-asset reactivity: an edit in a foreign UScriptableGraph that we embed via UScriptableNode_SubGraph
	 * must rebuild the embedding ed-nodes so their event-input / declared-output pins stay in sync. We filter to
	 * the properties that actually shape pins (Outputs/Nodes on the sub-asset, EventName on its ReceiveEvents). */
	UScriptableGraph* ForeignGraph = nullptr;
	bool bRelevantChange = false;
	const FName PropertyName = InEvent.Property ? InEvent.Property->GetFName() : NAME_None;

	if (UScriptableGraph* DirectChange = Cast<UScriptableGraph>(InObject))
	{
		if (DirectChange != Graph)
		{
			ForeignGraph = DirectChange;
			bRelevantChange = (PropertyName == GET_MEMBER_NAME_CHECKED(UScriptableGraph, Outputs)) ||
				(PropertyName == GET_MEMBER_NAME_CHECKED(UScriptableGraph, Nodes));
		}
	}
	else if (UScriptableNode_ReceiveEvent* EventNode = Cast<UScriptableNode_ReceiveEvent>(InObject))
	{
		UScriptableGraph* EventOwner = EventNode->GetTypedOuter<UScriptableGraph>();
		if (EventOwner && EventOwner != Graph)
		{
			ForeignGraph = EventOwner;
			bRelevantChange = (PropertyName == GET_MEMBER_NAME_CHECKED(UScriptableNode_ReceiveEvent, EventName));
		}
	}

	if (!bRelevantChange || !ForeignGraph) return;

	for (UEdGraphNode* EdNode : Graph->EdGraph->Nodes)
	{
		UScriptableEdGraphNode* SfEd = Cast<UScriptableEdGraphNode>(EdNode);
		if (!SfEd) continue;
		const UScriptableNode_SubGraph* SubGraphNode = Cast<UScriptableNode_SubGraph>(SfEd->GetRuntimeNode());
		if (!SubGraphNode || SubGraphNode->SubGraphAsset != ForeignGraph) continue;
		SfEd->ReconstructNode();
	}
}

void FScriptableGraphEditor::OnPackageSaved(const FString& PackageFileName, UPackage* SavedPackage, FObjectPostSaveContext SaveContext)
{
	UScriptableGraph* Graph = EditedGraph.Get();
	if (!Graph || !Graph->EdGraph || !SavedPackage) return;

	/** Own package: live property-change handler already reacts; skip to avoid a redundant reconstruct pass. */
	if (SavedPackage == Graph->GetPackage()) return;

	for (UEdGraphNode* EdNode : Graph->EdGraph->Nodes)
	{
		UScriptableEdGraphNode* SfEd = Cast<UScriptableEdGraphNode>(EdNode);
		if (!SfEd) continue;
		const UScriptableNode_SubGraph* SubGraphNode = Cast<UScriptableNode_SubGraph>(SfEd->GetRuntimeNode());
		if (!SubGraphNode || !SubGraphNode->SubGraphAsset) continue;
		if (SubGraphNode->SubGraphAsset->GetPackage() != SavedPackage) continue;
		SfEd->ReconstructNode();
	}
}

void FScriptableGraphEditor::OnGraphSelectionChanged(const FGraphPanelSelectionSet& NewSelection)
{
	/** Refresh the connection drawing policy's input: it brightens/thickens wires touching any selected node. */
	if (UScriptableGraph* Graph = EditedGraph.Get())
	{
		if (UScriptableEdGraph* SfEdGraph = Cast<UScriptableEdGraph>(Graph->EdGraph))
		{
			SfEdGraph->SelectedNodes.Reset();
			for (UObject* Selected : NewSelection)
			{
				if (const UEdGraphNode* Node = Cast<UEdGraphNode>(Selected))
				{
					SfEdGraph->SelectedNodes.Add(Node);
				}
			}
		}
	}

	if (!NodeDetailsView.IsValid()) return;

	// Single-selection only: anything else (empty or multi) clears the panel.
	if (NewSelection.Num() != 1)
	{
		NodeDetailsView->SetObject(nullptr);
		return;
	}

	UObject* Selected = *NewSelection.CreateConstIterator();

	if (UScriptableEdGraphNode* SfEdNode = Cast<UScriptableEdGraphNode>(Selected))
	{
		UScriptableNode* RuntimeNode = SfEdNode->GetRuntimeNode();
		NodeDetailsView->SetObject(RuntimeNode);
		return;
	}

	if (Selected && Selected->IsA<UEdGraphNode>())
	{
		NodeDetailsView->SetObject(Selected);
		return;
	}

	NodeDetailsView->SetObject(nullptr);
}

TSharedRef<SDockTab> FScriptableGraphEditor::SpawnTab_Validation(const FSpawnTabArgs& Args)
{
	SAssignNew(ValidationPanel, SKzValidationPanel)
		.OnIssueActivated(SKzValidationPanel::FOnIssueActivated::CreateSP(this, &FScriptableGraphEditor::HandleValidationIssueActivated))
		.OnRunValidation(SKzValidationPanel::FOnRunValidation::CreateSP(this, &FScriptableGraphEditor::HandleRunValidation));

	/** Dynamic label so the tab shows the unfiltered issue count as a BP-style badge ("Validation (3)"). */
	TAttribute<FText> TabLabel = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda([this]()
		{
			if (!ValidationPanel.IsValid()) return LOCTEXT("ValidationTab", "Validation");
			const int32 Count = ValidationPanel->GetTotalIssueCount();
			if (Count <= 0) return LOCTEXT("ValidationTab", "Validation");
			return FText::Format(LOCTEXT("ValidationTabWithCount", "Validation ({0})"), FText::AsNumber(Count));
		}));

	return SNew(SDockTab)
		.Label(TabLabel)
		[
			ValidationPanel.ToSharedRef()
		];
}

void FScriptableGraphEditor::ExtendToolbar()
{
	TSharedPtr<FExtender> Extender = MakeShared<FExtender>();
	Extender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateLambda([this](FToolBarBuilder& ToolbarBuilder)
			{
				ToolbarBuilder.BeginSection("Compile");
				{
					ToolbarBuilder.AddToolBarButton(
						FUIAction(FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnCompile)),
						NAME_None,
						LOCTEXT("CompileBtn", "Compile"),
						TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &FScriptableGraphEditor::GetCompileButtonTooltip)),
						TAttribute<FSlateIcon>::Create(TAttribute<FSlateIcon>::FGetter::CreateSP(this, &FScriptableGraphEditor::GetCompileButtonIcon)));

					/** Compact dropdown chevron next to Compile, mirroring the BP toolbar combo. */
					ToolbarBuilder.AddComboButton(
						FUIAction(),
						FOnGetContent::CreateSP(this, &FScriptableGraphEditor::GenerateCompileOptionsMenu),
						LOCTEXT("CompileOptions_Label", ""),
						LOCTEXT("CompileOptions_Tooltip", "Compile options"),
						TAttribute<FSlateIcon>(),
						/*bInSimpleComboBox=*/ true);
				}
				ToolbarBuilder.EndSection();

				ToolbarBuilder.BeginSection("GraphTools");
				{
					ToolbarBuilder.AddToolBarButton(
						FUIAction(FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnGoHome)),
						NAME_None,
						LOCTEXT("HomeBtn", "Home"),
						LOCTEXT("HomeBtnTip", "Center the view on the Entry node"),
						FSlateIcon(FAppStyle::GetAppStyleSetName(), "MaterialEditor.CameraHome"));

					ToolbarBuilder.AddToolBarButton(
						FUIAction(FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnCleanGraph)),
						NAME_None,
						LOCTEXT("CleanBtn", "Clean Graph"),
						LOCTEXT("CleanBtnTip", "Delete every node unreachable from Entry or any ReceiveEvent"),
						FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Clean"));

					ToolbarBuilder.AddToolBarButton(
						FUIAction(FExecuteAction::CreateSP(this, &FScriptableGraphEditor::OnOpenSearch)),
						NAME_None,
						LOCTEXT("SearchBtn", "Search"),
						LOCTEXT("SearchBtnTip", "Open the node search tab"),
						FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Search"));
				}
				ToolbarBuilder.EndSection();

				/**
				 * Debug Object selector at the far right, BP-style. AddComboButton would force the
				 * icon-above-label toolbar block; we want a flat horizontal "text + chevron" combo,
				 * matching the BP debug object widget, so we add a hand-built SComboButton via AddWidget.
				 */
				ToolbarBuilder.BeginSection("DebugObject");
				{
					ToolbarBuilder.AddWidget(
						SNew(SBox)
							.WidthOverride(200.f)
							.VAlign(VAlign_Center)
							.Padding(FMargin(4.f, 0.f))
							[
								SNew(SComboButton)
									.OnGetMenuContent(this, &FScriptableGraphEditor::BuildDebugObjectMenu)
									.ToolTipText(LOCTEXT("DebugObjectTip", "Live runner whose active nodes are highlighted on the canvas."))
									.ContentPadding(FMargin(4.f, 2.f))
									.ButtonContent()
									[
										SNew(STextBlock)
											.Text_Lambda([this]() { return GetDebugObjectLabel(); })
											.Font(FAppStyle::Get().GetFontStyle("NormalFont"))
									]
							]);
				}
				ToolbarBuilder.EndSection();
			}));

	AddToolbarExtender(Extender);
}

void FScriptableGraphEditor::OnCompile()
{
	if (const TSharedPtr<FTabManager> TabManagerPin = GetTabManager())
	{
		TabManagerPin->TryInvokeTab(ValidationTabId);
	}
	RunCompile(/*bMarkPackageDirty*/ true);
}

void FScriptableGraphEditor::RunCompile(bool bMarkPackageDirty)
{
	UScriptableGraph* Graph = EditedGraph.Get();
	if (!Graph) return;

	// The graph validators already drop issues on unreachable nodes (BP behaviour).
	const TArray<FKzValidationIssue> Issues = FKzAssetValidationUtils::RunValidation(Graph);

	const bool bHasError = Issues.ContainsByPredicate(
		[](const FKzValidationIssue& Issue) { return Issue.Severity == EKzValidationSeverity::Error; });

	/** Persist only when the result actually flipped, to avoid spurious package-dirty bumps on a clean re-compile. */
	if (Graph->bLastCompileFailed != bHasError)
	{
		if (bMarkPackageDirty) Graph->Modify();
		Graph->bLastCompileFailed = bHasError;
		if (bMarkPackageDirty) Graph->MarkPackageDirty();
	}

	if (ValidationPanel.IsValid())
	{
		ValidationPanel->SetIssues(Issues);
	}

	ApplyValidationToErrorBanners(Issues);

	/** Compile is the only event that clears dirty. The status icon polls the flag every paint. */
	bIsDirtySinceLastCompile = false;

	/** Save / Jump options only apply on user-initiated compile (open-time pass is silent). */
	if (bMarkPackageDirty)
	{
		const UScriptableGraphEditorSettings* Settings = GetDefault<UScriptableGraphEditorSettings>();

		const bool bShouldSave =
			Settings->SaveOnCompile == EScriptableSaveOnCompile::Always ||
			(Settings->SaveOnCompile == EScriptableSaveOnCompile::OnSuccessOnly && !bHasError);

		if (bShouldSave)
		{
			if (UEditorAssetSubsystem* AssetSub = GEditor ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr)
			{
				AssetSub->SaveLoadedAsset(Graph, /*bOnlyIfIsDirty*/ false);
			}
		}

		if (bHasError && Settings->bJumpToErrorNode && GraphEditorWidget.IsValid() && Graph->EdGraph)
		{
			for (UEdGraphNode* EdNode : Graph->EdGraph->Nodes)
			{
				if (EdNode && EdNode->bHasCompilerMessage && EdNode->ErrorType == EMessageSeverity::Error)
				{
					GraphEditorWidget->JumpToNode(EdNode, /*bRequestRename*/ false, /*bSelectNode*/ true);
					break;
				}
			}
		}
	}
}

void FScriptableGraphEditor::MarkDirtySinceLastCompile()
{
	bIsDirtySinceLastCompile = true;
}

void FScriptableGraphEditor::HandleGraphChanged(const FEdGraphEditAction& Action)
{
	if (bSuppressDirtyOnGraphChange) return;

	/** Pure selection changes don't count as edits. Everything else (add/remove node, pins, connections) does. */
	if (Action.Action == GRAPHACTION_SelectNode) return;
	MarkDirtySinceLastCompile();

	/** Re-validate on edits so banners flip live — if the user just fixed the last error, the red strip should
	 * vanish without waiting for an explicit Compile. ApplyValidationToErrorBanners guards its NotifyGraphChanged
	 * via bSuppressDirtyOnGraphChange so we don't recurse here. */
	RefreshErrorBanners();
}

void FScriptableGraphEditor::RefreshErrorBanners()
{
	UScriptableGraph* Graph = EditedGraph.Get();
	if (!Graph) return;

	ApplyValidationToErrorBanners(FKzAssetValidationUtils::RunValidation(Graph));
}

void FScriptableGraphEditor::ApplyValidationToErrorBanners(const TArray<FKzValidationIssue>& Issues)
{
	UScriptableGraph* Graph = EditedGraph.Get();
	if (!Graph || !Graph->EdGraph) return;

	/** Aggregate per node: highest severity (lowest enum value) wins, messages concatenate for the tooltip. */
	struct FNodeMarker
	{
		int32 Severity = EMessageSeverity::Info;
		FString Message;
	};
	TMap<FGuid, FNodeMarker> Markers;
	for (const FKzValidationIssue& Issue : Issues)
	{
		if (!Issue.ContextId.IsValid()) continue;

		const int32 EngineSeverity =
			Issue.Severity == EKzValidationSeverity::Error ? EMessageSeverity::Error :
			Issue.Severity == EKzValidationSeverity::Warning ? EMessageSeverity::Warning :
			EMessageSeverity::Info;

		FNodeMarker& Marker = Markers.FindOrAdd(Issue.ContextId);
		Marker.Severity = FMath::Min(Marker.Severity, EngineSeverity);
		if (!Marker.Message.IsEmpty()) Marker.Message += TEXT("\n");
		Marker.Message += Issue.Message.ToString();
	}

	for (UEdGraphNode* EdNode : Graph->EdGraph->Nodes)
	{
		UScriptableEdGraphNode* SfEd = Cast<UScriptableEdGraphNode>(EdNode);
		if (!SfEd) continue;

		const UScriptableNode* Runtime = SfEd->GetRuntimeNode();
		const FGuid BindingID = Runtime ? Runtime->GetBindingID() : FGuid();

		if (const FNodeMarker* Found = Markers.Find(BindingID))
		{
			SfEd->bHasCompilerMessage = true;
			SfEd->ErrorType = Found->Severity;
			/** Use the aggregated validator message so the banner's tooltip carries actionable detail instead of just "WARNING!" / "ERROR!". */
			SfEd->ErrorMsg = Found->Message;
		}
		else
		{
			SfEd->bHasCompilerMessage = false;
			SfEd->ErrorMsg.Reset();
		}
	}

	/** SGraphNode only re-reads ErrorMsg/ErrorType inside UpdateGraphNode; the cheapest way to retrigger that for every node is to broadcast OnGraphChanged. */
	TGuardValue<bool> Guard(bSuppressDirtyOnGraphChange, true);
	Graph->EdGraph->NotifyGraphChanged();
}

FSlateIcon FScriptableGraphEditor::GetCompileButtonIcon() const
{
	const FName StyleSet = FAppStyle::GetAppStyleSetName();
	const FName Base("Blueprint.CompileStatus.Background");

	if (bIsDirtySinceLastCompile)
	{
		return FSlateIcon(StyleSet, Base, NAME_None, "Blueprint.CompileStatus.Overlay.Unknown");
	}
	const UScriptableGraph* Graph = EditedGraph.Get();
	if (Graph && Graph->bLastCompileFailed)
	{
		return FSlateIcon(StyleSet, Base, NAME_None, "Blueprint.CompileStatus.Overlay.Error");
	}
	return FSlateIcon(StyleSet, Base, NAME_None, "Blueprint.CompileStatus.Overlay.Good");
}

TSharedRef<SWidget> FScriptableGraphEditor::GenerateCompileOptionsMenu()
{
	FMenuBuilder MenuBuilder(/*bShouldCloseAfter=*/ true, nullptr);

	MenuBuilder.AddSubMenu(
		LOCTEXT("SaveOnCompile", "Save on Compile"),
		LOCTEXT("SaveOnCompileTip", "Whether the asset is saved automatically after a Compile pass."),
		FNewMenuDelegate::CreateSP(this, &FScriptableGraphEditor::BuildSaveOnCompileMenu));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("JumpToErrorNode", "Jump to Error Node"),
		LOCTEXT("JumpToErrorNodeTip", "After a failed compile, pan to and select the first node carrying the error."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([]()
				{
					UScriptableGraphEditorSettings* Settings = GetMutableDefault<UScriptableGraphEditorSettings>();
					Settings->bJumpToErrorNode = !Settings->bJumpToErrorNode;
					Settings->SaveConfig();
				}),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([]()
				{
					return GetDefault<UScriptableGraphEditorSettings>()->bJumpToErrorNode;
				})),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);

	return MenuBuilder.MakeWidget();
}

void FScriptableGraphEditor::BuildSaveOnCompileMenu(FMenuBuilder& MenuBuilder)
{
	const EScriptableSaveOnCompile Levels[] =
	{
		EScriptableSaveOnCompile::Never,
		EScriptableSaveOnCompile::OnSuccessOnly,
		EScriptableSaveOnCompile::Always,
	};
	const FText Labels[] =
	{
		LOCTEXT("SaveNever", "Never"),
		LOCTEXT("SaveOnSuccess", "On Success Only"),
		LOCTEXT("SaveAlways", "Always"),
	};

	for (int32 i = 0; i < UE_ARRAY_COUNT(Levels); ++i)
	{
		const EScriptableSaveOnCompile Level = Levels[i];
		MenuBuilder.AddMenuEntry(
			Labels[i],
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([Level]()
					{
						UScriptableGraphEditorSettings* Settings = GetMutableDefault<UScriptableGraphEditorSettings>();
						Settings->SaveOnCompile = Level;
						Settings->SaveConfig();
					}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([Level]()
					{
						return GetDefault<UScriptableGraphEditorSettings>()->SaveOnCompile == Level;
					})),
			NAME_None,
			EUserInterfaceActionType::RadioButton);
	}
}

namespace
{
	/** "<Id> (<Owner>)" when both, else whichever is available, else the instance object name. Shared between toolbar label and combo entries. */
	FString FormatRunnerLabel(const UScriptableGraphInstance* Inst)
	{
		if (!Inst) return FString(TEXT("?"));
		const UObject* Owner = Inst->GetOwner();
		const FName Id = Inst->GetId();
		const bool bHasId = !Id.IsNone();
		const bool bHasOwner = Owner != nullptr;
		if (bHasId && bHasOwner) return FString::Printf(TEXT("%s (%s)"), *Id.ToString(), *Owner->GetName());
		if (bHasOwner)           return Owner->GetName();
		if (bHasId)              return Id.ToString();
		return Inst->GetName();
	}
}

FText FScriptableGraphEditor::GetDebugObjectLabel() const
{
	const UScriptableGraph* Graph = EditedGraph.Get();
	const UScriptableGraphInstance* Debug = FScriptableDebugRegistry::GetDebugInstance(Graph);
	if (!Debug || !Debug->IsRunning())
	{
		return LOCTEXT("NoDebugObject", "No debug object selected");
	}
	return FText::FromString(FormatRunnerLabel(Debug));
}

TSharedRef<SWidget> FScriptableGraphEditor::BuildDebugObjectMenu()
{
	FMenuBuilder MenuBuilder(/*bShouldCloseAfter*/ true, nullptr);

	UScriptableGraph* Graph = EditedGraph.Get();
	const UScriptableGraph* GraphConst = Graph;

	/** "No debug object selected" — clears the registry. Button (not Radio) so it doesn't show the bullet when current; the toolbar label already conveys that state. */
	MenuBuilder.AddMenuEntry(
		LOCTEXT("NoDebugObjectEntry", "No debug object selected"),
		FText::GetEmpty(),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([GraphConst]()
			{
				FScriptableDebugRegistry::SetDebugInstance(GraphConst, nullptr);
			})),
		NAME_None,
		EUserInterfaceActionType::Button);

	if (!Graph) return MenuBuilder.MakeWidget();

	/** Collect live runners of THIS asset across every world. Owner-name is the picker label. */
	TArray<TWeakObjectPtr<UScriptableGraphInstance>> Candidates;
	if (GEngine)
	{
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			UWorld* World = Ctx.World();
			if (!World) continue;
			UScriptableGraphSubsystem* Sub = World->GetSubsystem<UScriptableGraphSubsystem>();
			if (!Sub) continue;
			for (UScriptableGraphInstance* Inst : Sub->GetActiveRunners())
			{
				if (Inst && Inst->GetAsset() == Graph)
				{
					Candidates.Add(Inst);
				}
			}
		}
	}

	if (Candidates.IsEmpty()) return MenuBuilder.MakeWidget();

	MenuBuilder.AddSeparator();

	for (const TWeakObjectPtr<UScriptableGraphInstance>& WeakInst : Candidates)
	{
		UScriptableGraphInstance* Inst = WeakInst.Get();
		if (!Inst) continue;

		const FString Label = FormatRunnerLabel(Inst);

		MenuBuilder.AddMenuEntry(
			FText::FromString(Label),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([GraphConst, WeakInst]()
					{
						FScriptableDebugRegistry::SetDebugInstance(GraphConst, WeakInst.Get());
					}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([GraphConst, WeakInst]()
					{
						return FScriptableDebugRegistry::GetDebugInstance(GraphConst) == WeakInst.Get();
					})),
			NAME_None,
			EUserInterfaceActionType::RadioButton);
	}

	return MenuBuilder.MakeWidget();
}

FText FScriptableGraphEditor::GetCompileButtonTooltip() const
{
	if (bIsDirtySinceLastCompile)
	{
		return LOCTEXT("CompileBtnTip_Dirty", "Graph has uncompiled changes. Click to compile.");
	}
	const UScriptableGraph* Graph = EditedGraph.Get();
	if (Graph && Graph->bLastCompileFailed)
	{
		return LOCTEXT("CompileBtnTip_Failed", "Last compile failed. Click to recompile and see the issues.");
	}
	return LOCTEXT("CompileBtnTip_Good", "Graph is up to date. Click to recompile.");
}

void FScriptableGraphEditor::JumpToNode(UEdGraphNode* Node)
{
	if (Node && GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->JumpToNode(Node, /*bRequestRename*/ false, /*bSelectNode*/ true);
	}
}

TArray<FKzValidationIssue> FScriptableGraphEditor::HandleRunValidation()
{
	UScriptableGraph* Graph = EditedGraph.Get();
	if (!Graph) return {};

	// Does NOT persist bLastCompileFailed: that stays the user's last explicit Compile result.
	const TArray<FKzValidationIssue> Issues = FKzAssetValidationUtils::RunValidation(Graph);
	ApplyValidationToErrorBanners(Issues);

	/** Quick-fixes mutate state outside the details panel's UI; force a rebuild so cached row state catches up. */
	if (AssetDetailsView.IsValid()) AssetDetailsView->ForceRefresh();
	if (NodeDetailsView.IsValid())  NodeDetailsView->ForceRefresh();

	return Issues;
}

void FScriptableGraphEditor::HandleValidationIssueActivated(const FKzValidationIssue& Issue)
{
	if (!Issue.ContextId.IsValid() || !GraphEditorWidget.IsValid()) return;

	if (UEdGraphNode* EdNode = FindEdNodeByRuntimeId(Issue.ContextId))
	{
		GraphEditorWidget->JumpToNode(EdNode, /*bRequestRename*/ false, /*bSelectNode*/ true);
	}
}

UEdGraphNode* FScriptableGraphEditor::FindEdNodeByRuntimeId(const FGuid& RuntimeId) const
{
	UScriptableGraph* Graph = EditedGraph.Get();
	if (!Graph || !Graph->EdGraph || !RuntimeId.IsValid()) return nullptr;

	for (UEdGraphNode* EdNode : Graph->EdGraph->Nodes)
	{
		const UScriptableEdGraphNode* SfEdNode = Cast<UScriptableEdGraphNode>(EdNode);
		if (SfEdNode && SfEdNode->GetRuntimeNode() && SfEdNode->GetRuntimeNode()->GetBindingID() == RuntimeId)
		{
			return EdNode;
		}
	}
	return nullptr;
}

void FScriptableGraphEditor::OnGoHome()
{
	UScriptableGraph* Graph = EditedGraph.Get();
	if (!Graph || !GraphEditorWidget.IsValid()) return;

	if (UEdGraphNode* EntryEdNode = FindEdNodeByRuntimeId(Graph->EntryNodeID))
	{
		GraphEditorWidget->JumpToNode(EntryEdNode, /*bRequestRename*/ false, /*bSelectNode*/ false);
	}
}

void FScriptableGraphEditor::OnCleanGraph()
{
	UScriptableGraph* Graph = EditedGraph.Get();
	if (!Graph || !Graph->EdGraph || !GraphEditorWidget.IsValid()) return;

	// Reachability from Entry + every ReceiveEvent over forward connections (same rule as the validator).
	TMultiMap<FGuid, FGuid> Adjacency;
	for (const FScriptableGraphConnection& Conn : Graph->Connections)
	{
		Adjacency.Add(Conn.From.NodeID, Conn.To.NodeID);
	}

	TSet<FGuid> Reachable;
	TArray<FGuid> Queue;
	if (Graph->EntryNodeID.IsValid()) { Reachable.Add(Graph->EntryNodeID); Queue.Add(Graph->EntryNodeID); }
	for (const TObjectPtr<UScriptableNode>& Node : Graph->Nodes)
	{
		// ReceiveEvent: woken by FireEvent. Exit: fired directly by the runner at finish/cancel.
		// Seed both so they and their downstream sub-flows are kept by Clean Graph.
		if (Node && (Node->IsA<UScriptableNode_ReceiveEvent>() || Node->IsA<UScriptableNode_Exit>()))
		{
			bool bAlready = false;
			Reachable.Add(Node->GetBindingID(), &bAlready);
			if (!bAlready) Queue.Add(Node->GetBindingID());
		}
	}
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

	// Gather unreachable, user-deletable ed-nodes (Entry is exempt via CanUserDeleteNode).
	TArray<UEdGraphNode*> ToDelete;
	TSet<FGuid> DeletedIds;
	for (UEdGraphNode* EdNode : Graph->EdGraph->Nodes)
	{
		const UScriptableEdGraphNode* SfEdNode = Cast<UScriptableEdGraphNode>(EdNode);
		if (!SfEdNode || !SfEdNode->GetRuntimeNode()) continue;

		const FGuid Id = SfEdNode->GetRuntimeNode()->GetBindingID();
		if (!Id.IsValid() || Reachable.Contains(Id)) continue;
		if (!EdNode->CanUserDeleteNode()) continue;

		ToDelete.Add(EdNode);
		DeletedIds.Add(Id);
	}

	// Stale connections: those touching a node we're deleting, or referencing a node that no longer
	// exists (left dangling by an earlier delete — these are what show up as "missing node" in validation).
	TSet<FGuid> ValidNodeIds;
	ValidNodeIds.Reserve(Graph->Nodes.Num());
	for (const TObjectPtr<UScriptableNode>& Node : Graph->Nodes)
	{
		if (Node) ValidNodeIds.Add(Node->GetBindingID());
	}

	auto IsStaleConnection = [&DeletedIds, &ValidNodeIds](const FScriptableGraphConnection& Conn)
		{
			return DeletedIds.Contains(Conn.From.NodeID) || DeletedIds.Contains(Conn.To.NodeID)
				|| !ValidNodeIds.Contains(Conn.From.NodeID) || !ValidNodeIds.Contains(Conn.To.NodeID);
		};

	const bool bHasStaleConnections = Graph->Connections.ContainsByPredicate(IsStaleConnection);
	if (ToDelete.IsEmpty() && !bHasStaleConnections) return;

	const FScopedTransaction Transaction(LOCTEXT("CleanGraphTx", "Clean Graph"));
	Graph->Modify();
	Graph->EdGraph->Modify();

	// Drop stale connections, then destroy the orphan ed-nodes (which also strip their runtime nodes).
	Graph->Connections.RemoveAll(IsStaleConnection);

	GraphEditorWidget->ClearSelectionSet();
	for (UEdGraphNode* EdNode : ToDelete)
	{
		EdNode->Modify();
		EdNode->DestroyNode();
	}

	GraphEditorWidget->NotifyGraphChanged();
}

void FScriptableGraphEditor::OnOpenSearch()
{
	if (const TSharedPtr<FTabManager> TabManagerPin = GetTabManager())
	{
		TabManagerPin->TryInvokeTab(SearchTabId);
	}
	if (SearchBox.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(SearchBox);
	}
}

namespace
{
	/** Searchable label: ReceiveEvent shows its EventName, Task wrappers the inner task class, others the node class display name. */
	FString GetNodeSearchLabel(const UScriptableNode* Node)
	{
		if (!Node) return FString();
		if (const UScriptableNode_ReceiveEvent* Receive = Cast<UScriptableNode_ReceiveEvent>(Node))
		{
			return FString::Printf(TEXT("Event: %s"), Receive->EventName.IsNone() ? TEXT("(unnamed)") : *Receive->EventName.ToString());
		}
		if (const UScriptableNode_Task* TaskNode = Cast<UScriptableNode_Task>(Node))
		{
			if (TaskNode->Task) return TaskNode->Task->GetClass()->GetDisplayNameText().ToString();
		}
		return Node->GetClass()->GetDisplayNameText().ToString();
	}

	/** Appends an object's editable property values ("Name = Value") and binding paths ("Source -> Target") as searchable fragments. */
	void AppendObjectSearchText(const UScriptableObject* Object, TArray<FString>& OutParts)
	{
		if (!Object) return;

		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			const FProperty* Prop = *It;
			if (!Prop->HasAnyPropertyFlags(CPF_Edit)) continue;

			FString Value;
			Prop->ExportTextItem_Direct(Value, Prop->ContainerPtrToValuePtr<void>(Object), nullptr, const_cast<UScriptableObject*>(Object), PPF_None);
			if (!Value.IsEmpty())
			{
				OutParts.Add(FString::Printf(TEXT("%s = %s"), *Prop->GetName(), *Value));
			}
		}

		for (const FScriptablePropertyBinding& Binding : Object->GetPropertyBindings().Bindings)
		{
			OutParts.Add(FString::Printf(TEXT("%s -> %s"), *Binding.SourcePath.ToString(), *Binding.TargetPath.ToString()));
		}
	}

	/** Searchable fragments for a node: its title first, then its (and its task's) property values and binding paths. */
	void GatherNodeSearchParts(const UScriptableNode* Node, TArray<FString>& OutParts)
	{
		if (!Node) return;

		OutParts.Add(GetNodeSearchLabel(Node));
		AppendObjectSearchText(Node, OutParts);

		if (const UScriptableNode_Task* TaskNode = Cast<UScriptableNode_Task>(Node))
		{
			AppendObjectSearchText(TaskNode->Task, OutParts);
		}
	}

	/** True if any fragment of the node contains Query. OutContext is the matched fragment, or empty when the match is the title. */
	bool NodeMatchesQuery(const UScriptableNode* Node, const FString& Query, FString& OutContext)
	{
		TArray<FString> Parts;
		GatherNodeSearchParts(Node, Parts);

		for (int32 Index = 0; Index < Parts.Num(); ++Index)
		{
			if (Parts[Index].Contains(Query))
			{
				OutContext = (Index == 0) ? FString() : Parts[Index];
				return true;
			}
		}
		return false;
	}
}

TSharedRef<SDockTab> FScriptableGraphEditor::SpawnTab_Search(const FSpawnTabArgs& Args)
{
	TSharedRef<SWidget> Content =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.f)
		[
			SAssignNew(SearchBox, SSearchBox)
			.HintText(LOCTEXT("SearchHint", "Search nodes, properties, bindings..."))
			.OnTextChanged(this, &FScriptableGraphEditor::OnSearchTextChanged)
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SAssignNew(SearchListView, SListView<TSharedPtr<FScriptableGraphSearchResult>>)
			.ListItemsSource(&SearchResults)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &FScriptableGraphEditor::OnGenerateSearchRow)
			.OnMouseButtonClick(this, &FScriptableGraphEditor::OnSearchResultClicked)
		];

	// Seed with every node so an empty query lists the whole graph.
	OnSearchTextChanged(FText::GetEmpty());

	return SNew(SDockTab)
		.Label(LOCTEXT("SearchTab", "Search"))
		[
			Content
		];
}

void FScriptableGraphEditor::OnSearchTextChanged(const FText& InText)
{
	SearchResults.Reset();

	if (UScriptableGraph* Graph = EditedGraph.Get())
	{
		const FString Query = InText.ToString().TrimStartAndEnd();
		for (const TObjectPtr<UScriptableNode>& Node : Graph->Nodes)
		{
			if (!Node) continue;

			if (Query.IsEmpty())
			{
				SearchResults.Add(MakeShared<FScriptableGraphSearchResult>(FScriptableGraphSearchResult{ Node, FString() }));
				continue;
			}

			FString MatchContext;
			if (NodeMatchesQuery(Node, Query, MatchContext))
			{
				SearchResults.Add(MakeShared<FScriptableGraphSearchResult>(FScriptableGraphSearchResult{ Node, MatchContext }));
			}
		}
	}

	if (SearchListView.IsValid())
	{
		SearchListView->RequestListRefresh();
	}
}

TSharedRef<ITableRow> FScriptableGraphEditor::OnGenerateSearchRow(TSharedPtr<FScriptableGraphSearchResult> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	const UScriptableNode* Node = Item.IsValid() ? Item->Node.Get() : nullptr;
	const FString Label = Node ? GetNodeSearchLabel(Node) : TEXT("<invalid>");

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock).Text(FText::FromString(Label))
		];

	if (Item.IsValid() && !Item->MatchContext.IsEmpty())
	{
		Box->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
					.Text(FText::FromString(Item->MatchContext))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Font(FAppStyle::GetFontStyle("PropertyWindow.ItalicFont"))
			];
	}

	return SNew(STableRow<TSharedPtr<FScriptableGraphSearchResult>>, OwnerTable)
		.Padding(FMargin(4.f, 2.f))
		[
			Box
		];
}

void FScriptableGraphEditor::OnSearchResultClicked(TSharedPtr<FScriptableGraphSearchResult> Item)
{
	if (!Item.IsValid() || !Item->Node.IsValid() || !GraphEditorWidget.IsValid()) return;

	if (UEdGraphNode* EdNode = FindEdNodeByRuntimeId(Item->Node->GetBindingID()))
	{
		GraphEditorWidget->JumpToNode(EdNode, /*bRequestRename*/ false, /*bSelectNode*/ true);
	}
}

namespace
{
	/** Make a name unique against a set of already-used FNames by appending _N suffixes until it fits. */
	FName MakeUniqueAmong(FName Desired, const TSet<FName>& UsedNames, const TCHAR* FallbackBase)
	{
		FName Candidate = Desired.IsNone() ? FName(FallbackBase) : Desired;
		if (!UsedNames.Contains(Candidate)) return Candidate;
		for (int32 i = 1; i < 10000; ++i)
		{
			Candidate = FName(*FString::Printf(TEXT("%s_%d"), *Desired.ToString(), i));
			if (!UsedNames.Contains(Candidate)) return Candidate;
		}
		return Desired;
	}
}

bool FScriptableGraphEditor::CanConvertSelectionToSubGraph() const
{
	if (!GraphEditorWidget.IsValid()) return false;
	const FGraphPanelSelectionSet& Selection = GraphEditorWidget->GetSelectedNodes();
	if (Selection.IsEmpty()) return false;

	/** Need at least one node that isn't Entry/Exit — those don't migrate, they only inform sub-asset layout. */
	for (UObject* Obj : Selection)
	{
		const UScriptableEdGraphNode* SfEd = Cast<UScriptableEdGraphNode>(Obj);
		if (!SfEd) continue;
		const UScriptableNode* Runtime = SfEd->GetRuntimeNode();
		if (Runtime && !Runtime->IsA<UScriptableNode_Entry>() && !Runtime->IsA<UScriptableNode_Exit>()) return true;
	}
	return false;
}

void FScriptableGraphEditor::OnConvertSelectionToSubGraph()
{
	UScriptableGraph* SourceGraph = EditedGraph.Get();
	if (!SourceGraph || !SourceGraph->EdGraph || !GraphEditorWidget.IsValid()) return;

	/** 1. Gather selection. Entry/Exit are pseudo-selected: they don't migrate (Entry must stay in the source,
	 * sub-asset already has its own), but their position and Outputs (Exit case) seed the sub-asset's layout. */
	TArray<UScriptableEdGraphNode*> SelectedEd;
	TArray<UScriptableNode*> SelectedRuntime;
	TSet<FGuid> SelectedIds;
	int64 SumX = 0, SumY = 0;
	int32 PosCount = 0;

	bool bEntryPseudoSelected = false;
	FIntPoint SourceEntryPos(0, 0);
	bool bExitPseudoSelected = false;
	FIntPoint SourceExitPos(0, 0);

	for (UObject* Obj : GraphEditorWidget->GetSelectedNodes())
	{
		UScriptableEdGraphNode* SfEd = Cast<UScriptableEdGraphNode>(Obj);
		if (!SfEd) continue;
		UScriptableNode* Runtime = SfEd->GetRuntimeNode();
		if (!Runtime) continue;

		if (Runtime->IsA<UScriptableNode_Entry>())
		{
			bEntryPseudoSelected = true;
			SourceEntryPos = FIntPoint(SfEd->NodePosX, SfEd->NodePosY);
			continue;
		}
		if (Runtime->IsA<UScriptableNode_Exit>())
		{
			bExitPseudoSelected = true;
			SourceExitPos = FIntPoint(SfEd->NodePosX, SfEd->NodePosY);
			continue;
		}

		SelectedEd.Add(SfEd);
		SelectedRuntime.Add(Runtime);
		SelectedIds.Add(Runtime->GetBindingID());
		SumX += SfEd->NodePosX;
		SumY += SfEd->NodePosY;
		++PosCount;
	}
	if (SelectedRuntime.IsEmpty()) return;

	const int32 CentroidX = PosCount > 0 ? static_cast<int32>(SumX / PosCount) : 0;
	const int32 CentroidY = PosCount > 0 ? static_cast<int32>(SumY / PosCount) : 0;

	/** 2. Save-As dialog for the new sub-asset. Cancel aborts the whole op. */
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	const FString DefaultPath = FPaths::GetPath(SourceGraph->GetPathName());
	UScriptableGraphAssetFactory* Factory = NewObject<UScriptableGraphAssetFactory>();
	UObject* CreatedAsset = AssetTools.CreateAssetWithDialog(TEXT("NewSubGraph"), DefaultPath, UScriptableGraph::StaticClass(), Factory, NAME_None);
	UScriptableGraph* SubAsset = Cast<UScriptableGraph>(CreatedAsset);
	if (!SubAsset) return;

	/** 3. Partition source's Connections by which side of the selection each endpoint lies on. */
	TArray<FScriptableGraphConnection> Internal;
	TArray<FScriptableGraphConnection> Inbound;
	TArray<FScriptableGraphConnection> Outbound;
	for (const FScriptableGraphConnection& C : SourceGraph->Connections)
	{
		const bool bFromSel = SelectedIds.Contains(C.From.NodeID);
		const bool bToSel = SelectedIds.Contains(C.To.NodeID);
		if (bFromSel && bToSel) Internal.Add(C);
		else if (bToSel) Inbound.Add(C);
		else if (bFromSel) Outbound.Add(C);
	}

	const FGuid SourceEntryId = SourceGraph->EntryNodeID;
	FGuid SourceExitId;
	for (const TObjectPtr<UScriptableNode>& Node : SourceGraph->Nodes)
	{
		if (Node && Node->IsA<UScriptableNode_Exit>()) { SourceExitId = Node->GetBindingID(); break; }
	}

	/**
	 * Sub-asset construction runs OUTSIDE the source's transaction: cross-package Rename undo is unreliable
	 * and rolling back the sub-asset on Ctrl-Z would orphan its package file (already on disk via the Save-As
	 * dialog). Instead we clone the selected nodes into the sub-asset; the originals remain intact in the source
	 * (Ctrl-Z restores them with their positions because we never touched the originals' properties).
	 */
	if (!SubAsset->EdGraph)
	{
		UScriptableEdGraph* SubEdGraph = NewObject<UScriptableEdGraph>(SubAsset, UScriptableEdGraph::StaticClass(), NAME_None, RF_Transactional);
		SubEdGraph->Schema = UScriptableEdGraphSchema::StaticClass();
		SubAsset->EdGraph = SubEdGraph;
	}

	/** Clone each selected node into the sub-asset. DuplicateObject preserves UPROPERTYs including BindingID,
	 * so connection refs by NodeID still resolve to the clones inside the sub-asset. Visual-only state on the
	 * ed-node (hide-unconnected toggle, comment bubble, NodeComment) is copied over after the spawn since
	 * SpawnEdNodeForRuntime builds a fresh ed-node. */
	for (int32 i = 0; i < SelectedRuntime.Num(); ++i)
	{
		UScriptableNode* Original = SelectedRuntime[i];
		UScriptableEdGraphNode* OriginalEd = SelectedEd[i];
		UScriptableNode* Cloned = DuplicateObject<UScriptableNode>(Original, SubAsset);
		if (!Cloned) continue;
		SubAsset->Nodes.Add(Cloned);
		UScriptableEdGraphNode* ClonedEd = ScriptableGraphEditorHelpers::SpawnEdNodeForRuntime(SubAsset->EdGraph, Cloned, FVector2f(OriginalEd->NodePosX, OriginalEd->NodePosY));
		if (ClonedEd)
		{
			ClonedEd->bHideUnconnectedPins = OriginalEd->bHideUnconnectedPins;
			ClonedEd->NodeComment = OriginalEd->NodeComment;
			ClonedEd->bCommentBubblePinned = OriginalEd->bCommentBubblePinned;
			ClonedEd->bCommentBubbleVisible = OriginalEd->bCommentBubbleVisible;
			ClonedEd->ApplyPinVisibility();
		}
	}

	/** Move internal connections verbatim — BindingIDs in clones match the originals' IDs. */
	for (const FScriptableGraphConnection& C : Internal)
	{
		SubAsset->Connections.Add(C);
	}

	auto FindSubEdNodeForRuntime = [SubAsset](const FGuid& RuntimeID) -> UScriptableEdGraphNode*
		{
			for (UEdGraphNode* EdNode : SubAsset->EdGraph->Nodes)
			{
				UScriptableEdGraphNode* SfEd = Cast<UScriptableEdGraphNode>(EdNode);
				if (SfEd && SfEd->GetRuntimeNode() && SfEd->GetRuntimeNode()->GetBindingID() == RuntimeID) return SfEd;
			}
			return nullptr;
		};

	int32 MigratedMinX = INT_MAX, MigratedMaxX = INT_MIN;
	for (const UScriptableEdGraphNode* SfEd : SelectedEd)
	{
		MigratedMinX = FMath::Min(MigratedMinX, SfEd->NodePosX);
		MigratedMaxX = FMath::Max(MigratedMaxX, SfEd->NodePosX);
	}
	const int32 LeftAnchor = MigratedMinX == INT_MAX ? 0 : MigratedMinX;
	const int32 RightAnchor = MigratedMaxX == INT_MIN ? 0 : MigratedMaxX;

	/** Position the sub-asset's auto-created Entry at the source Entry's coordinates IF the user pseudo-selected it.
	 * Without that signal we leave Entry's ed-node unspawned so the editor places it at the default (0,0) on first open. */
	if (bEntryPseudoSelected)
	{
		UScriptableNode* SubEntryRuntime = nullptr;
		for (const TObjectPtr<UScriptableNode>& N : SubAsset->Nodes)
		{
			if (N && N->GetBindingID() == SubAsset->EntryNodeID) { SubEntryRuntime = N; break; }
		}
		if (SubEntryRuntime && !FindSubEdNodeForRuntime(SubAsset->EntryNodeID))
		{
			ScriptableGraphEditorHelpers::SpawnEdNodeForRuntime(SubAsset->EdGraph, SubEntryRuntime, FVector2f(SourceEntryPos.X, SourceEntryPos.Y));
		}
	}

	auto EnsureSubExit = [&](const FVector2f& Location) -> UScriptableNode_Exit*
		{
			for (const TObjectPtr<UScriptableNode>& N : SubAsset->Nodes)
			{
				if (UScriptableNode_Exit* E = Cast<UScriptableNode_Exit>(N)) return E;
			}
			UScriptableNode_Exit* NewExit = NewObject<UScriptableNode_Exit>(SubAsset, NAME_None, RF_Transactional);
			SubAsset->Nodes.Add(NewExit);
			ScriptableGraphEditorHelpers::SpawnEdNodeForRuntime(SubAsset->EdGraph, NewExit, Location);
			return NewExit;
		};

	/** Exit pseudo-selected: mirror the source's whole Outputs list so the sub-asset's Exit has the same pin shape
	 * the user laid out, and pre-spawn it at the source Exit's position. */
	if (bExitPseudoSelected)
	{
		for (const FName& Out : SourceGraph->Outputs)
		{
			if (!Out.IsNone()) SubAsset->Outputs.AddUnique(Out);
		}
		EnsureSubExit(FVector2f(SourceExitPos.X, SourceExitPos.Y));
	}

	/** Inbound: dedupe by (TargetNodeID, TargetPinName). Inbounds from the source's Entry/Exit short-circuit
	 * onto the sub-asset's own Entry/Exit; everything else gets a ReceiveEvent per unique target pin. */
	TMap<FScriptableGraphPinRef, FName> InboundEventName;
	TSet<FName> UsedEventNames;
	UsedEventNames.Add(UScriptableNode_SubGraph::InInputName);
	UsedEventNames.Add(UScriptableNode_SubGraph::CancelInputName);
	for (const FScriptableGraphConnection& C : Inbound)
	{
		if (C.From.NodeID == SourceEntryId)
		{
			FScriptableGraphConnection Wire;
			Wire.From.NodeID = SubAsset->EntryNodeID;
			Wire.From.PinName = UScriptableNode_Entry::OutOutputName;
			Wire.To = C.To;
			SubAsset->Connections.Add(Wire);
			continue;
		}

		if (SourceExitId.IsValid() && C.From.NodeID == SourceExitId)
		{
			/** When Exit wasn't pseudo-selected, this is the fallback path: spawn it near the right edge of the migrated nodes. */
			UScriptableNode_Exit* SubExit = EnsureSubExit(FVector2f(RightAnchor + 400, CentroidY));
			if (C.From.PinName != UScriptableNode_Exit::FinishedOutputName && C.From.PinName != UScriptableNode_Exit::CancelledOutputName)
			{
				SubAsset->Outputs.AddUnique(C.From.PinName);
			}
			FScriptableGraphConnection Wire;
			Wire.From.NodeID = SubExit->GetBindingID();
			Wire.From.PinName = C.From.PinName;
			Wire.To = C.To;
			SubAsset->Connections.Add(Wire);
			continue;
		}

		if (InboundEventName.Contains(C.To)) continue;
		const FName EventName = MakeUniqueAmong(C.To.PinName, UsedEventNames, TEXT("Event"));
		UsedEventNames.Add(EventName);

		UScriptableNode_ReceiveEvent* Ev = NewObject<UScriptableNode_ReceiveEvent>(SubAsset, NAME_None, RF_Transactional);
		Ev->EventName = EventName;
		SubAsset->Nodes.Add(Ev);

		FVector2f EvLoc(0.f, 0.f);
		if (const UScriptableEdGraphNode* TargetEd = FindSubEdNodeForRuntime(C.To.NodeID))
		{
			EvLoc = FVector2f(TargetEd->NodePosX - 300, TargetEd->NodePosY);
		}
		ScriptableGraphEditorHelpers::SpawnEdNodeForRuntime(SubAsset->EdGraph, Ev, EvLoc);

		FScriptableGraphConnection Wire;
		Wire.From.NodeID = Ev->GetBindingID();
		Wire.From.PinName = UScriptableNode_ReceiveEvent::OutOutputName;
		Wire.To = C.To;
		SubAsset->Connections.Add(Wire);

		InboundEventName.Add(C.To, EventName);
	}

	/** Outbound: dedupe by (SourceNodeID, SourcePinName). A source pin literally named "Completed" is the universal
	 * task default — collapse it to a blank Finish (which fires the built-in "Finished" output) so we don't pollute
	 * Outputs with a duplicate of what's already there. Named pins ("Wololo", "True", etc.) become custom outputs. */
	TMap<FScriptableGraphPinRef, FName> OutboundOutputName;
	TSet<FName> UsedOutputNames;
	UsedOutputNames.Add(UScriptableNode_Exit::FinishedOutputName);
	UsedOutputNames.Add(UScriptableNode_Exit::CancelledOutputName);
	for (const FName& Existing : SubAsset->Outputs) UsedOutputNames.Add(Existing);

	const FName DefaultCompletedPin = UScriptableTask::CompletedOutputName;

	for (const FScriptableGraphConnection& C : Outbound)
	{
		if (OutboundOutputName.Contains(C.From)) continue;

		const bool bIsDefaultCompletion = (C.From.PinName == DefaultCompletedPin);

		FName OutputName;
		if (bIsDefaultCompletion)
		{
			OutputName = UScriptableNode_Exit::FinishedOutputName;
		}
		else
		{
			OutputName = MakeUniqueAmong(C.From.PinName, UsedOutputNames, TEXT("Out"));
			UsedOutputNames.Add(OutputName);
			SubAsset->Outputs.AddUnique(OutputName);
		}

		UScriptableNode_Finish* Fin = NewObject<UScriptableNode_Finish>(SubAsset, NAME_None, RF_Transactional);
		Fin->OutputName = bIsDefaultCompletion ? NAME_None : OutputName;
		SubAsset->Nodes.Add(Fin);

		FVector2f FinLoc(0.f, 0.f);
		if (const UScriptableEdGraphNode* SourceEd = FindSubEdNodeForRuntime(C.From.NodeID))
		{
			FinLoc = FVector2f(SourceEd->NodePosX + 300, SourceEd->NodePosY);
		}
		ScriptableGraphEditorHelpers::SpawnEdNodeForRuntime(SubAsset->EdGraph, Fin, FinLoc);

		FScriptableGraphConnection Wire;
		Wire.From = C.From;
		Wire.To.NodeID = Fin->GetBindingID();
		Wire.To.PinName = UScriptableNode_Finish::InInputName;
		SubAsset->Connections.Add(Wire);

		OutboundOutputName.Add(C.From, OutputName);
	}

	SubAsset->MarkPackageDirty();

	/**
	 * Source-side changes wrapped in their own transaction. Ctrl-Z reverts these (originals come back, SubGraph
	 * node disappears) without trying to roll back the sub-asset package. The sub-asset stays as-is — the user
	 * can save it or delete the asset file if they don't want it.
	 */
	const FScopedTransaction Tx(LOCTEXT("ConvertToSubGraphTx", "Convert to Sub-Graph"));
	SourceGraph->Modify();
	SourceGraph->EdGraph->Modify();

	UScriptableNode_SubGraph* SubGraphNode = NewObject<UScriptableNode_SubGraph>(SourceGraph, NAME_None, RF_Transactional);
	SubGraphNode->SubGraphAsset = SubAsset;
	SourceGraph->Nodes.Add(SubGraphNode);

	SourceGraph->Connections.RemoveAll([&SelectedIds](const FScriptableGraphConnection& C)
		{
			return SelectedIds.Contains(C.From.NodeID) || SelectedIds.Contains(C.To.NodeID);
		});

	TArray<FScriptableGraphConnection> EmittedInbound;
	auto AddInboundParent = [&](const FScriptableGraphConnection& NewC)
		{
			if (EmittedInbound.ContainsByPredicate([&NewC](const FScriptableGraphConnection& X) { return X == NewC; })) return;
			EmittedInbound.Add(NewC);
			SourceGraph->Connections.Add(NewC);
		};
	for (const FScriptableGraphConnection& C : Inbound)
	{
		FScriptableGraphConnection NewC;
		NewC.From = C.From;
		NewC.To.NodeID = SubGraphNode->GetBindingID();
		if (C.From.NodeID == SourceEntryId || (SourceExitId.IsValid() && C.From.NodeID == SourceExitId))
		{
			NewC.To.PinName = UScriptableNode_SubGraph::InInputName;
		}
		else
		{
			NewC.To.PinName = InboundEventName.FindRef(C.To);
		}
		AddInboundParent(NewC);
	}
	for (const FScriptableGraphConnection& C : Outbound)
	{
		FScriptableGraphConnection NewC;
		NewC.From.NodeID = SubGraphNode->GetBindingID();
		NewC.From.PinName = OutboundOutputName.FindRef(C.From);
		NewC.To = C.To;
		SourceGraph->Connections.Add(NewC);
	}

	/** Remove the originals from the source. Modify on each SfEd captures its pre-remove state (LinkedTo +
	 * RuntimeNode pointer) so Ctrl-Z brings them back intact. ReconstructEdGraphFromAsset re-wires visual links
	 * from SourceGraph->Connections below, so we don't bother clearing LinkedTo here. */
	for (UScriptableEdGraphNode* SfEd : SelectedEd)
	{
		SfEd->Modify();
		SourceGraph->EdGraph->Nodes.Remove(SfEd);
	}
	for (UScriptableNode* Original : SelectedRuntime)
	{
		SourceGraph->Nodes.Remove(Original);
	}

	UScriptableEdGraphNode* SubGraphEdNode = ScriptableGraphEditorHelpers::SpawnEdNodeForRuntime(SourceGraph->EdGraph, SubGraphNode, FVector2f(CentroidX, CentroidY));
	if (SubGraphEdNode)
	{
		SubGraphEdNode->NodePosX = CentroidX;
		SubGraphEdNode->NodePosY = CentroidY;
	}

	ReconstructEdGraphFromAsset();
	MarkDirtySinceLastCompile();
	RefreshErrorBanners();

	if (GEditor)
	{
		if (UAssetEditorSubsystem* AES = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			AES->OpenEditorForAsset(SubAsset);
		}
	}
}

#undef LOCTEXT_NAMESPACE