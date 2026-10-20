// Copyright 2026 kirzo

#include "SScriptableTypePicker.h"

#include "ScriptableFrameworkEditor.h"
#include "ScriptableFrameworkEditorHelpers.h"
#include "ScriptableFrameworkEditorStyle.h"
#include "ScriptableTypeCache.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Input/SSearchBox.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "ScriptableTasks/ScriptableTask.h"
#include "ScriptableTasks/ScriptableActionAsset.h"
#include "ScriptableConditions/ScriptableCondition.h"
#include "ScriptableConditions/ScriptableRequirementAsset.h"

#include "ScriptableFrameworkEd/Graph/ScriptablePaletteAction.h"
#include "GraphEditorDragDropAction.h"

#include "ScriptableNodes/ScriptableNode.h"
#include "ScriptableFrameworkEd/Graph/ScriptableEdGraphNode.h"
#include "ScriptableFrameworkEd/Graph/ScriptableEdGraphNode_Native.h"
#include "ScriptableFrameworkEd/Graph/ScriptableEdGraphNodeRegistry.h"

#define LOCTEXT_NAMESPACE "ScriptableFrameworkEditor"

// --------------------------------------------------------------------------------------
// SScriptableTypeSelector Implementation
// --------------------------------------------------------------------------------------

TMap<FObjectKey, SScriptableTypeSelector::FCategoryExpansionState> SScriptableTypeSelector::CategoryExpansionStates;

SScriptableTypeSelector::~SScriptableTypeSelector()
{
	OnPickerClosed.ExecuteIfBound();
}

void SScriptableTypeSelector::Construct(const FArguments& InArgs)
{
	// Set default style if none provided (copied from ComboBox.Row default)
	ItemStyle = InArgs._ItemStyle ? InArgs._ItemStyle : &FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("ComboBox.Row");
	MenuRowPadding = FMargin(2.0f);

	OnNodeTypePicked = InArgs._OnNodeTypePicked;
	OnPickerClosed = InArgs._OnPickerClosed;
	CategoryKey = FObjectKey(InArgs._BaseScriptStruct);

	ClassCategoryMeta = InArgs._ClassCategoryMeta;
	FilterCategoryMeta = InArgs._FilterCategoryMeta;
	AdditionalBaseClass = InArgs._AdditionalBaseClass;
	BaseClassRootCategory = InArgs._BaseClassRootCategory;
	AdditionalBaseClassRootCategory = InArgs._AdditionalBaseClassRootCategory;
	AdditionalBaseClassUserRootCategory = InArgs._AdditionalBaseClassUserRootCategory;
	ExcludedClasses = InArgs._ExcludedClasses;
	bEnableDragOut = InArgs._EnableDragOut;

	TArray<FString> Filters;
	InArgs._Filter.ParseIntoArray(Filters, TEXT(","));
	for (FString& Filter : Filters)
	{
		Filter.TrimStartAndEndInline();
		TArray<FString> SubPath;
		Filter.ParseIntoArray(SubPath, TEXT("|"));
		for (FString& FilterPath : SubPath)
		{
			FilterPath.TrimStartAndEndInline();
		}

		if (!SubPath.IsEmpty())
		{
			FilterPaths.Add(SubPath);
		}
	}

	CacheTypes(InArgs._BaseScriptStruct, InArgs._BaseClass);

	NodeTypeTree = SNew(STreeView<TSharedPtr<FScriptableTypeItem>>)
		.SelectionMode(ESelectionMode::Single)
		.TreeItemsSource(&FilteredRootNode->Children)
		.OnGenerateRow(this, &SScriptableTypeSelector::GenerateNodeTypeRow)
		.OnGetChildren(this, &SScriptableTypeSelector::GetNodeTypeChildren)
		.OnMouseButtonClick(this, &SScriptableTypeSelector::OnNodeTypeMouseClick)
		.OnKeyDownHandler(this, &SScriptableTypeSelector::OnTreeKeyDown)
		.OnExpansionChanged(this, &SScriptableTypeSelector::OnNodeTypeExpansionChanged);

	// Restore category expansion state from previous use.
	RestoreExpansionState();

	// Expand current selection.
	const TArray<TSharedPtr<FScriptableTypeItem>> Path = GetPathToItemStruct(InArgs._CurrentStruct);
	if (Path.Num() > 0)
	{
		// Expand all categories up to the selected item.
		bIsRestoringExpansion = true;
		for (const TSharedPtr<FScriptableTypeItem>& Item : Path)
		{
			NodeTypeTree->SetItemExpansion(Item, true);
		}
		bIsRestoringExpansion = false;

		NodeTypeTree->SetItemSelection(Path.Last(), true);
		NodeTypeTree->RequestScrollIntoView(Path.Last());
	}

	const bool bHasTitle = !InArgs._TitleText.IsEmpty();

	// Collapse-all button is shared between two layouts. Build it once.
	TSharedRef<SButton> CollapseButton = SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
		.ToolTipText(LOCTEXT("CollapseAllCategories", "Collapse All"))
		.OnClicked(this, &SScriptableTypeSelector::OnCollapseAllButtonClicked)
		[
			SNew(SImage)
				.Image(FAppStyle::GetBrush("SpinBox.Arrows"))
				.ColorAndOpacity(FSlateColor::UseForeground())
		];

	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

	if (bHasTitle)
	{
		// Title row: label on the left (filling), collapse button tucked on the right.
		Body->AddSlot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.Padding(6, 4, 4, 4)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
							.Text(InArgs._TitleText)
							.Font(FAppStyle::Get().GetFontStyle("BoldFont"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						CollapseButton
					]
			];

		// Search alone, full width, so it visually matches the native BP context menu.
		Body->AddSlot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Top)
			.Padding(4, 0, 4, 4)
			.AutoHeight()
			[
				SAssignNew(SearchBox, SSearchBox)
					.OnTextChanged(this, &SScriptableTypeSelector::OnSearchBoxTextChanged)
					.OnTextCommitted(this, &SScriptableTypeSelector::OnSearchBoxCommitted)
					.Visibility(InArgs._SearchVisibility)
			];
	}
	else
	{
		// Legacy layout (used by customizations): collapse + search share one compact row.
		Body->AddSlot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Top)
			.Padding(4, 2, 4, 2)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						CollapseButton
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Top)
					.FillWidth(1.0f)
					[
						SAssignNew(SearchBox, SSearchBox)
							.OnTextChanged(this, &SScriptableTypeSelector::OnSearchBoxTextChanged)
							.Visibility(InArgs._SearchVisibility)
					]
			];
	}

	Body->AddSlot()
		[
			NodeTypeTree.ToSharedRef()
		];

	ChildSlot
		[
			SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Menu.Background"))
				.Padding(6)
				[
					SNew(SBox)
						.WidthOverride(InArgs._ListWidth)
						.HeightOverride(InArgs._ListHeight)
						[
							Body
						]
				]
		];
}

