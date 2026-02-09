// Copyright 2026 kirzo

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/STreeView.h"
#include "Textures/SlateIcon.h"
#include "UObject/ObjectKey.h"

class UStruct;
class UScriptStruct;
class UClass;
class SSearchBox;

/**
 * Widget that displays a searchable list of Scriptable types (Search box + Tree View).
 * Can be used directly in custom menus or embedded in other widgets.
 */
class SScriptableTypeSelector : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_TwoParams(FOnNodeTypePicked, const UStruct*, const FAssetData&);

	SLATE_BEGIN_ARGS(SScriptableTypeSelector)
		: _MinListWidth(200.0f)
		, _MaxListHeight(450.0f)
		, _ItemStyle(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("ComboBox.Row"))
		, _SearchVisibility(EVisibility::Visible)
		, _CurrentStruct(nullptr)
		, _BaseScriptStruct(nullptr)
		, _BaseClass(nullptr)
		, _ClassCategoryMeta(NAME_None)
		, _FilterCategoryMeta(NAME_None)
		, _Filter()
	{}
		/** The min width of the menu */
		SLATE_ARGUMENT(float, MinListWidth)

		/** The max height of the menu */
		SLATE_ARGUMENT(float, MaxListHeight)

		/** Allow setting the visibility of the search box dynamically */
		SLATE_ATTRIBUTE(EVisibility, SearchVisibility)

		/** Currently selected struct, initially highlighted. */
		SLATE_ARGUMENT(const UStruct*, CurrentStruct)
		/** Base struct of the node */
		SLATE_ARGUMENT(const UScriptStruct*, BaseScriptStruct)
		/** Base class of the node */
		SLATE_ARGUMENT(const UClass*, BaseClass)
		/** Category meta */
		SLATE_ARGUMENT(FName, ClassCategoryMeta)
		/** Category meta */
		SLATE_ARGUMENT(FName, FilterCategoryMeta)
		/** Filter */
		SLATE_ARGUMENT(FString, Filter)
		/** Callback to call when a type is selected. */
		SLATE_ARGUMENT(FOnNodeTypePicked, OnNodeTypePicked)

		/** The item style to use. */
		SLATE_STYLE_ARGUMENT(FTableRowStyle, ItemStyle)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** @returns widget to focus (search box) when the picker is opened. */
	TSharedPtr<SWidget> GetWidgetToFocusOnOpen();

	/** Clears the selection (useful when reopening) */
	void ClearSelection();

private:
	// Stores a category path segment, or a node type.
	struct FScriptableTypeItem
	{
		bool IsCategory() const { return CategoryPath.Num() > 0; }
		FString GetCategoryName() { return CategoryPath.Num() > 0 ? CategoryPath.Last() : FString(); }

		TArray<FString> CategoryPath;
		const UStruct* Struct = nullptr;
		FSlateIcon Icon;
		FSlateColor IconColor;
		TArray<TSharedPtr<FScriptableTypeItem>> Children;
		FAssetData AssetData;
	};

	// Stores per session node expansion state for a node type.
	struct FCategoryExpansionState
	{
		TSet<FString> CollapsedCategories;
	};

	static void SortNodeTypesFunctionItemsRecursive(TArray<TSharedPtr<FScriptableTypeItem>>& Items);
	static TSharedPtr<FScriptableTypeItem> FindOrCreateItemForCategory(TArray<TSharedPtr<FScriptableTypeItem>>& Items, TArrayView<FString> CategoryPath);
	FText GetNodeCategory(const UStruct* Struct);
	void AddNode(const UStruct* Struct);
	void AddNode(const FAssetData& AssetData);

	bool MatchesCategoryPath(const TArray<FString>& CategoryPath);
	bool MatchesFilter(const FAssetData& AssetData);
	bool MatchesFilter(const UStruct* Struct);

	void CacheTypes(const UScriptStruct* BaseScriptStruct, const UClass* BaseClass);

	TSharedRef<ITableRow> GenerateNodeTypeRow(TSharedPtr<FScriptableTypeItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void GetNodeTypeChildren(TSharedPtr<FScriptableTypeItem> Item, TArray<TSharedPtr<FScriptableTypeItem>>& OutItems) const;
	void OnNodeTypeSelected(TSharedPtr<FScriptableTypeItem> SelectedItem, ESelectInfo::Type);
	void OnNodeTypeExpansionChanged(TSharedPtr<FScriptableTypeItem> ExpandedItem, bool bInExpanded);
	void OnSearchBoxTextChanged(const FText& NewText);
	int32 FilterNodeTypesChildren(const TArray<FString>& FilterStrings, const bool bParentMatches, const TArray<TSharedPtr<FScriptableTypeItem>>& SourceArray, TArray<TSharedPtr<FScriptableTypeItem>>& OutDestArray);
	void ExpandAll(const TArray<TSharedPtr<FScriptableTypeItem>>& Items);
	TArray<TSharedPtr<FScriptableTypeItem>> GetPathToItemStruct(const UStruct* Struct) const;

	FReply OnCollapseAllButtonClicked();

	void CollapseAll();
	void RestoreExpansionState();

	FName ClassCategoryMeta;
	FName FilterCategoryMeta;

	TArray<TArray<FString>> FilterPaths;

	TSharedPtr<FScriptableTypeItem> RootNode;
	TSharedPtr<FScriptableTypeItem> FilteredRootNode;
	
	TSharedPtr<SSearchBox> SearchBox;
	TSharedPtr<STreeView<TSharedPtr<FScriptableTypeItem>>> NodeTypeTree;
	bool bIsRestoringExpansion = false;

	FOnNodeTypePicked OnNodeTypePicked;

	/** The item style to use. */
	const FTableRowStyle* ItemStyle;

	/** The padding around each menu row */
	FMargin MenuRowPadding;

	FObjectKey CategoryKey;

	// Save expansion state for each base node type. The expansion state does not persist between editor sessions. 
	static TMap<FObjectKey, FCategoryExpansionState> CategoryExpansionStates;
};

