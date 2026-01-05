// Copyright 2025 kirzo

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
#include "ScriptableTasks/ScriptableTaskAsset.h"
#include "ScriptableConditions/ScriptableCondition.h"
#include "ScriptableConditions/ScriptableConditionAsset.h"

#define LOCTEXT_NAMESPACE "ScriptableFrameworkEditor"

TMap<FObjectKey, SScriptableTypePicker::FCategoryExpansionState> SScriptableTypePicker::CategoryExpansionStates;

SScriptableTypePicker::SScriptableTypePicker()
{
}

SScriptableTypePicker::~SScriptableTypePicker()
{
	
}

void SScriptableTypePicker::Construct(const SScriptableTypePicker::FArguments& InArgs)
{
	check(InArgs._ComboBoxStyle);

	ItemStyle = InArgs._ItemStyle;
	MenuRowPadding = InArgs._ComboBoxStyle->MenuRowPadding;

	TAttribute<EVisibility> SearchVisibility = InArgs._SearchVisibility;
	const EVisibility CurrentSearchVisibility = SearchVisibility.Get();

	// Work out which values we should use based on whether we were given an override, or should use the style's version
	const FComboButtonStyle& OurComboButtonStyle = InArgs._ComboBoxStyle->ComboButtonStyle;
	const FButtonStyle* const OurButtonStyle = InArgs._ButtonStyle ? InArgs._ButtonStyle : &OurComboButtonStyle.ButtonStyle;

	OnNodeTypePicked = InArgs._OnNodeTypePicked;
	CategoryKey = FObjectKey(InArgs._BaseScriptStruct);

	ClassCategoryMeta = InArgs._ClassCategoryMeta;
	FilterCategoryMeta = InArgs._FilterCategoryMeta;

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
		.OnGenerateRow(this, &SScriptableTypePicker::GenerateNodeTypeRow)
		.OnGetChildren(this, &SScriptableTypePicker::GetNodeTypeChildren)
		.OnSelectionChanged(this, &SScriptableTypePicker::OnNodeTypeSelected)
		.OnExpansionChanged(this, &SScriptableTypePicker::OnNodeTypeExpansionChanged);
	
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

	TSharedRef<SWidget> ComboBoxMenuContent =
		SNew(SBox)
		.MaxDesiredHeight(InArgs._MaxListHeight)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
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
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
					.ToolTipText_Lambda([]() -> FText
					{
						return LOCTEXT("CollapseAllCategories", "Collapse All");
					})
					.OnClicked(this, &SScriptableTypePicker::OnCollapseAllButtonClicked)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("SpinBox.Arrows"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]

				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Top)
				.AutoWidth()
				[
					SAssignNew(SearchBox, SSearchBox)
						.OnTextChanged(this, &SScriptableTypePicker::OnSearchBoxTextChanged)
						.Visibility(SearchVisibility)
				]
			]

			+ SVerticalBox::Slot()
			[
				NodeTypeTree.ToSharedRef()
			]
		];

	// Set up content
	TSharedPtr<SWidget> ButtonContent = InArgs._Content.Widget;
	if (InArgs._Content.Widget == SNullWidget::NullWidget)
	{
		SAssignNew(ButtonContent, STextBlock)
			.Text(NSLOCTEXT("SScriptableTypePicker", "ContentWarning", "No Content Provided"))
			.ColorAndOpacity(FLinearColor::Red);
	}

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
			ComboBoxMenuContent
		]
		.HasDownArrow(InArgs._HasDownArrow)
		.ContentPadding(InArgs._ContentPadding)
		.ForegroundColor(InArgs._ForegroundColor)
		.IsFocusable(true)
	);

	if (CurrentSearchVisibility == EVisibility::Visible)
	{
		SetMenuContentWidgetToFocus(SearchBox);
	}
	else
	{
		SetMenuContentWidgetToFocus(NodeTypeTree);
	}
}