TSharedPtr<SWidget> SScriptableTypeSelector::GetWidgetToFocusOnOpen()
{
	return SearchBox;
}

void SScriptableTypeSelector::ClearSelection()
{
	if (NodeTypeTree.IsValid())
	{
		NodeTypeTree->ClearSelection();
	}
}

void SScriptableTypeSelector::SortNodeTypesFunctionItemsRecursive(TArray<TSharedPtr<FScriptableTypeItem>>& Items)
{
	Items.Sort([](const TSharedPtr<FScriptableTypeItem>& A, const TSharedPtr<FScriptableTypeItem>& B)
	{
		if (A->IsCategory() && B->IsCategory())
		{
			const bool bSystemA = A->GetCategoryName().Equals(ScriptableFrameworkEditor::MD_SystemCategory.ToString(), ESearchCase::IgnoreCase);
			const bool bSystemB = B->GetCategoryName().Equals(ScriptableFrameworkEditor::MD_SystemCategory.ToString(), ESearchCase::IgnoreCase);

			if (!bSystemA && bSystemB)
			{
				return false;
			}

			return A->GetCategoryName() < B->GetCategoryName();
		}
		if (A->IsCategory() && !B->IsCategory())
		{
			return true;
		}
		if (!A->IsCategory() && B->IsCategory())
		{
			return false;
		}
		if (A->Struct != nullptr && B->Struct != nullptr)
		{
			return A->Struct->GetDisplayNameText().CompareTo(B->Struct->GetDisplayNameText()) <= 0;
		}
		return true;
	});

	for (const TSharedPtr<FScriptableTypeItem>& Item : Items)
	{
		SortNodeTypesFunctionItemsRecursive(Item->Children);
	}
}

TSharedPtr<SScriptableTypeSelector::FScriptableTypeItem> SScriptableTypeSelector::FindOrCreateItemForCategory(TArray<TSharedPtr<FScriptableTypeItem>>& Items, TArrayView<FString> CategoryPath)
{
	check(CategoryPath.Num() > 0);

	const FString& CategoryName = CategoryPath.Last();

	int32 Idx = 0;
	for (; Idx < Items.Num(); ++Idx)
	{
		// found item
		if (Items[Idx]->GetCategoryName() == CategoryName)
		{
			return Items[Idx];
		}

		// passed the place where it should have been, break out
		if (Items[Idx]->GetCategoryName() > CategoryName)
		{
			break;
		}
	}

	TSharedPtr<FScriptableTypeItem> NewItem = Items.Insert_GetRef(MakeShared<FScriptableTypeItem>(), Idx);
	NewItem->CategoryPath = CategoryPath;
	return NewItem;
}

