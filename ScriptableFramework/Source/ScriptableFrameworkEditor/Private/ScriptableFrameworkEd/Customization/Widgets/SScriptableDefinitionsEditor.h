// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "Misc/NotifyHook.h"
#include "StructUtils/PropertyBag.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class FScopedTransaction;
class IDetailTreeNode;
class IPropertyHandle;
class IPropertyRowGenerator;
class SHeaderRow;
struct FKzNamedVariant;
struct FKzParamDef;
struct FScriptableContainer;

/**
 * Context and Locals of a container in one window, a tab each, as a table of name, type and, for Locals, initial value. Structural edits work on
 * the container directly, each an undoable change of its owners; initial values are edited through a row generator per Local.
 */
class SScriptableDefinitionsEditor : public SCompoundWidget, public FNotifyHook, public FSelfRegisteringEditorUndoClient
{
public:
	enum class ETab : uint8
	{
		Context,
		Locals
	};

	/** An entry of the open tab, or a member of a Local's struct value under it. */
	struct FRow
	{
		bool bEntry = false;

		/** The Local's index, for Locals. */
		int32 LocalIndex = INDEX_NONE;

		/** The entry's name, for Context, whose set has no stable index. */
		FName ContextName;

		/** Owns the value's widgets, for Locals. */
		TSharedPtr<IPropertyRowGenerator> Generator;

		/** Where the value's widgets come from: the Local's value, or a member of it. */
		TSharedPtr<IDetailTreeNode> Node;

		TArray<TSharedPtr<FRow>> Children;
	};

	static const FName NameColumn;
	static const FName TypeColumn;
	static const FName ValueColumn;
	static const FName ActionsColumn;

	SLATE_BEGIN_ARGS(SScriptableDefinitionsEditor)
		: _Tab(ETab::Locals)
		{}
		SLATE_ARGUMENT(ETab, Tab)
	SLATE_END_ARGS()

	/** Opens the window on the container behind the handle at the tab, or brings back the one already open on it. */
	static void Open(const TSharedRef<IPropertyHandle>& InContainerHandle, ETab InTab);

	void Construct(const FArguments& InArgs, const TSharedRef<IPropertyHandle>& InContainerHandle);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void SetTab(ETab InTab);

	/** One cell's widget; an entry's name cell comes without the expander and the drag handle, which its row adds. */
	TSharedRef<SWidget> MakeCell(const TSharedPtr<FRow>& Row, FName Column);

	FText GetEntryName(const TSharedPtr<FRow>& Row) const;

	/** Moves the dragged entry next to the target: below it, or above. Context, a set, is rebuilt in the new order, which it then iterates in. */
	void MoveEntry(const TSharedPtr<FRow>& Source, const TSharedPtr<FRow>& Target, bool bBelow);

	virtual void NotifyPreChange(FProperty* PropertyAboutToChange) override;
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

private:
	/** The container, or null once one of its owners is gone. */
	FScriptableContainer* GetContainer() const;
	FKzNamedVariant* GetLocal(const FRow& Row) const;
	const FKzParamDef* FindContext(FName Name) const;

	void Regenerate();
	TSharedPtr<FRow> MakeLocalRow(int32 Index);
	void AddMemberRows(FRow& Parent, const TSharedRef<IDetailTreeNode>& Node);

	/** Runs a structural edit as one undoable change of the container's owners, then rebuilds the rows. */
	void ApplyChange(const FText& Description, TFunctionRef<void(FScriptableContainer&)> Edit);

	/** Snapshots the owners for undo before a change, and lets them and their nested tasks catch up after it. */
	void BeginChange();
	void EndChange();

	void Rename(const TSharedPtr<FRow>& Row, FName NewName);
	void SetType(const TSharedPtr<FRow>& Row, EPropertyBagPropertyType Type, const UObject* TypeObject, EPropertyBagContainerType ContainerType);
	void Delete(const TSharedPtr<FRow>& Row);
	void AddEntry();

	FText GetTitle() const;

	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FRow> Row, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildren(TSharedPtr<FRow> Row, TArray<TSharedPtr<FRow>>& OutChildren);

	TSharedPtr<IPropertyHandle> ContainerHandle;
	TArray<TWeakObjectPtr<UObject>> Owners;
	FScriptableContainer* Container = nullptr;
	const UScriptStruct* ContainerStruct = nullptr;

	ETab Tab = ETab::Locals;
	TArray<TSharedPtr<FRow>> Rows;
	TSharedPtr<STreeView<TSharedPtr<FRow>>> TreeView;
	TSharedPtr<SHeaderRow> HeaderRow;

	/** A value edit's transaction, when the row generator did not open one. */
	TUniquePtr<FScopedTransaction> ValueTransaction;
	bool bValueChanging = false;
};