TSharedPtr<SWidget> SScriptableTypePicker::GetWidgetToFocusOnOpen()
{
	return SearchBox;
}

void SScriptableTypePicker::SortNodeTypesFunctionItemsRecursive(TArray<TSharedPtr<FScriptableTypeItem>>& Items)
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

TSharedPtr<SScriptableTypePicker::FScriptableTypeItem> SScriptableTypePicker::FindOrCreateItemForCategory(TArray<TSharedPtr<FScriptableTypeItem>>& Items, TArrayView<FString> CategoryPath)
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

void SScriptableTypePicker::AddNode(const UStruct* Struct)
{
	if (!Struct || !RootNode.IsValid())
	{
		return;
	}

	const FText CategoryName = Struct->GetMetaDataText(ClassCategoryMeta);

	TSharedPtr<FScriptableTypeItem> ParentItem = RootNode;

	if (!CategoryName.IsEmpty())
	{
		// Split into subcategories and trim
		TArray<FString> CategoryPath;
		CategoryName.ToString().ParseIntoArray(CategoryPath, TEXT("|"));
		for (FString& SubCategory : CategoryPath)
		{
			SubCategory.TrimStartAndEndInline();
		}

		// Create items for the entire category path
		// eg. "Math|Boolean|AND" 
		// Math 
		//   > Boolean
		//     > AND
		for (int32 PathIndex = 0; PathIndex < CategoryPath.Num(); ++PathIndex)
		{
			ParentItem = FindOrCreateItemForCategory(ParentItem->Children, MakeArrayView(CategoryPath.GetData(), PathIndex + 1));
		}
	}
	check(ParentItem);

	const TSharedPtr<FScriptableTypeItem>& Item = ParentItem->Children.Add_GetRef(MakeShared<FScriptableTypeItem>());
	Item->Struct = Struct;
	/*if (!IconName.IsNone())
	{
		Item->Icon = UE::StateTreeEditor::EditorNodeUtils::ParseIcon(IconName);
	}*/
	Item->IconColor = FLinearColor::Gray;
}