FText SScriptableTypeSelector::GetNodeCategory(const UStruct* Struct, const FName& MetaKey) const
{
	if (const UClass* Class = Cast<const UClass>(Struct))
	{
		if (Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
		{
			return Class->GetMetaDataText(TEXT("Category"));
		}
	}
	return Struct->GetMetaDataText(MetaKey);
}

namespace
{
	/** Returns the icon (and tint) the graph ed-node uses for a native node class, or an unset icon if it has none. */
	FSlateIcon GetNativeNodeGraphIcon(const UClass* NodeClass, FLinearColor& OutColor)
	{
		OutColor = FLinearColor::White;
		if (!NodeClass || !NodeClass->IsChildOf(UScriptableNode::StaticClass())) return FSlateIcon();

		const UScriptableNode* NodeCDO = Cast<UScriptableNode>(NodeClass->GetDefaultObject());
		if (!NodeCDO) return FSlateIcon();

		// Resolve the matching ed-node (falling back to the generic one) and ask it for its icon —
		// the same lookup the graph editor uses when spawning a visual node.
		UClass* EdNodeClass = FScriptableEdGraphNodeRegistry::FindEdNodeClassFor(NodeCDO);
		if (!EdNodeClass) EdNodeClass = UScriptableEdGraphNode_Native::StaticClass();

		const UScriptableEdGraphNode* EdCDO = EdNodeClass->GetDefaultObject<UScriptableEdGraphNode>();
		return EdCDO ? EdCDO->GetIconAndTint(OutColor) : FSlateIcon();
	}
}

void SScriptableTypeSelector::AddNode(const UStruct* Struct, const FName& MetaKey, const FText& RootCategory)
{
	if (!Struct || !RootNode.IsValid())
	{
		return;
	}

	const FText CategoryName = GetNodeCategory(Struct, MetaKey);

	// Build the full category path: RootCategory (if any) is the top-level group, then the
	// node's own meta-driven path appends below it. This is what creates the "Native Nodes /
	// Sequence" vs "Scriptable Tasks / Debug / Log Message" split when the caller asks for it,
	// while leaving callers that pass an empty RootCategory unaffected (no extra grouping).
	TArray<FString> CategoryPath;
	if (!RootCategory.IsEmpty())
	{
		CategoryPath.Add(RootCategory.ToString());
	}
	if (!CategoryName.IsEmpty())
	{
		TArray<FString> SubPath;
		CategoryName.ToString().ParseIntoArray(SubPath, TEXT("|"));
		for (FString& SubCategory : SubPath)
		{
			SubCategory.TrimStartAndEndInline();
		}
		CategoryPath.Append(MoveTemp(SubPath));
	}

	TSharedPtr<FScriptableTypeItem> ParentItem = RootNode;
	for (int32 PathIndex = 0; PathIndex < CategoryPath.Num(); ++PathIndex)
	{
		ParentItem = FindOrCreateItemForCategory(ParentItem->Children, MakeArrayView(CategoryPath.GetData(), PathIndex + 1));
	}
	check(ParentItem);

	const TSharedPtr<FScriptableTypeItem>& Item = ParentItem->Children.Add_GetRef(MakeShared<FScriptableTypeItem>());
	Item->Struct = Struct;
	Item->IconColor = FLinearColor::Gray;

	// Native nodes show the same icon their ed-node uses on the graph canvas; tasks and conditions, their own
	if (const UClass* NodeClass = Cast<const UClass>(Struct))
	{
		FLinearColor IconTint;
		FSlateIcon Icon = GetNativeNodeGraphIcon(NodeClass, IconTint);
		if (!Icon.IsSet())
		{
			Icon = FScriptableFrameworkEditorStyle::GetScriptableObjectIcon(NodeClass, IconTint);
		}

		if (Icon.IsSet())
		{
			Item->Icon = Icon;
			Item->IconColor = IconTint;
		}
	}
}

void SScriptableTypeSelector::AddNode(const FAssetData& AssetData, const FText& RootCategory)
{
	if (!RootNode.IsValid())
	{
		return;
	}

	// Get Category from Asset Registry Tag
	static const FName CategoryTagName(TEXT("MenuCategory"));
	FString CategoryStr;
	AssetData.GetTagValue<FString>(CategoryTagName, CategoryStr);

	// Build full category path: RootCategory (if any) at top, then the asset's tag-driven path.
	TArray<FString> CategoryPath;
	if (!RootCategory.IsEmpty())
	{
		CategoryPath.Add(RootCategory.ToString());
	}
	if (!CategoryStr.IsEmpty())
	{
		TArray<FString> SubPath;
		CategoryStr.ParseIntoArray(SubPath, TEXT("|"), true);
		for (FString& SubCategory : SubPath)
		{
			SubCategory.TrimStartAndEndInline();
		}
		CategoryPath.Append(MoveTemp(SubPath));
	}

	TSharedPtr<FScriptableTypeItem> ParentItem = RootNode;
	for (int32 PathIndex = 0; PathIndex < CategoryPath.Num(); ++PathIndex)
	{
		ParentItem = FindOrCreateItemForCategory(ParentItem->Children, MakeArrayView(CategoryPath.GetData(), PathIndex + 1));
	}
	check(ParentItem);

	TSharedPtr<FScriptableTypeItem> Item = ParentItem->Children.Add_GetRef(MakeShared<FScriptableTypeItem>());
	Item->AssetData = AssetData;

	// The asset class's own icon (Action, Requirement, Graph), falling back to the generic one
	const UClass* AssetClass = AssetData.GetClass();
	Item->Icon = FSlateIconFinder::FindIconForClass(AssetClass ? AssetClass : UObject::StaticClass());
	Item->IconColor = FLinearColor::White;
}

bool SScriptableTypeSelector::MatchesCategoryPath(const TArray<FString>& CategoryPath)
{
	if (FilterPaths.IsEmpty())
	{
		return true;
	}

	if (CategoryPath.IsEmpty())
	{
		return true;
	}

	// Always show 'System' category (Special case)
	if (CategoryPath[0].Equals(ScriptableFrameworkEditor::MD_SystemCategory.ToString(), ESearchCase::IgnoreCase))
	{
		return true;
	}

	return FilterPaths.ContainsByPredicate([CategoryPath](const TArray<FString>& FilterPath)
	{
		if (CategoryPath.Num() < FilterPath.Num())
		{
			return false;
		}

		for (int32 i = 0; i < FilterPath.Num(); i++)
		{
			if (!FilterPath[i].Equals(CategoryPath[i], ESearchCase::IgnoreCase))
			{
				return false;
			}
		}

		return true;
	});
}

bool SScriptableTypeSelector::MatchesFilter(const UStruct* Struct, const FName& MetaKey)
{
	if (!Struct || !RootNode.IsValid())
	{
		return false;
	}

	const FText CategoryName = GetNodeCategory(Struct, MetaKey);

	if (CategoryName.IsEmpty())
	{
		return true;
	}

	// Split into subcategories and trim
	TArray<FString> CategoryPath;
	CategoryName.ToString().ParseIntoArray(CategoryPath, TEXT("|"));
	for (FString& SubCategory : CategoryPath)
	{
		SubCategory.TrimStartAndEndInline();
	}

	return MatchesCategoryPath(CategoryPath);
}

bool SScriptableTypeSelector::MatchesFilter(const FAssetData& AssetData)
{
	if (!RootNode.IsValid())
	{
		return false;
	}

	// Read category from Tag
	static const FName CategoryTagName(TEXT("MenuCategory"));
	FString CategoryStr;
	AssetData.GetTagValue<FString>(CategoryTagName, CategoryStr);

	if (CategoryStr.IsEmpty())
	{
		return true;
	}

	// Split into subcategories and trim
	TArray<FString> CategoryPath;
	CategoryStr.ParseIntoArray(CategoryPath, TEXT("|"));
	for (FString& SubCategory : CategoryPath)
	{
		SubCategory.TrimStartAndEndInline();
	}

	return MatchesCategoryPath(CategoryPath);
}

void SScriptableTypeSelector::CacheTypes(const UScriptStruct* BaseScriptStruct, const UClass* BaseClass)
{
	// Get all usable types from the class cache.
	FScriptableFrameworkEditorModule& EditorModule = FModuleManager::GetModuleChecked<FScriptableFrameworkEditorModule>(TEXT("ScriptableFrameworkEditor"));
	FScriptableTypeCache* TypeCache = EditorModule.GetScriptableTypeCache().Get();
	check(TypeCache);

	// Create tree of node types based on category.
	RootNode = MakeShared<FScriptableTypeItem>();

	// 1. Structs from BaseScriptStruct (uses ClassCategoryMeta).
	if (BaseScriptStruct)
	{
		TArray<TSharedPtr<FScriptableTypeData>> StructNodes;
		TypeCache->GetScripStructs(BaseScriptStruct, StructNodes);

		for (const TSharedPtr<FScriptableTypeData>& Data : StructNodes)
		{
			if (const UScriptStruct* ScriptStruct = Data->GetScriptStruct())
			{
				if (ScriptStruct == BaseScriptStruct) continue;
				if (ScriptStruct->HasMetaData(TEXT("Hidden"))) continue;
				if (!MatchesFilter(ScriptStruct, ClassCategoryMeta)) continue;

				AddNode(ScriptStruct, ClassCategoryMeta);
			}
		}
	}

	// 2. Classes from BaseClass (uses ClassCategoryMeta), grouped under BaseClassRootCategory if set.
	CacheClassesFromBase(BaseClass, ClassCategoryMeta, BaseClassRootCategory);

	// 3. Classes from AdditionalBaseClass (uses AdditionalClassCategoryMeta, falling back to ClassCategoryMeta).
	// When AdditionalBaseClassUserRootCategory is set, user-defined subclasses (those that live
	// outside /Script/ScriptableFramework*) go under that root instead of the default one — letting
	// the picker show built-ins and project nodes as separate top-level groups.
	if (AdditionalBaseClass)
	{
		const FName ExtraMetaKey = AdditionalClassCategoryMeta.IsNone() ? ClassCategoryMeta : AdditionalClassCategoryMeta;
		CacheClassesFromBase(AdditionalBaseClass, ExtraMetaKey, AdditionalBaseClassRootCategory, AdditionalBaseClassUserRootCategory);
	}

	// 4. Assets associated with BaseClass (Task → ActionAsset, Condition → RequirementAsset).
	UClass* AssetClassToSearch = nullptr;

	if (BaseClass && BaseClass->IsChildOf(UScriptableTask::StaticClass()))
	{
		AssetClassToSearch = UScriptableActionAsset::StaticClass();
	}
	else if (BaseClass && BaseClass->IsChildOf(UScriptableCondition::StaticClass()))
	{
		AssetClassToSearch = UScriptableRequirementAsset::StaticClass();
	}

	if (AssetClassToSearch)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> AssetDataList;

		FARFilter Filter;
		Filter.ClassPaths.Add(AssetClassToSearch->GetClassPathName());
		Filter.bRecursivePaths = true;

		AssetRegistryModule.Get().GetAssets(Filter, AssetDataList);

		for (const FAssetData& Asset : AssetDataList)
		{
			if (MatchesFilter(Asset))
			{
				AddNode(Asset, BaseClassRootCategory);
			}
		}
	}

	SortNodeTypesFunctionItemsRecursive(RootNode->Children);

	FilteredRootNode = RootNode;
}

