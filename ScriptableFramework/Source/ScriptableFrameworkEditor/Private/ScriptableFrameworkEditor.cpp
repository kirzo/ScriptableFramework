// Copyright 2026 kirzo

#pragma once

#include "ScriptableFrameworkEditor.h"
#include "ScriptableTypeCache.h"
#include "ScriptableFrameworkEditorStyle.h"

#include "ScriptableTasks/ScriptableTask.h"
#include "ScriptableTasks/ScriptableActionAsset.h"
#include "ScriptableTasks/ScriptableAction.h"

#include "ScriptableConditions/ScriptableCondition.h"
#include "ScriptableConditions/ScriptableRequirementAsset.h"
#include "ScriptableConditions/ScriptableRequirement.h"

#include "ScriptableFrameworkEd/Customization/ScriptableTaskCustomization.h"
#include "ScriptableFrameworkEd/Customization/ScriptableActionCustomization.h"

#include "ScriptableFrameworkEd/Customization/ScriptableRequirementCustomization.h"
#include "ScriptableFrameworkEd/Customization/ScriptableConditionCustomization.h"

#include "ScriptableConditions/ScriptableCondition_NestedRequirement.h"
#include "ScriptableFrameworkEd/Customization/ScriptableConditionNestedRequirementCustomization.h"

#include "ScriptableTasks/ScriptableTask_NestedAction.h"
#include "ScriptableFrameworkEd/Customization/ScriptableTaskNestedActionCustomization.h"

#include "ScriptableTasks/ScriptableTask_SetLocal.h"
#include "ScriptableFrameworkEd/Customization/ScriptableTaskSetLocalCustomization.h"

#include "ScriptableTasks/ScriptableTask_CallFunction.h"
#include "ScriptableFrameworkEd/Customization/ScriptableTaskCallFunctionCustomization.h"

#include "ScriptableNodes/ScriptableNode_Finish.h"
#include "ScriptableFrameworkEd/Customization/ScriptableNodeFinishCustomization.h"

#include "ScriptableNodes/ScriptableNode.h"
#include "ScriptableNodes/ScriptableGraph.h"
#include "ScriptableFrameworkEd/Graph/Editor/ScriptableGraphEditor.h"
#include "ScriptableFrameworkEd/Graph/Editor/ScriptableGraphAssetTypeActions.h"
#include "ScriptableFrameworkEd/Graph/ScriptableEdGraphNode.h"
#include "ScriptableFrameworkEd/RunnerViewer/SScriptableRunnerViewer.h"
#include "EdGraph/EdGraph.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "ScriptableFrameworkEd/Graph/ScriptableGraphPinFactory.h"
#include "ScriptableFrameworkEd/Graph/ScriptableGraphNodeFactory.h"
#include "ScriptableFrameworkEd/Graph/ScriptableGraphCommands.h"
#include "ScriptableFrameworkEd/Graph/ScriptableEdGraphNodeRegistry.h"

#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Containers/Ticker.h"
#include "Validation/KzAssetValidationUtils.h"
#include "Core/KzValidationTypes.h"
#include "ScriptableFrameworkEditorHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Widgets/SWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/StyleColors.h"

#define LOCTEXT_NAMESPACE "FScriptableFrameworkEditorModule"