bool SScriptableTypePicker::MatchesFilter(const UStruct* Struct)
{
	if (!Struct || !RootNode.IsValid())
	{
		return false;
	}

	if (FilterPaths.IsEmpty())
	{
		return true;
	}

	const FText CategoryName = Struct->GetMetaDataText(ClassCategoryMeta);

	TSharedPtr<FScriptableTypeItem> ParentItem = RootNode;

	if (CategoryName.IsEmpty())
	{
		return false;
	}

	// Split into subcategories and trim
	TArray<FString> CategoryPath;
	CategoryName.ToString().ParseIntoArray(CategoryPath, TEXT("|"));
	for (FString& SubCategory : CategoryPath)
	{
		SubCategory.TrimStartAndEndInline();
	}

	// Always show 'System' category
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

void SScriptableTypePicker::CacheTypes(const UScriptStruct* BaseScriptStruct, const UClass* BaseClass)
{
	// Get all usable types from the class cache.
	FScriptableFrameworkEditorModule& EditorModule = FModuleManager::GetModuleChecked<FScriptableFrameworkEditorModule>(TEXT("ScriptableFrameworkEditor"));
	FScriptableTypeCache* TypeCache = EditorModule.GetScriptableTypeCache().Get();
	check(TypeCache);

	TArray<TSharedPtr<FScriptableTypeData>> StructNodes;
	TArray<TSharedPtr<FScriptableTypeData>> ObjectNodes;

	TypeCache->GetScripStructs(BaseScriptStruct, StructNodes);
	TypeCache->GetClasses(BaseClass, ObjectNodes);

	// Create tree of node types based on category.
	RootNode = MakeShared<FScriptableTypeItem>();

	for (const TSharedPtr<FScriptableTypeData>& Data : StructNodes)
	{
		if (const UScriptStruct* ScriptStruct = Data->GetScriptStruct())
		{
			if (ScriptStruct == BaseScriptStruct)
			{
				continue;
			}
			if (ScriptStruct->HasMetaData(TEXT("Hidden")))
			{
				continue;
			}
			if (!MatchesFilter(ScriptStruct))
			{
				continue;
			}

			AddNode(ScriptStruct);
		}
	}

	for (const TSharedPtr<FScriptableTypeData>& Data : ObjectNodes)
	{
		if (Data->GetClass() != nullptr)
		{
			const UClass* Class = Data->GetClass();
			if (Class == BaseClass)
			{
				continue;
			}
			if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Hidden | CLASS_HideDropDown))
			{
				continue;
			}
			if (Class->HasMetaData(TEXT("Hidden")))
			{
				continue;
			}
			if (!MatchesFilter(Class))
			{
				continue;
			}

			AddNode(Class);
		}
	}

	// Determine which asset class to search for based on the picker's base class.
	UClass* AssetClassToSearch = nullptr;

	if (BaseClass && BaseClass->IsChildOf(UScriptableTask::StaticClass()))
	{
		AssetClassToSearch = UScriptableTaskAsset::StaticClass();
	}
	else if (BaseClass && BaseClass->IsChildOf(UScriptableCondition::StaticClass()))
	{
		AssetClassToSearch = UScriptableConditionAsset::StaticClass();
	}

	if (AssetClassToSearch)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> AssetDataList;

		FARFilter Filter;
		Filter.ClassPaths.Add(AssetClassToSearch->GetClassPathName());
		Filter.bRecursivePaths = true;

		// Filter by path if you want to limit the search scope (e.g. only inside /Game).
		// Filter.PackagePaths.Add("/Game"); 

		AssetRegistryModule.Get().GetAssets(Filter, AssetDataList);

		// Add found assets to the tree.
		for (const FAssetData& Asset : AssetDataList)
		{
			// Create a virtual root category "Project Assets" to group them.
			TArray<FString> CategoryPath;
			CategoryPath.Add(TEXT("Project Assets"));

			// Use the asset's folder path as subcategories
			FString PathStr = Asset.PackagePath.ToString();

			// Remove the "/Game/" prefix to avoid redundant top-level categories.
			if (PathStr.StartsWith(TEXT("/Game/")))
			{
				PathStr.RemoveFromStart(TEXT("/Game/"));
			}

			// Split the path into the category array (e.g., "Folder/SubFolder" -> ["Folder", "SubFolder"]).
			if (!PathStr.IsEmpty())
			{
				PathStr.ParseIntoArray(CategoryPath, TEXT("/"), true);
			}

			// Find or create the category structure in the tree.
			TSharedPtr<FScriptableTypeItem> ParentItem = RootNode;
			for (int32 i = 0; i < CategoryPath.Num(); ++i)
			{
				// Note: using MakeArrayView with index to pass a valid mutable view to the helper function.
				ParentItem = FindOrCreateItemForCategory(ParentItem->Children, MakeArrayView(CategoryPath.GetData(), i + 1));
			}

			// Add the leaf node (the actual Asset).
			TSharedPtr<FScriptableTypeItem> NewItem = ParentItem->Children.Add_GetRef(MakeShared<FScriptableTypeItem>());
			NewItem->AssetData = Asset;

			FName IconName = NAME_None;

			if (AssetClassToSearch->IsChildOf(UScriptableTaskAsset::StaticClass()))
			{
				IconName = "ClassIcon.ScriptableTaskAsset";
			}
			else if (AssetClassToSearch->IsChildOf(UScriptableConditionAsset::StaticClass()))
			{
				IconName = "ClassIcon.ScriptableConditionAsset";
			}

			if (!IconName.IsNone())
			{
				NewItem->Icon = FSlateIcon(FScriptableFrameworkEditorStyle::Get().GetStyleSetName(), IconName);
				NewItem->IconColor = FLinearColor::White;
			}
			else
			{
				NewItem->Icon = FSlateIconFinder::FindIconForClass(AssetClassToSearch);
				NewItem->IconColor = FLinearColor::White;
			}
		}
	}

	SortNodeTypesFunctionItemsRecursive(RootNode->Children);

	FilteredRootNode = RootNode;
}