void SScriptableTypeSelector::CacheClassesFromBase(const UClass* BaseClass, const FName& MetaKey, const FText& RootCategory, const FText& UserRootCategory)
{
	if (!BaseClass) return;

	FScriptableFrameworkEditorModule& EditorModule = FModuleManager::GetModuleChecked<FScriptableFrameworkEditorModule>(TEXT("ScriptableFrameworkEditor"));
	FScriptableTypeCache* TypeCache = EditorModule.GetScriptableTypeCache().Get();
	check(TypeCache);

	TArray<TSharedPtr<FScriptableTypeData>> ObjectNodes;
	TypeCache->GetClasses(BaseClass, ObjectNodes);

	const bool bSplitByPackage = !UserRootCategory.IsEmpty();
	static const TCHAR* FrameworkPackagePrefix = TEXT("/Script/ScriptableFramework");

	for (const TSharedPtr<FScriptableTypeData>& Data : ObjectNodes)
	{
		const UClass* Class = Data->GetClass();
		if (!Class) continue;
		if (Class == BaseClass) continue;
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Hidden | CLASS_HideDropDown)) continue;
		if (Class->HasMetaData(TEXT("Hidden"))) continue;
		if (ExcludedClasses.Contains(Class)) continue;
		if (!MatchesFilter(Class, MetaKey)) continue;

		FText EffectiveRoot = RootCategory;
		if (bSplitByPackage)
		{
			// Prefix check covers the framework's runtime module ("/Script/ScriptableFramework")
			// AND its sub-plugins ("/Script/ScriptableFrameworkAI", "/Script/ScriptableFrameworkSequencer",
			// ...). Anything else — user project modules, unrelated plugins — is treated as user-defined.
			const FString PackageName = Class->GetOutermost()->GetName();
			if (!PackageName.StartsWith(FrameworkPackagePrefix))
			{
				EffectiveRoot = UserRootCategory;
			}
		}

		AddNode(Class, MetaKey, EffectiveRoot);
	}
}

