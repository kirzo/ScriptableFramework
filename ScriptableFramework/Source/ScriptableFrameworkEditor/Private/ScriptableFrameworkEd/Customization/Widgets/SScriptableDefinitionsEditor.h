// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "Misc/NotifyHook.h"
#include "ScriptableFrameworkEd/Customization/Widgets/ScriptableDefinitionsSource.h"
#include "StructUtils/PropertyBag.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class FScopedTransaction;
class IDetailTreeNode;
class IPropertyRowGenerator;
class SHeaderRow;
class SWindow;

/**
 * Context, Locals and, for a graph, Outputs of a definitions source in one widget, a tab each, as a table of name, type and, for Locals, initial value. Structural edits
 * work on the source directly, each an undoable change of its owners; initial values are edited through a row generator per Local.
 */
class SScriptableDefinitionsEditor : public SCompoundWidget, public FNotifyHook, public FSelfRegisteringEditorUndoClient
{
public:
	using ETab = EScriptableDefinitionsTab;

	/** An entry of the open tab, or a member of a Local's struct value under it. */
	struct FRow
	{
		bool bEntry = false;

		/** The entry's index, for Locals and Outputs. */
		int32 Index = INDEX_NONE;

		/** The entry's name, for Context, whose entries have no stable index. */
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
	static const FName UsesColumn;
	static const FName ActionsColumn;

	SLATE_BEGIN_ARGS(SScriptableDefinitionsEditor)
		: _Tab(ETab::Locals)
		{}
		SLATE_ARGUMENT(ETab, Tab)
	SLATE_END_ARGS()

	/** Opens a window on the source at the tab, or brings back the one already open on what it edits. */
	static void OpenWindow(const TSharedRef<FScriptableDefinitionsSource>& InSource, ETab InTab);

	void Construct(const FArguments& InArgs, const TSharedRef<FScriptableDefinitionsSource>& InSource);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void SetTab(ETab InTab);

	/** One cell's widget; an entry's name cell comes without the expander and the drag handle, which its row adds. */
	TSharedRef<SWidget> MakeCell(const TSharedPtr<FRow>& Row, FName Column);

	FText GetEntryName(const TSharedPtr<FRow>& Row) const;

	/** Moves the dragged entry next to the target: below it, or above. */
	void MoveEntry(const TSharedPtr<FRow>& SourceRow, const TSharedPtr<FRow>& TargetRow, bool bBelow);

	virtual void NotifyPreChange(FProperty* PropertyAboutToChange) override;
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

private:
	FKzNamedVariant* GetLocal(const FRow& Row) const;
	FName* GetOutput(const FRow& Row) const;
	TOptional<FKzParamDef> FindContext(FName Name) const;
	FName GetRowName(const TSharedPtr<FRow>& Row) const;

	/** Why the row can't take the name, or empty when it can: none, invalid characters, or another entry of the tab already has it. */
	FText GetNameError(const TSharedPtr<FRow>& Row, FName Name) const;

	void Regenerate();
	TSharedPtr<FRow> MakeLocalRow(int32 Index);
	void AddMemberRows(FRow& Parent, const TSharedRef<IDetailTreeNode>& Node);

	/** Runs a structural edit of the open tab as one undoable change of the source's owners, then rebuilds the rows. */
	void ApplyChange(const FText& Description, TFunctionRef<void()> Edit);

	/** Rewrites the Context entries through a copy, the source keeping its own storage. */
	void EditContext(TFunctionRef<void(TArray<FKzParamDef>&)> Edit);

	void Rename(const TSharedPtr<FRow>& Row, FName NewName);
	void SetType(const TSharedPtr<FRow>& Row, EPropertyBagPropertyType Type, const UObject* TypeObject, EPropertyBagContainerType ContainerType);
	void Delete(const TSharedPtr<FRow>& Row);
	void AddEntry();

	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FRow> Row, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildren(TSharedPtr<FRow> Row, TArray<TSharedPtr<FRow>>& OutChildren);

	TSharedPtr<FScriptableDefinitionsSource> Source;

	/** The window opened for this editor, closed once the source is gone. */
	TWeakPtr<SWindow> OwnWindow;

	ETab Tab = ETab::Locals;
	TArray<TSharedPtr<FRow>> Rows;
	TSharedPtr<STreeView<TSharedPtr<FRow>>> TreeView;
	TSharedPtr<SHeaderRow> HeaderRow;

	/** Column fill widths, remembered across windows. */
	float NameFill = 0.35f;
	float TypeFill = 0.25f;
	float ValueFill = 0.4f;

	/** A value edit's transaction, when the row generator did not open one. */
	TUniquePtr<FScopedTransaction> ValueTransaction;
	bool bValueChanging = false;
};