void FScriptableFrameworkEditorModule::OnStartupModule()
{
	FScriptableFrameworkEditorStyle::Initialize();
	FScriptableGraphCommands::Register();

	FScriptableEdGraphNodeRegistry::Initialize();

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	ScriptableAssetCategoryBit = AssetTools.RegisterAdvancedAssetCategory("ScriptableFramework", INVTEXT("Scriptable Framework"));

	RegisterAssetTypeAction<UScriptableActionAsset>(ScriptableAssetCategoryBit, INVTEXT("Scriptable Action"), FScriptableFrameworkEditorStyle::ActionColor.ToFColor(true));
	RegisterAssetTypeAction<UScriptableRequirementAsset>(ScriptableAssetCategoryBit, INVTEXT("Scriptable Requirement"), FScriptableFrameworkEditorStyle::RequirementColor.ToFColor(true));

	// Registered by hand (not via the template) so it carries a custom PerformAssetDiff.
	GraphAssetTypeActions = MakeShared<FScriptableGraphAssetTypeActions>(
		ScriptableAssetCategoryBit, INVTEXT("Scriptable Graph"),
		FScriptableFrameworkEditorStyle::GraphColor.ToFColor(true),
		UScriptableGraph::StaticClass(), TArray<FText>());
	FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().RegisterAssetTypeActions(GraphAssetTypeActions.ToSharedRef());

	RegisterPropertyLayout<UScriptableTask, FScriptableTaskCustomization>();
	RegisterPropertyLayout<UScriptableCondition, FScriptableConditionCustomization>();
	RegisterPropertyLayout<FScriptableAction, FScriptableActionCustomization>();
	RegisterPropertyLayout<FScriptableRequirement, FScriptableRequirementCustomization>();
	RegisterPropertyLayout<UScriptableCondition_NestedRequirement, FScriptableConditionNestedRequirementCustomization>();
	RegisterPropertyLayout<UScriptableTask_NestedAction, FScriptableTaskNestedActionCustomization>();
	RegisterPropertyLayout<UScriptableTask_SetLocal, FScriptableTaskSetLocalCustomization>();
	RegisterPropertyLayout<UScriptableTask_CallFunction, FScriptableTaskCallFunctionCustomization>();
	RegisterClassLayout<UScriptableNode_Finish, FScriptableNodeFinishCustomization>();

	RegisterPinFactory<FScriptableGraphPinFactory>();
	RegisterNodeFactory<FScriptableGraphNodeFactory>();

	LaunchBlockedHandle = UScriptableGraph::OnLaunchBlockedByCompile.AddRaw(this, &FScriptableFrameworkEditorModule::HandleLaunchBlocked);
	PostLoadedHandle = UScriptableGraph::OnPostLoaded.AddRaw(this, &FScriptableFrameworkEditorModule::HandleGraphPostLoaded);
	BeginPIEHandle = FEditorDelegates::BeginPIE.AddRaw(this, &FScriptableFrameworkEditorModule::HandleBeginPIE);
	EndPIEHandle = FEditorDelegates::EndPIE.AddRaw(this, &FScriptableFrameworkEditorModule::HandleEndPIE);
	NodeActivatedHandle = UScriptableNode::OnNodeActivatedEditor.AddRaw(this, &FScriptableFrameworkEditorModule::HandleNodeActivated);

	/** Global "Scriptable Runners" tab. Spawnable from Window menu (via the FTabSpawnerEntry). */
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		TEXT("ScriptableRunners"),
		FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
			{
				return SNew(SDockTab)
					.TabRole(ETabRole::NomadTab)
					.Label(NSLOCTEXT("ScriptableFrameworkEditor", "ScriptableRunnersTab", "Scriptable Runners"))
					[
						SNew(SScriptableRunnerViewer)
					];
			}))
		.SetDisplayName(NSLOCTEXT("ScriptableFrameworkEditor", "ScriptableRunnersTabLabel", "Scriptable Runners"))
		.SetTooltipText(NSLOCTEXT("ScriptableFrameworkEditor", "ScriptableRunnersTabTip", "Live view of every active scriptable graph runner across PIE worlds."))
		.SetGroup(FModuleManager::LoadModuleChecked<FWorkspaceMenuStructureModule>("WorkspaceMenuStructure").GetWorkspaceMenuStructure().GetDeveloperToolsDebugCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Server"));
}

void FScriptableFrameworkEditorModule::OnShutdownModule()
{
	if (GraphAssetTypeActions.IsValid() && FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get().UnregisterAssetTypeActions(GraphAssetTypeActions.ToSharedRef());
	}
	GraphAssetTypeActions.Reset();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TEXT("ScriptableRunners"));
	UScriptableNode::OnNodeActivatedEditor.Remove(NodeActivatedHandle);
	FEditorDelegates::EndPIE.Remove(EndPIEHandle);
	FEditorDelegates::BeginPIE.Remove(BeginPIEHandle);
	UScriptableGraph::OnPostLoaded.Remove(PostLoadedHandle);
	UScriptableGraph::OnLaunchBlockedByCompile.Remove(LaunchBlockedHandle);

	FScriptableEdGraphNodeRegistry::Shutdown();
	FScriptableGraphCommands::Unregister();
	FScriptableFrameworkEditorStyle::Shutdown();
}