TSharedRef<ITableRow> SScriptableTypeSelector::GenerateNodeTypeRow(TSharedPtr<FScriptableTypeItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	FText DisplayName;
	if (Item->IsCategory())
	{
		DisplayName = FText::FromString(Item->GetCategoryName());
	}
	else if (Item->AssetData.IsValid())
	{
		DisplayName = FText::FromName(Item->AssetData.AssetName);
	}
	else
	{
		DisplayName = Item->Struct ? Item->Struct->GetDisplayNameText() : LOCTEXT("None", "None");
	}

	FText Tooltip = Item->Struct ? Item->Struct->GetMetaDataText("Tooltip") : FText::GetEmpty();
	if (Tooltip.IsEmpty())
	{
		Tooltip = DisplayName;
	}

	const FSlateBrush* Icon = nullptr;
	FSlateColor IconColor;
	if (!Item->IsCategory())
	{
		if (Item->Icon.IsSet())
		{
			Icon = Item->Icon.GetIcon();
			IconColor = Item->IconColor;
		}
		else if (const UClass* ItemClass = Cast<UClass>(Item->Struct))
		{
			Icon = FSlateIconFinder::FindIconBrushForClass(ItemClass);
			IconColor = Item->IconColor;
		}
		else if (const UScriptStruct* ItemScriptStruct = Cast<UScriptStruct>(Item->Struct))
		{
			Icon = FSlateIconFinder::FindIconBrushForClass(UScriptStruct::StaticClass());
			IconColor = Item->IconColor;
		}
		else
		{
			// None
			Icon = FSlateIconFinder::FindIconBrushForClass(nullptr);
			IconColor = FSlateColor::UseForeground();
		}
	}

	FOnDragDetected DragHandler;
	if (bEnableDragOut)
	{
		DragHandler.BindLambda([Item](const FGeometry&, const FPointerEvent&) -> FReply
			{
				if (!Item.IsValid() || Item->IsCategory())
				{
					return FReply::Unhandled();
				}

				// Wrap the row's payload in a schema action that SGraphPanel recognizes at drop.
				// FGraphSchemaActionDragDropAction is the standard UE op — at drop, SGraphPanel
				// calls PerformAction on the wrapped action with the graph and cursor position.
				TSharedPtr<FEdGraphSchemaAction> Action = MakeShared<FScriptablePaletteAction>(
					FText::GetEmpty(),
					Item->Struct ? Item->Struct->GetDisplayNameText() : FText::FromName(Item->AssetData.AssetName),
					FText::GetEmpty(),
					0,
					Item->Struct,
					Item->AssetData);

				return FReply::Handled().BeginDragDrop(FGraphSchemaActionDragDropAction::New(Action));
			});
	}

	TSharedRef<STableRow<TSharedPtr<FScriptableTypeItem>>> Row = SNew(STableRow<TSharedPtr<FScriptableTypeItem>>, OwnerTable)
		.Style(ItemStyle)
		.Padding(MenuRowPadding)
		.OnDragDetected(DragHandler);

	Row->SetContent(
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(0, 2.0f, 4.0f, 2.0f)
		.AutoWidth()
		[
			SNew(SImage)
				.Visibility(Icon ? EVisibility::Visible : EVisibility::Collapsed)
				.ColorAndOpacity(IconColor)
				.DesiredSizeOverride(FVector2D(16.0f, 16.0f))
				.Image(Icon)
		]
	+ SHorizontalBox::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
				.Font(Item->IsCategory() ? FAppStyle::Get().GetFontStyle("BoldFont") : FAppStyle::Get().GetFontStyle("NormalText"))
				.Text(DisplayName)
				.ToolTipText(Tooltip)
				.HighlightText_Lambda([this]() { return SearchBox.IsValid() ? SearchBox->GetText() : FText::GetEmpty(); })
		]
		);

	return Row;
}