/**
 * Widget that displays a dropdown button to select Scriptable types.
 * Acts as a wrapper around SScriptableTypeSelector using an SComboButton.
 */
class SScriptableTypePicker : public SComboButton
{
public:
	typedef SScriptableTypeSelector::FOnNodeTypePicked FOnNodeTypePicked;

	SLATE_BEGIN_ARGS(SScriptableTypePicker)
		: _Content()
		, _ComboBoxStyle(&FAppStyle::Get().GetWidgetStyle<FComboBoxStyle>("ComboBox"))
		, _ButtonStyle(nullptr)
		, _ItemStyle(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("ComboBox.Row"))
		, _ContentPadding(_ComboBoxStyle->ContentPadding)
		, _ForegroundColor(FSlateColor::UseStyle())
		, _Method()
		, _MinListWidth(200.0f)
		, _MaxListHeight(450.0f)
		, _HasDownArrow(true)
		, _SearchVisibility()
		, _CurrentStruct(nullptr)
		, _BaseScriptStruct(nullptr)
		, _BaseClass(nullptr)
		, _ClassCategoryMeta(NAME_None)
		, _FilterCategoryMeta(NAME_None)
		, _Filter()
	{}

		/** Slot for this button's content (optional) */
		SLATE_DEFAULT_SLOT(FArguments, Content)

		SLATE_STYLE_ARGUMENT(FComboBoxStyle, ComboBoxStyle)

		/** The visual style of the button part of the combo box (overrides ComboBoxStyle) */
		SLATE_STYLE_ARGUMENT(FButtonStyle, ButtonStyle)

		SLATE_STYLE_ARGUMENT(FTableRowStyle, ItemStyle)

		SLATE_ATTRIBUTE(FMargin, ContentPadding)
		SLATE_ATTRIBUTE(FSlateColor, ForegroundColor)

		SLATE_ARGUMENT(TOptional<EPopupMethod>, Method)

		/** The min width of the combo box menu */
		SLATE_ARGUMENT(float, MinListWidth)

		/** The max height of the combo box menu */
		SLATE_ARGUMENT(float, MaxListHeight)

		/**
		 * When false, the down arrow is not generated and it is up to the API consumer
		 * to make their own visual hint that this is a drop down.
		 */
		SLATE_ARGUMENT(bool, HasDownArrow)

		/** Allow setting the visibility of the search box dynamically */
		SLATE_ATTRIBUTE(EVisibility, SearchVisibility)

		/** Currently selected struct, initially highlighted. */
		SLATE_ARGUMENT(const UStruct*, CurrentStruct)
		/** Base struct of the node */
		SLATE_ARGUMENT(const UScriptStruct*, BaseScriptStruct)
		/** Base class of the node */
		SLATE_ARGUMENT(const UClass*, BaseClass)
		/** Category meta */
		SLATE_ARGUMENT(FName, ClassCategoryMeta)
		/** Category meta */
		SLATE_ARGUMENT(FName, FilterCategoryMeta)
		/** Filter */
		SLATE_ARGUMENT(FString, Filter)
		/** Callback to call when a type is selected. */
		SLATE_ARGUMENT(FOnNodeTypePicked, OnNodeTypePicked)
	SLATE_END_ARGS()

	SScriptableTypePicker();
	virtual ~SScriptableTypePicker() override;

	void Construct(const FArguments& InArgs);

private:
	/** The inner content widget containing the tree and search */
	TSharedPtr<SScriptableTypeSelector> TypeSelector;
};