TSharedRef<ITableRow> SScriptableTypePicker::GenerateNodeTypeRow(TSharedPtr<FScriptableTypeItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
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

	TSharedRef<STableRow<TSharedPtr<FScriptableTypeItem>>> Row = SNew(STableRow<TSharedPtr<FScriptableTypeItem>>, OwnerTable)
		.Style(ItemStyle)
		.Padding(MenuRowPadding);
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

void SScriptableTypePicker::GetNodeTypeChildren(TSharedPtr<FScriptableTypeItem> Item, TArray<TSharedPtr<FScriptableTypeItem>>& OutItems) const
{
	if (Item.IsValid())
	{
		OutItems = Item->Children;
	}
}

void SScriptableTypePicker::OnNodeTypeSelected(TSharedPtr<FScriptableTypeItem> SelectedItem, ESelectInfo::Type Type)
{
	// Skip selection set via code, or if Selected Item is invalid
	if (Type == ESelectInfo::Direct || !SelectedItem.IsValid())
	{
		return;
	}

	if (!SelectedItem->IsCategory() && OnNodeTypePicked.IsBound())
	{
		OnNodeTypePicked.Execute(SelectedItem->Struct, SelectedItem->AssetData);
	}
}

void SScriptableTypePicker::OnNodeTypeExpansionChanged(TSharedPtr<FScriptableTypeItem> ExpandedItem, bool bInExpanded)
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

void SScriptableTypePicker::OnSearchBoxTextChanged(const FText& NewText)
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
}

int32 SScriptableTypePicker::FilterNodeTypesChildren(const TArray<FString>& FilterStrings, const bool bParentMatches, const TArray<TSharedPtr<FScriptableTypeItem>>& SourceArray, TArray<TSharedPtr<FScriptableTypeItem>>& OutDestArray)
{
	int32 NumFound = 0;

	auto MatchFilter = [&FilterStrings](const TSharedPtr<FScriptableTypeItem>& SourceItem)
	{
		const FString ItemName = SourceItem->Struct ? SourceItem->Struct->GetDisplayNameText().ToString() : SourceItem->GetCategoryName();
		for (const FString& Filter : FilterStrings)
		{
			if (ItemName.Contains(Filter))
			{
				return true;
			}
		}
		return false;
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
			NewItem->Children = FilteredChildren;

			NumFound += NumChildren;
		}
	}

	return NumFound;
}

TArray<TSharedPtr<SScriptableTypePicker::FScriptableTypeItem>> SScriptableTypePicker::GetPathToItemStruct(const UStruct* Struct) const
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

FReply SScriptableTypePicker::OnCollapseAllButtonClicked()
{
	CollapseAll();
	return FReply::Handled();;
}

void SScriptableTypePicker::ExpandAll(const TArray<TSharedPtr<FScriptableTypeItem>>& Items)
{
	for (const TSharedPtr<FScriptableTypeItem>& Item : Items)
	{
		NodeTypeTree->SetItemExpansion(Item, true);
		ExpandAll(Item->Children);
	}
}

void SScriptableTypePicker::CollapseAll()
{
	FCategoryExpansionState& ExpansionState = CategoryExpansionStates.FindOrAdd(CategoryKey);

	for (const TSharedPtr<FScriptableTypeItem> Item : NodeTypeTree->GetRootItems())
	{
		const FString Path = FString::Join(Item->CategoryPath, TEXT("|"));
		ExpansionState.CollapsedCategories.Add(Path);
	}

	RestoreExpansionState();
}

void SScriptableTypePicker::RestoreExpansionState()
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

#undef LOCTEXT_NAMESPACE