void SScriptableTypeSelector::GetNodeTypeChildren(TSharedPtr<FScriptableTypeItem> Item, TArray<TSharedPtr<FScriptableTypeItem>>& OutItems) const
{
	if (Item.IsValid())
	{
		OutItems = Item->Children;
	}
}

void SScriptableTypeSelector::OnNodeTypeMouseClick(TSharedPtr<FScriptableTypeItem> ClickedItem)
{
	// Per-click commit. OnSelectionChanged misses the case where the user clicks an item that's
	// already selected (e.g. the search-driven auto-highlight), so we hook the row click directly.
	if (!ClickedItem.IsValid() || ClickedItem->IsCategory()) return;
	if (OnNodeTypePicked.IsBound())
	{
		OnNodeTypePicked.Execute(ClickedItem->Struct, ClickedItem->AssetData);
	}
}

void SScriptableTypeSelector::CommitSelection()
{
	if (!NodeTypeTree.IsValid()) return;

	const TArray<TSharedPtr<FScriptableTypeItem>> Selected = NodeTypeTree->GetSelectedItems();
	if (Selected.Num() == 1 && Selected[0].IsValid() && !Selected[0]->IsCategory() && OnNodeTypePicked.IsBound())
	{
		OnNodeTypePicked.Execute(Selected[0]->Struct, Selected[0]->AssetData);
	}
}

FReply SScriptableTypeSelector::OnTreeKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Enter && NodeTypeTree.IsValid())
	{
		const TArray<TSharedPtr<FScriptableTypeItem>> Selected = NodeTypeTree->GetSelectedItems();
		if (Selected.Num() == 1 && Selected[0].IsValid() && !Selected[0]->IsCategory())
		{
			CommitSelection();
			return FReply::Handled();
		}
	}

	// Let the tree handle navigation keys (arrows, Home/End) and category Enter (expand/collapse).
	return FReply::Unhandled();
}

void SScriptableTypeSelector::OnNodeTypeExpansionChanged(TSharedPtr<FScriptableTypeItem> ExpandedItem, bool bInExpanded)
{
	// Do not save expansion state we're restoring expansion state, or when showing filtered results. 
	if (bIsRestoringExpansion || FilteredRootNode != RootNode)
	{
		return;
	}

	if (ExpandedItem.IsValid() && ExpandedItem->CategoryPath.Num() > 0)
	{
		FCategoryExpansionState& ExpansionState = CategoryExpansionStates.FindOrAdd(CategoryKey);
		const FString Path = FString::Join(ExpandedItem->CategoryPath, TEXT("|"));
		if (bInExpanded)
		{
			ExpansionState.CollapsedCategories.Remove(Path);
		}
		else
		{
			ExpansionState.CollapsedCategories.Add(Path);
		}
	}
}

void SScriptableTypeSelector::OnSearchBoxTextChanged(const FText& NewText)
{
	if (!NodeTypeTree.IsValid())
	{
		return;
	}

	FilteredRootNode.Reset();

	TArray<FString> FilterStrings;
	NewText.ToString().ParseIntoArrayWS(FilterStrings);
	FilterStrings.RemoveAll([](const FString& String) { return String.IsEmpty(); });

	if (FilterStrings.IsEmpty())
	{
		// Show all when there's no filter string.
		FilteredRootNode = RootNode;
		NodeTypeTree->SetTreeItemsSource(&FilteredRootNode->Children);
		RestoreExpansionState();
		NodeTypeTree->RequestTreeRefresh();
		return;
	}

	FilteredRootNode = MakeShared<FScriptableTypeItem>();
	FilterNodeTypesChildren(FilterStrings, /*bParentMatches*/false, RootNode->Children, FilteredRootNode->Children);

	NodeTypeTree->SetTreeItemsSource(&FilteredRootNode->Children);
	ExpandAll(FilteredRootNode->Children);
	NodeTypeTree->RequestTreeRefresh();

	// Auto-highlight the first leaf in the filtered set so Enter commits the top match immediately,
	// mirroring the BP action menu's behaviour. Scrolls it into view in case it was off-screen.
	NodeTypeTree->ClearSelection();
	if (const TSharedPtr<FScriptableTypeItem> FirstLeaf = FindFirstLeaf(FilteredRootNode->Children))
	{
		NodeTypeTree->SetItemSelection(FirstLeaf, true, ESelectInfo::Direct);
		NodeTypeTree->RequestScrollIntoView(FirstLeaf);
	}
}