void FScriptableFrameworkEditorModule::HandleLaunchBlocked(UScriptableGraph* Asset)
{
	if (!Asset) return;
	CompileBlockedDuringPIE.AddUnique(Asset);
}

void FScriptableFrameworkEditorModule::HandleGraphPostLoaded(UScriptableGraph* Asset)
{
	if (!Asset) return;

	/**
	 * Defer to next tick: PostLoad fires mid-load, when cross-asset references (RunGraph targets,
	 * SubGraph asset pointers) may still be resolving. By the next tick the world is consistent and
	 * any validator that walks references gets a stable view.
	 */
	TWeakObjectPtr<UScriptableGraph> WeakAsset(Asset);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakAsset](float) -> bool
		{
			UScriptableGraph* Graph = WeakAsset.Get();
			if (!Graph) return false;

			const bool bHasError = FKzAssetValidationUtils::RunValidation(Graph).ContainsByPredicate([](const FKzValidationIssue& Issue)
				{
					return Issue.Severity == EKzValidationSeverity::Error;
				});

			/** Passive refresh: update the in-memory flag only. The asset-open compile and the Compile button persist it. */
			Graph->bLastCompileFailed = bHasError;
			return false;
		}), 0.0f);
}

void FScriptableFrameworkEditorModule::HandleBeginPIE(const bool bSimulating)
{
	CompileBlockedDuringPIE.Reset();
}

void FScriptableFrameworkEditorModule::HandleNodeActivated(UScriptableNode* Node)
{
	if (!Node || !GEditor) return;

	const UScriptableGraph* Asset = Node->FindOwningAsset();
	if (!Asset || !Asset->EdGraph) return;

	/** Check the asset's breakpoint table — must exist AND be enabled to trigger the pause. */
	const FGuid TargetId = Node->GetBindingID();
	const bool* EnabledPtr = Asset->Breakpoints.Find(TargetId);
	if (!EnabledPtr || !*EnabledPtr) return;

	/** Find the source ed-node to focus the editor on it. */
	UEdGraphNode* MatchingEdNode = nullptr;
	for (UEdGraphNode* EdNode : Asset->EdGraph->Nodes)
	{
		const UScriptableEdGraphNode* SfEd = Cast<UScriptableEdGraphNode>(EdNode);
		if (SfEd && SfEd->GetRuntimeNode() && SfEd->GetRuntimeNode()->GetBindingID() == TargetId)
		{
			MatchingEdNode = EdNode;
			break;
		}
	}

	/** Engine's BP-style pause toggle. Engine then drives PIE viewport into the paused state next tick. */
	if (UWorld* PlayWorld = GEditor->PlayWorld)
	{
		PlayWorld->bDebugPauseExecution = true;
	}

	UAssetEditorSubsystem* Sub = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!Sub) return;

	UScriptableGraph* MutableAsset = const_cast<UScriptableGraph*>(Asset);
	Sub->OpenEditorForAsset(MutableAsset);
	if (IAssetEditorInstance* EditorInst = Sub->FindEditorForAsset(MutableAsset, /*bFocusIfOpen*/ true))
	{
		if (FScriptableGraphEditor* GraphEditor = static_cast<FScriptableGraphEditor*>(EditorInst))
		{
			GraphEditor->JumpToNode(MatchingEdNode);
		}
	}
}

void FScriptableFrameworkEditorModule::HandleEndPIE(const bool bSimulating)
{
	if (CompileBlockedDuringPIE.IsEmpty()) return;

	/** Filter out stale weak pointers; we still want the dialog if at least one survived. */
	TArray<TWeakObjectPtr<UScriptableGraph>> Survivors;
	Survivors.Reserve(CompileBlockedDuringPIE.Num());
	for (const TWeakObjectPtr<UScriptableGraph>& Weak : CompileBlockedDuringPIE)
	{
		if (Weak.IsValid()) Survivors.Add(Weak);
	}
	CompileBlockedDuringPIE.Reset();
	if (Survivors.IsEmpty()) return;

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("CompileBlockedPIETitle", "Scriptable Graph Compilation Errors"))
		.SizingRule(ESizingRule::Autosized)
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	/**
	 * Click on a link sets this and closes the window; we open the asset after AddModalWindow returns.
	 * Opening from inside the modal loop makes the engine flicker because the new editor window can't
	 * take focus until the modal exits.
	 */
	TSharedRef<TWeakObjectPtr<UScriptableGraph>> PendingOpen = MakeShared<TWeakObjectPtr<UScriptableGraph>>();
	const TWeakPtr<SWindow> WeakWindow = Window;

	TSharedRef<SVerticalBox> LinkList = SNew(SVerticalBox);
	for (const TWeakObjectPtr<UScriptableGraph>& Weak : Survivors)
	{
		TWeakObjectPtr<UScriptableGraph> Captured = Weak;
		LinkList->AddSlot()
			.AutoHeight()
			.Padding(0.f, 2.f)
			.HAlign(HAlign_Left)
			[
				SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
					.ContentPadding(FMargin(0.f))
					.Cursor(EMouseCursor::Hand)
					.OnClicked_Lambda([Captured, PendingOpen, WeakWindow]()
					{
						*PendingOpen = Captured;
						if (TSharedPtr<SWindow> Pin = WeakWindow.Pin())
						{
							Pin->RequestDestroyWindow();
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock)
							.Text(FText::FromString(Weak.Get()->GetName()))
							.ColorAndOpacity(FStyleColors::AccentBlue)
					]
			];
	}

	const TSharedRef<SWidget> Content =
		SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(16.f)
			[
				SNew(SVerticalBox)

					/** Icon + header text. */
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Top)
							.Padding(0.f, 0.f, 12.f, 0.f)
							[
								SNew(SImage)
									.DesiredSizeOverride(FVector2D(24.f, 24.f))
									.Image(FAppStyle::GetBrush("Icons.WarningWithColor"))
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.f)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
									.AutoWrapText(true)
									.Text(LOCTEXT("CompileBlockedPIEHeader", "The following Scriptable Graphs have unresolved compiler errors."))
							]
					]

					/** Asset links. */
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(36.f, 12.f, 0.f, 0.f)
					[
						LinkList
					]

					/** Close button, bottom-right. */
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Right)
					.Padding(0.f, 16.f, 0.f, 0.f)
					[
						SNew(SButton)
							.Text(LOCTEXT("CompileBlockedPIEClose", "Close"))
							.OnClicked_Lambda([WeakWindow]()
							{
								if (TSharedPtr<SWindow> Pin = WeakWindow.Pin())
								{
									Pin->RequestDestroyWindow();
								}
								return FReply::Handled();
							})
					]
			];

	Window->SetContent(Content);

	const TSharedPtr<SWindow> RootWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
	FSlateApplication::Get().AddModalWindow(Window, RootWindow);

	/** Modal closed; safe to open the asset editor now. */
	if (UScriptableGraph* Graph = PendingOpen->Get())
	{
		if (UAssetEditorSubsystem* Sub = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr)
		{
			Sub->OpenEditorForAsset(Graph);
		}
	}
}

TSharedPtr<FScriptableTypeCache> FScriptableFrameworkEditorModule::GetScriptableTypeCache()
{
	if (!ScriptableTypeCache.IsValid())
	{
		ScriptableTypeCache = MakeShareable(new FScriptableTypeCache());
		ScriptableTypeCache->AddRootClass(UScriptableTask::StaticClass());
		ScriptableTypeCache->AddRootClass(UScriptableCondition::StaticClass());
		ScriptableTypeCache->AddRootClass(UScriptableNode::StaticClass());
	}

	return ScriptableTypeCache;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FScriptableFrameworkEditorModule, ScriptableFrameworkEditor);