TSharedPtr<SScriptableTypeSelector::FScriptableTypeItem> SScriptableTypeSelector::FindFirstLeaf(const TArray<TSharedPtr<FScriptableTypeItem>>& Items) const
{
	for (const TSharedPtr<FScriptableTypeItem>& Item : Items)
	{
		if (!Item.IsValid()) continue;
		if (!Item->IsCategory()) return Item;
		if (TSharedPtr<FScriptableTypeItem> Nested = FindFirstLeaf(Item->Children)) return Nested;
	}
	return nullptr;
}

void SScriptableTypeSelector::OnSearchBoxCommitted(const FText& InText, ETextCommit::Type CommitType)
{
	// Only Enter commits; losing focus or pressing Escape leaves the picker open without selecting.
	if (CommitType == ETextCommit::OnEnter)
	{
		CommitSelection();
	}
}

int32 SScriptableTypeSelector::FilterNodeTypesChildren(const TArray<FString>& FilterStrings, const bool bParentMatches, const TArray<TSharedPtr<FScriptableTypeItem>>& SourceArray, TArray<TSharedPtr<FScriptableTypeItem>>& OutDestArray)
{
	int32 NumFound = 0;

	/**
	 * Subsequence-based fuzzy match: every char of Filter must appear in Item in order, case-insensitive.
	 * "lgmsg" matches "LogMessage". Empty Filter is a wildcard, empty Item never matches a non-empty Filter.
	 */
	auto FuzzyMatch = [](const FString& Filter, const FString& Item)
	{
		const int32 FilterLen = Filter.Len();
		if (FilterLen == 0) return true;
		const int32 ItemLen = Item.Len();
		if (ItemLen == 0) return false;

		int32 FilterIdx = 0;
		for (int32 ItemIdx = 0; ItemIdx < ItemLen && FilterIdx < FilterLen; ++ItemIdx)
		{
			if (FChar::ToLower(Item[ItemIdx]) == FChar::ToLower(Filter[FilterIdx]))
			{
				++FilterIdx;
			}
		}
		return FilterIdx == FilterLen;
	};

	/** AND across whitespace-separated tokens: every token must fuzzy-match the item name. */
	auto MatchFilter = [&FilterStrings, &FuzzyMatch](const TSharedPtr<FScriptableTypeItem>& SourceItem)
	{
		const FString ItemName = SourceItem->Struct ? SourceItem->Struct->GetDisplayNameText().ToString() : SourceItem->GetCategoryName();
		for (const FString& Filter : FilterStrings)
		{
			if (!FuzzyMatch(Filter, ItemName)) return false;
		}
		return true;
	};

	for (const TSharedPtr<FScriptableTypeItem>& SourceItem : SourceArray)
	{
		// Check if our name matches the filters
		// If bParentMatches is true, the search matched a parent category.
		const bool bMatchesFilters = bParentMatches || MatchFilter(SourceItem);

		int32 NumChildren = 0;
		if (bMatchesFilters)
		{
			NumChildren++;
		}

		// if we don't match, then we still want to check all our children
		TArray<TSharedPtr<FScriptableTypeItem>> FilteredChildren;
		NumChildren += FilterNodeTypesChildren(FilterStrings, bMatchesFilters, SourceItem->Children, FilteredChildren);

		// then add this item to the destination array
		if (NumChildren > 0)
		{
			TSharedPtr<FScriptableTypeItem>& NewItem = OutDestArray.Add_GetRef(MakeShared<FScriptableTypeItem>());
			NewItem->CategoryPath = SourceItem->CategoryPath;
			NewItem->Struct = SourceItem->Struct;
			NewItem->Icon = SourceItem->Icon;
			NewItem->IconColor = SourceItem->IconColor;
			NewItem->Children = FilteredChildren;

			NumFound += NumChildren;
		}
	}

	return NumFound;
}

TArray<TSharedPtr<SScriptableTypeSelector::FScriptableTypeItem>> SScriptableTypeSelector::GetPathToItemStruct(const UStruct* Struct) const
{
	TArray<TSharedPtr<FScriptableTypeItem>> Path;

	TSharedPtr<FScriptableTypeItem> CurrentParent = FilteredRootNode;

	if (Struct)
	{
		FText FullCategoryName = Struct->GetMetaDataText("Category");
		if (!FullCategoryName.IsEmpty())
		{
			TArray<FString> CategoryPath;
			FullCategoryName.ToString().ParseIntoArray(CategoryPath, TEXT("|"));

			for (const FString& SubCategory : CategoryPath)
			{
				const FString Trimmed = SubCategory.TrimStartAndEnd();

				TSharedPtr<FScriptableTypeItem>* FoundItem =
					CurrentParent->Children.FindByPredicate([&Trimmed](const TSharedPtr<FScriptableTypeItem>& Item)
				{
					return Item->GetCategoryName() == Trimmed;
				});

				if (FoundItem != nullptr)
				{
					Path.Add(*FoundItem);
					CurrentParent = *FoundItem;
				}
			}
		}
	}

	const TSharedPtr<FScriptableTypeItem>* FoundItem =
		CurrentParent->Children.FindByPredicate([Struct](const TSharedPtr<FScriptableTypeItem>& Item)
	{
		return Item->Struct == Struct;
	});

	if (FoundItem != nullptr)
	{
		Path.Add(*FoundItem);
	}

	return Path;
}

FReply SScriptableTypeSelector::OnCollapseAllButtonClicked()
{
	CollapseAll();
	return FReply::Handled();;
}

void SScriptableTypeSelector::ExpandAll(const TArray<TSharedPtr<FScriptableTypeItem>>& Items)
{
	for (const TSharedPtr<FScriptableTypeItem>& Item : Items)
	{
		NodeTypeTree->SetItemExpansion(Item, true);
		ExpandAll(Item->Children);
	}
}

void SScriptableTypeSelector::CollapseAll()
{
	FCategoryExpansionState& ExpansionState = CategoryExpansionStates.FindOrAdd(CategoryKey);

	for (const TSharedPtr<FScriptableTypeItem> Item : NodeTypeTree->GetRootItems())
	{
		const FString Path = FString::Join(Item->CategoryPath, TEXT("|"));
		ExpansionState.CollapsedCategories.Add(Path);
	}

	RestoreExpansionState();
}

void SScriptableTypeSelector::RestoreExpansionState()
{
	FCategoryExpansionState& ExpansionState = CategoryExpansionStates.FindOrAdd(CategoryKey);

	TSet<TSharedPtr<FScriptableTypeItem>> CollapseNodes;
	for (const FString& Category : ExpansionState.CollapsedCategories)
	{
		TArray<FString> CategoryPath;
		Category.ParseIntoArray(CategoryPath, TEXT("|"));

		TSharedPtr<FScriptableTypeItem> CurrentParent = RootNode;

		for (const FString& SubCategory : CategoryPath)
		{
			TSharedPtr<FScriptableTypeItem>* FoundItem =
				CurrentParent->Children.FindByPredicate([&SubCategory](const TSharedPtr<FScriptableTypeItem>& Item)
			{
				return Item->GetCategoryName() == SubCategory;
			});

			if (FoundItem != nullptr)
			{
				CollapseNodes.Add(*FoundItem);
				CurrentParent = *FoundItem;
			}
		}
	}

	if (NodeTypeTree.IsValid())
	{
		bIsRestoringExpansion = true;

		ExpandAll(RootNode->Children);
		for (const TSharedPtr<FScriptableTypeItem>& Node : CollapseNodes)
		{
			NodeTypeTree->SetItemExpansion(Node, false);
		}

		bIsRestoringExpansion = false;
	}
}


// --------------------------------------------------------------------------------------
// SScriptableTypePicker Implementation (Facade)
// --------------------------------------------------------------------------------------

SScriptableTypePicker::SScriptableTypePicker()
{
}

SScriptableTypePicker::~SScriptableTypePicker()
{

}

void SScriptableTypePicker::Construct(const FArguments& InArgs)
{
	check(InArgs._ComboBoxStyle);

	// Setup Button Content
	TSharedPtr<SWidget> ButtonContent = InArgs._Content.Widget;
	if (InArgs._Content.Widget == SNullWidget::NullWidget)
	{
		SAssignNew(ButtonContent, STextBlock)
			.Text(NSLOCTEXT("SScriptableTypePicker", "ContentWarning", "No Content Provided"))
			.ColorAndOpacity(FLinearColor::Red);
	}

	// Create inner selector
	TypeSelector = SNew(SScriptableTypeSelector)
		.ItemStyle(InArgs._ItemStyle)
		.ListWidth(InArgs._ListWidth)
		.ListHeight(InArgs._ListHeight)
		.SearchVisibility(InArgs._SearchVisibility)
		.CurrentStruct(InArgs._CurrentStruct)
		.BaseScriptStruct(InArgs._BaseScriptStruct)
		.BaseClass(InArgs._BaseClass)
		.ClassCategoryMeta(InArgs._ClassCategoryMeta)
		.FilterCategoryMeta(InArgs._FilterCategoryMeta)
		.AdditionalBaseClass(InArgs._AdditionalBaseClass)
		.AdditionalClassCategoryMeta(InArgs._AdditionalClassCategoryMeta)
		.Filter(InArgs._Filter)
		.OnNodeTypePicked(InArgs._OnNodeTypePicked);

	// Determine button styles
	const FComboButtonStyle& OurComboButtonStyle = InArgs._ComboBoxStyle->ComboButtonStyle;
	const FButtonStyle* const OurButtonStyle = InArgs._ButtonStyle ? InArgs._ButtonStyle : &OurComboButtonStyle.ButtonStyle;

	SComboButton::Construct(SComboButton::FArguments()
													.ComboButtonStyle(&OurComboButtonStyle)
													.ButtonStyle(OurButtonStyle)
													.Method(InArgs._Method)
													.ButtonContent()
													[
														ButtonContent.ToSharedRef()
													]
													.MenuContent()
													[
														TypeSelector.ToSharedRef()
													]
													.HasDownArrow(InArgs._HasDownArrow)
													.ContentPadding(InArgs._ContentPadding)
													.ForegroundColor(InArgs._ForegroundColor)
													.IsFocusable(true)
	);

	// Forward focus based on search visibility
	if (InArgs._SearchVisibility.Get() == EVisibility::Visible)
	{
		SetMenuContentWidgetToFocus(TypeSelector->GetWidgetToFocusOnOpen());
	}
}

#undef LOCTEXT_NAMESPACE