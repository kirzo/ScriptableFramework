// Copyright 2026 kirzo

#include "ScriptableFrameworkEd/Customization/Widgets/SScriptableDefinitionsEditor.h"
#include "Widgets/SKzTypeSelector.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "Input/DragAndDrop.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "UObject/StructOnScope.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SExpanderArrow.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SScriptableDefinitionsEditor"

const FName SScriptableDefinitionsEditor::NameColumn(TEXT("Name"));
const FName SScriptableDefinitionsEditor::TypeColumn(TEXT("Type"));
const FName SScriptableDefinitionsEditor::ValueColumn(TEXT("Value"));
const FName SScriptableDefinitionsEditor::UsesColumn(TEXT("Uses"));
const FName SScriptableDefinitionsEditor::ActionsColumn(TEXT("Actions"));

namespace ScriptableDefinitionsEditor
{
	const TCHAR* const ConfigSection = TEXT("ScriptableFramework.DefinitionsWindow");

	/** The window open on each source key, so its button brings it back instead of opening another. */
	TMap<const void*, TWeakPtr<SScriptableDefinitionsEditor>> OpenEditors;

	/** The first property row under the nodes, depth first: a bag struct's single value. */
	TSharedPtr<IDetailTreeNode> FindPropertyNode(const TArray<TSharedRef<IDetailTreeNode>>& Nodes)
	{
		for (const TSharedRef<IDetailTreeNode>& Node : Nodes)
		{
			if (Node->GetNodeType() == EDetailNodeType::Item && Node->CreatePropertyHandle().IsValid())
			{
				return Node;
			}

			TArray<TSharedRef<IDetailTreeNode>> Children;
			Node->GetChildren(Children);
			if (const TSharedPtr<IDetailTreeNode> Found = FindPropertyNode(Children))
			{
				return Found;
			}
		}
		return nullptr;
	}

	FText ByTab(EScriptableDefinitionsTab Tab, const FText& Context, const FText& Locals, const FText& Outputs)
	{
		return Tab == EScriptableDefinitionsTab::Context ? Context : Tab == EScriptableDefinitionsTab::Locals ? Locals : Outputs;
	}

	FName ToName(const FText& Text)
	{
		return FName(*Text.ToString().TrimStartAndEnd());
	}

	void SaveFill(const TCHAR* Key, float Fill)
	{
		GConfig->SetFloat(ConfigSection, Key, Fill, GEditorPerProjectIni);
	}

	/** Base, then Base1, Base2... until no entry uses it. */
	FName MakeUniqueName(const FString& Base, TFunctionRef<bool(FName)> IsUsed)
	{
		FName Name(*Base);
		for (int32 Suffix = 1; IsUsed(Name); ++Suffix)
		{
			Name = FName(*FString::Printf(TEXT("%s%d"), *Base, Suffix));
		}
		return Name;
	}

	/** An entry being dragged to another place in its list. */
	class FEntryDragDropOp : public FDragDropOperation
	{
	public:
		DRAG_DROP_OPERATOR_TYPE(FEntryDragDropOp, FDragDropOperation)

		TSharedPtr<SScriptableDefinitionsEditor::FRow> Source;
		FText Label;

		static TSharedRef<FEntryDragDropOp> New(const TSharedPtr<SScriptableDefinitionsEditor::FRow>& InSource, const FText& InLabel)
		{
			const TSharedRef<FEntryDragDropOp> Operation = MakeShared<FEntryDragDropOp>();
			Operation->Source = InSource;
			Operation->Label = InLabel;
			Operation->Construct();
			return Operation;
		}

		virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
		{
			return SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Menu.Background"))
				.Padding(FMargin(8.0f, 4.0f))
				[
					SNew(STextBlock).Text(Label)
				];
		}
	};

	/** A table row; its name cell gets the expander that shows a struct value's members and, on entries, the handle to drag them by. */
	class SDefinitionRow : public SMultiColumnTableRow<TSharedPtr<SScriptableDefinitionsEditor::FRow>>
	{
	public:
		SLATE_BEGIN_ARGS(SDefinitionRow) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, const TSharedPtr<SScriptableDefinitionsEditor::FRow>& InRow, const TSharedRef<SScriptableDefinitionsEditor>& InEditor)
		{
			Row = InRow;
			Editor = InEditor;
			FSuperRowType::Construct(FSuperRowType::FArguments()
				.Padding(FMargin(0.0f, 2.0f))
				.OnDragDetected(this, &SDefinitionRow::HandleDragDetected)
				.OnCanAcceptDrop(this, &SDefinitionRow::HandleCanAcceptDrop)
				.OnAcceptDrop(this, &SDefinitionRow::HandleAcceptDrop), OwnerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column) override
		{
			const TSharedPtr<SScriptableDefinitionsEditor> PinnedEditor = Editor.Pin();
			if (!PinnedEditor.IsValid())
			{
				return SNullWidget::NullWidget;
			}

			const TSharedRef<SWidget> Cell = PinnedEditor->MakeCell(Row, Column);
			if (Column != SScriptableDefinitionsEditor::NameColumn)
			{
				return Cell;
			}

			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SExpanderArrow, SharedThis(this))
					.IndentAmount(12.0f)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SBorder)
					.Visibility(Row->bEntry ? EVisibility::Visible : EVisibility::Collapsed)
					.BorderImage(FAppStyle::GetNoBrush())
					.Padding(FMargin(2.0f, 0.0f))
					.Cursor(EMouseCursor::GrabHand)
					.ToolTipText(LOCTEXT("DragToReorder", "Drag to reorder"))
					.OnMouseButtonDown(this, &SDefinitionRow::HandleGripMouseDown)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("VerticalBoxDragIndicatorShort"))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					Cell
				];
		}

	private:
		/** With selection off the row ignores clicks, so the handle starts the drag detection itself. */
		FReply HandleGripMouseDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
		{
			if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
			{
				return FReply::Unhandled();
			}
			return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
		}

		FReply HandleDragDetected(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
		{
			const TSharedPtr<SScriptableDefinitionsEditor> PinnedEditor = Editor.Pin();
			if (!PinnedEditor.IsValid() || !Row->bEntry)
			{
				return FReply::Unhandled();
			}
			return FReply::Handled().BeginDragDrop(FEntryDragDropOp::New(Row, PinnedEditor->GetEntryName(Row)));
		}

		TOptional<EItemDropZone> HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<SScriptableDefinitionsEditor::FRow> TargetRow)
		{
			if (!DragDropEvent.GetOperationAs<FEntryDragDropOp>().IsValid() || !TargetRow->bEntry)
			{
				return TOptional<EItemDropZone>();
			}
			return DropZone == EItemDropZone::OntoItem ? EItemDropZone::BelowItem : DropZone;
		}

		FReply HandleAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<SScriptableDefinitionsEditor::FRow> TargetRow)
		{
			const TSharedPtr<FEntryDragDropOp> Operation = DragDropEvent.GetOperationAs<FEntryDragDropOp>();
			const TSharedPtr<SScriptableDefinitionsEditor> PinnedEditor = Editor.Pin();
			if (!Operation.IsValid() || !PinnedEditor.IsValid() || !TargetRow->bEntry)
			{
				return FReply::Unhandled();
			}
			PinnedEditor->MoveEntry(Operation->Source, TargetRow, DropZone == EItemDropZone::BelowItem);
			return FReply::Handled();
		}

		TSharedPtr<SScriptableDefinitionsEditor::FRow> Row;
		TWeakPtr<SScriptableDefinitionsEditor> Editor;
	};
}

void SScriptableDefinitionsEditor::OpenWindow(const TSharedRef<FScriptableDefinitionsSource>& InSource, ETab InTab)
{
	if (!InSource->IsValid())
	{
		return;
	}

	const void* Key = InSource->GetKey();
	if (const TSharedPtr<SScriptableDefinitionsEditor> Existing = ScriptableDefinitionsEditor::OpenEditors.FindRef(Key).Pin())
	{
		Existing->SetTab(InTab);
		if (const TSharedPtr<SWindow> ExistingWindow = FSlateApplication::Get().FindWidgetWindow(Existing.ToSharedRef()))
		{
			ExistingWindow->BringToFront();
		}
		return;
	}

	const TSharedRef<SScriptableDefinitionsEditor> Editor = SNew(SScriptableDefinitionsEditor, InSource).Tab(InTab);
	ScriptableDefinitionsEditor::OpenEditors.Add(Key, Editor);

	FVector2D Size(640.0, 440.0);
	GConfig->GetVector2D(ScriptableDefinitionsEditor::ConfigSection, TEXT("Size"), Size, GEditorPerProjectIni);

	const TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(InSource->GetTitle())
		.ClientSize(Size)
		.MinWidth(420.0f)
		.MinHeight(260.0f)
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		[
			Editor
		];

	Window->SetOnWindowClosed(FOnWindowClosed::CreateLambda([Key](const TSharedRef<SWindow>& ClosedWindow)
	{
		const FVector2D ClosedSize = FVector2D(ClosedWindow->GetClientSizeInScreen()) / ClosedWindow->GetDPIScaleFactor();
		GConfig->SetVector2D(ScriptableDefinitionsEditor::ConfigSection, TEXT("Size"), ClosedSize, GEditorPerProjectIni);
		ScriptableDefinitionsEditor::OpenEditors.Remove(Key);
	}));
	Editor->OwnWindow = Window;

	// A child of the editor's main window, so it stays above it.
	if (const TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(Window, RootWindow.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(Window);
	}
}

void SScriptableDefinitionsEditor::Construct(const FArguments& InArgs, const TSharedRef<FScriptableDefinitionsSource>& InSource)
{
	Source = InSource;
	Tab = InArgs._Tab;

	GConfig->GetFloat(ScriptableDefinitionsEditor::ConfigSection, TEXT("NameFill"), NameFill, GEditorPerProjectIni);
	GConfig->GetFloat(ScriptableDefinitionsEditor::ConfigSection, TEXT("TypeFill"), TypeFill, GEditorPerProjectIni);
	GConfig->GetFloat(ScriptableDefinitionsEditor::ConfigSection, TEXT("ValueFill"), ValueFill, GEditorPerProjectIni);

	// With OnWidthChanged bound the header stops keeping widths itself; they come back through FillWidth.
	SAssignNew(HeaderRow, SHeaderRow)
		+ SHeaderRow::Column(NameColumn)
		.DefaultLabel(LOCTEXT("NameColumn", "Name"))
		.FillWidth_Lambda([this]() { return NameFill; })
		.OnWidthChanged_Lambda([this](float Fill) { NameFill = Fill; ScriptableDefinitionsEditor::SaveFill(TEXT("NameFill"), Fill); })
		+ SHeaderRow::Column(TypeColumn)
		.DefaultLabel(LOCTEXT("TypeColumn", "Type"))
		.FillWidth_Lambda([this]() { return TypeFill; })
		.OnWidthChanged_Lambda([this](float Fill) { TypeFill = Fill; ScriptableDefinitionsEditor::SaveFill(TEXT("TypeFill"), Fill); })
		.ShouldGenerateWidget_Lambda([this]() { return Tab != ETab::Outputs; })
		+ SHeaderRow::Column(ValueColumn)
		.DefaultLabel(LOCTEXT("ValueColumn", "Initial Value"))
		.FillWidth_Lambda([this]() { return ValueFill; })
		.OnWidthChanged_Lambda([this](float Fill) { ValueFill = Fill; ScriptableDefinitionsEditor::SaveFill(TEXT("ValueFill"), Fill); })
		.ShouldGenerateWidget_Lambda([this]() { return Tab == ETab::Locals; })
		+ SHeaderRow::Column(UsesColumn)
		.DefaultLabel(LOCTEXT("UsesColumn", "Uses"))
		.DefaultTooltip_Lambda([this]()
		{
			return Tab == ETab::Outputs ? LOCTEXT("OutputUsesTooltip", "Finish nodes that pick the output, and wires from its Exit pin") : LOCTEXT("UsesColumnTooltip", "Bindings and tasks that read the entry");
		})
		.FixedWidth(48.0f)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Center)
		.VAlignCell(VAlign_Center)
		+ SHeaderRow::Column(ActionsColumn)
		.DefaultLabel(FText::GetEmpty())
		.FixedWidth(32.0f);

	TSharedPtr<SSegmentedControl<ETab>> TabControl;
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
		.Padding(FMargin(12.0f, 10.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			[
				SAssignNew(TabControl, SSegmentedControl<ETab>)
				.Value_Lambda([this]() { return Tab; })
				.OnValueChanged(this, &SScriptableDefinitionsEditor::SetTab)
				+ SSegmentedControl<ETab>::Slot(ETab::Context)
				.Text_Lambda([this]()
				{
					return FText::Format(LOCTEXT("ContextTab", "Context ({0})"), Source->GetContext().Num());
				})
				+ SSegmentedControl<ETab>::Slot(ETab::Locals)
				.Text_Lambda([this]()
				{
					const TArray<FKzNamedVariant>* Locals = Source->GetLocals();
					return FText::Format(LOCTEXT("LocalsTab", "Locals ({0})"), Locals ? Locals->Num() : 0);
				})
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 10.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Text_Lambda([this]()
				{
					return ScriptableDefinitionsEditor::ByTab(Tab,
						LOCTEXT("ContextDescription", "What the runner passes in each time it runs this. Tasks read it through bindings; nothing here writes it."),
						LOCTEXT("LocalsDescription", "State each run starts with, at these values. Set Local changes it; tasks read it through bindings."),
						LOCTEXT("OutputsDescription", "Ways the graph can end besides Finished and Cancelled. A Finish node picks one; the Exit node and every node running this graph get a pin for each."));
				})
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
				.Padding(0.0f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SAssignNew(TreeView, STreeView<TSharedPtr<FRow>>)
						.TreeItemsSource(&Rows)
						.OnGenerateRow(this, &SScriptableDefinitionsEditor::OnGenerateRow)
						.OnGetChildren(this, &SScriptableDefinitionsEditor::OnGetChildren)
						.SelectionMode(ESelectionMode::None)
						.HeaderRow(HeaderRow)
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Visibility_Lambda([this]() { return Rows.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.Text_Lambda([this]() { return ScriptableDefinitionsEditor::ByTab(Tab, LOCTEXT("NoContext", "No context entries yet."), LOCTEXT("NoLocals", "No locals yet."), LOCTEXT("NoOutputs", "No outputs yet.")); })
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked_Lambda([this]()
				{
					AddEntry();
					return FReply::Handled();
				})
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.PlusCircle"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text_Lambda([this]() { return ScriptableDefinitionsEditor::ByTab(Tab, LOCTEXT("AddContext", "Add Context Entry"), LOCTEXT("AddLocal", "Add Local"), LOCTEXT("AddOutput", "Add Output")); })
					]
				]
			]
		]
	];

	if (Source->GetOutputs())
	{
		TabControl->AddSlot(ETab::Outputs)
			.Text_Lambda([this]()
			{
				const TArray<FName>* Outputs = Source->GetOutputs();
				return FText::Format(LOCTEXT("OutputsTab", "Outputs ({0})"), Outputs ? Outputs->Num() : 0);
			});
	}

	Regenerate();
}

void SScriptableDefinitionsEditor::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// An owner gone, as a Blueprint compile reinstancing it, takes the edited memory with it.
	const TSharedPtr<SWindow> Window = OwnWindow.Pin();
	if (Window.IsValid() && !Source->IsValid())
	{
		Window->RequestDestroyWindow();
	}
}

void SScriptableDefinitionsEditor::SetTab(ETab InTab)
{
	Tab = InTab;
	HeaderRow->RefreshColumns();
	Regenerate();
}

FKzNamedVariant* SScriptableDefinitionsEditor::GetLocal(const FRow& Row) const
{
	TArray<FKzNamedVariant>* Locals = Source->GetLocals();
	return Locals && Locals->IsValidIndex(Row.Index) ? &(*Locals)[Row.Index] : nullptr;
}

FName* SScriptableDefinitionsEditor::GetOutput(const FRow& Row) const
{
	TArray<FName>* Outputs = Source->GetOutputs();
	return Outputs && Outputs->IsValidIndex(Row.Index) ? &(*Outputs)[Row.Index] : nullptr;
}

TOptional<FKzParamDef> SScriptableDefinitionsEditor::FindContext(FName Name) const
{
	for (const FKzParamDef& Def : Source->GetContext())
	{
		if (Def.Name == Name)
		{
			return Def;
		}
	}
	return {};
}

void SScriptableDefinitionsEditor::Regenerate()
{
	Rows.Reset();
	if (Source->IsValid())
	{
		if (Tab == ETab::Locals)
		{
			for (int32 Index = 0; Index < Source->GetLocals()->Num(); ++Index)
			{
				Rows.Add(MakeLocalRow(Index));
			}
		}
		else if (Tab == ETab::Outputs)
		{
			const TArray<FName>* Outputs = Source->GetOutputs();
			for (int32 Index = 0; Outputs && Index < Outputs->Num(); ++Index)
			{
				const TSharedPtr<FRow> Row = MakeShared<FRow>();
				Row->bEntry = true;
				Row->Index = Index;
				Rows.Add(Row);
			}
		}
		else
		{
			for (const FKzParamDef& Def : Source->GetContext())
			{
				const TSharedPtr<FRow> Row = MakeShared<FRow>();
				Row->bEntry = true;
				Row->ContextName = Def.Name;
				Rows.Add(Row);
			}
		}
	}

	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
	}
}

TSharedPtr<SScriptableDefinitionsEditor::FRow> SScriptableDefinitionsEditor::MakeLocalRow(int32 Index)
{
	const TSharedPtr<FRow> Row = MakeShared<FRow>();
	Row->bEntry = true;
	Row->Index = Index;

	const FKzVariant& Value = (*Source->GetLocals())[Index].GetValue();
	if (!Value.IsValid())
	{
		return Row;
	}

	// The value is edited in place through its bag struct; this editor hears of the edits to notify the owners.
	FPropertyRowGeneratorArgs Args;
	Args.NotifyHook = this;
	Row->Generator = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreatePropertyRowGenerator(Args);

	const TSharedPtr<FStructOnScope> ValueStruct = MakeShared<FStructOnScope>(Value.GetBagStruct(), Value.GetBagMemory());
	ValueStruct->SetPackage(Source->GetPackage());
	Row->Generator->SetStructure(ValueStruct);

	Row->Node = ScriptableDefinitionsEditor::FindPropertyNode(Row->Generator->GetRootTreeNodes());
	if (Row->Node.IsValid())
	{
		AddMemberRows(*Row, Row->Node.ToSharedRef());
	}
	return Row;
}

void SScriptableDefinitionsEditor::AddMemberRows(FRow& Parent, const TSharedRef<IDetailTreeNode>& Node)
{
	TArray<TSharedRef<IDetailTreeNode>> Children;
	Node->GetChildren(Children);
	for (const TSharedRef<IDetailTreeNode>& Child : Children)
	{
		const TSharedPtr<FRow> Row = MakeShared<FRow>();
		Row->Node = Child;
		AddMemberRows(*Row, Child);
		Parent.Children.Add(Row);
	}
}

TSharedRef<SWidget> SScriptableDefinitionsEditor::MakeCell(const TSharedPtr<FRow>& Row, FName Column)
{
	// Value widgets come from the row generator, for a Local's value and the members under it.
	auto MakeNodeCell = [](const TSharedPtr<IDetailTreeNode>& Node, bool bName) -> TSharedRef<SWidget>
	{
		if (!Node.IsValid())
		{
			return SNullWidget::NullWidget;
		}
		const FNodeWidgets Widgets = Node->CreateNodeWidgets();
		const TSharedPtr<SWidget> Widget = bName ? Widgets.NameWidget : (Widgets.ValueWidget.IsValid() ? Widgets.ValueWidget : Widgets.WholeRowWidget);
		return Widget.IsValid() ? Widget.ToSharedRef() : SNullWidget::NullWidget;
	};

	if (!Row->bEntry)
	{
		return Column == NameColumn || Column == ValueColumn ? MakeNodeCell(Row->Node, Column == NameColumn) : SNullWidget::NullWidget;
	}

	if (Column == NameColumn)
	{
		// A name that fails verification never commits; the box shows the reason instead.
		const TSharedRef<SEditableTextBox> NameBox = SNew(SEditableTextBox)
			.Text_Lambda([this, Row]() { return GetEntryName(Row); })
			.RevertTextOnEscape(true)
			.OnVerifyTextChanged_Lambda([this, Row](const FText& Text, FText& OutError)
			{
				OutError = GetNameError(Row, ScriptableDefinitionsEditor::ToName(Text));
				return OutError.IsEmpty();
			})
			.OnTextCommitted_Lambda([this, Row](const FText& Text, ETextCommit::Type)
			{
				Rename(Row, ScriptableDefinitionsEditor::ToName(Text));
			});

		// A name already wrong when saved, such as a duplicate from older data.
		NameBox->SetError(GetNameError(Row, GetRowName(Row)));
		return NameBox;
	}

	if (Column == TypeColumn)
	{
		return SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(FMargin(8.0f, 0.0f))
			[
				SNew(SKzTypeSelector)
				.AllowArrays(Tab == ETab::Context)
				.ValueType_Lambda([this, Row]()
				{
					if (const FKzNamedVariant* Local = Tab == ETab::Locals ? GetLocal(*Row) : nullptr)
					{
						return Local->GetValue().GetType();
					}
					const TOptional<FKzParamDef> Def = FindContext(Row->ContextName);
					return Def ? Def->Type.ValueType : EPropertyBagPropertyType::None;
				})
				.ValueTypeObject_Lambda([this, Row]() -> const UObject*
				{
					if (const FKzNamedVariant* Local = Tab == ETab::Locals ? GetLocal(*Row) : nullptr)
					{
						return Local->GetValue().GetTypeObject();
					}
					const TOptional<FKzParamDef> Def = FindContext(Row->ContextName);
					return Def ? Def->Type.ValueTypeObject.Get() : nullptr;
				})
				.ContainerType_Lambda([this, Row]()
				{
					const TOptional<FKzParamDef> Def = Tab == ETab::Context ? FindContext(Row->ContextName) : TOptional<FKzParamDef>();
					return Def ? Def->Type.ContainerType : EPropertyBagContainerType::None;
				})
				.OnTypeChanged_Lambda([this, Row](EPropertyBagPropertyType NewType, const UObject* NewTypeObject, EPropertyBagContainerType NewContainerType)
				{
					SetType(Row, NewType, NewTypeObject, NewContainerType);
				})
			];
	}

	if (Column == ValueColumn)
	{
		return SNew(SBox)
			.VAlign(VAlign_Center)
			.Padding(FMargin(8.0f, 0.0f))
			[
				MakeNodeCell(Row->Node, false)
			];
	}

	if (Column == UsesColumn)
	{
		return SNew(STextBlock)
			.Text_Lambda([this, Row]() { return FText::AsNumber(Source->CountReferences(Tab, GetRowName(Row))); })
			.ColorAndOpacity_Lambda([this, Row]() { return Source->CountReferences(Tab, GetRowName(Row)) > 0 ? FSlateColor::UseForeground() : FSlateColor::UseSubduedForeground(); });
	}

	if (Column == ActionsColumn)
	{
		return SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ToolTipText(LOCTEXT("DeleteTooltip", "Delete"))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnClicked_Lambda([this, Row]()
			{
				Delete(Row);
				return FReply::Handled();
			})
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Delete"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			];
	}

	return SNullWidget::NullWidget;
}

void SScriptableDefinitionsEditor::ApplyChange(const FText& Description, TFunctionRef<void()> Edit)
{
	if (!Source->IsValid())
	{
		return;
	}

	{
		const FScopedTransaction Transaction(Description);
		Source->BeginChange(Tab);
		Edit();
		Source->EndChange(Tab);
	}
	Regenerate();
}

void SScriptableDefinitionsEditor::EditContext(TFunctionRef<void(TArray<FKzParamDef>&)> Edit)
{
	TArray<FKzParamDef> Entries = Source->GetContext();
	Edit(Entries);
	Source->SetContext(Entries);
}

void SScriptableDefinitionsEditor::Rename(const TSharedPtr<FRow>& Row, FName NewName)
{
	const FName OldName = GetRowName(Row);
	if (NewName == OldName || (Tab == ETab::Locals && !GetLocal(*Row)) || (Tab == ETab::Outputs && !GetOutput(*Row)) || !GetNameError(Row, NewName).IsEmpty())
	{
		return;
	}

	ApplyChange(LOCTEXT("RenameDefinition", "Rename Definition"), [this, Row, OldName, NewName]()
	{
		if (Tab == ETab::Locals)
		{
			GetLocal(*Row)->SetName(NewName);
		}
		else if (Tab == ETab::Outputs)
		{
			*GetOutput(*Row) = NewName;
		}
		else
		{
			EditContext([OldName, NewName](TArray<FKzParamDef>& Entries)
			{
				if (FKzParamDef* Def = Entries.FindByPredicate([OldName](const FKzParamDef& Entry) { return Entry.Name == OldName; }))
				{
					Def->Name = NewName;
				}
			});
		}

		Source->RedirectReferences(Tab, OldName, NewName);
	});
}

void SScriptableDefinitionsEditor::SetType(const TSharedPtr<FRow>& Row, EPropertyBagPropertyType Type, const UObject* TypeObject, EPropertyBagContainerType ContainerType)
{
	ApplyChange(LOCTEXT("ChangeDefinitionType", "Change Definition Type"), [this, Row, Type, TypeObject, ContainerType]()
	{
		if (Tab == ETab::Locals)
		{
			if (FKzNamedVariant* Local = GetLocal(*Row))
			{
				Local->GetValue().SetType(Type, TypeObject);
			}
			return;
		}

		EditContext([Name = Row->ContextName, Type, TypeObject, ContainerType](TArray<FKzParamDef>& Entries)
		{
			if (FKzParamDef* Def = Entries.FindByPredicate([Name](const FKzParamDef& Entry) { return Entry.Name == Name; }))
			{
				Def->Type = FKzTypeDef(ContainerType, Type, TypeObject);
			}
		});
	});
}

void SScriptableDefinitionsEditor::Delete(const TSharedPtr<FRow>& Row)
{
	const int32 Uses = Source->CountReferences(Tab, GetRowName(Row));
	if (Uses > 0 && FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(LOCTEXT("DeleteInUse", "{0} is used in {1} {1}|plural(one=place,other=places), which will stop working. Delete it anyway?"), GetEntryName(Row), Uses)) != EAppReturnType::Yes)
	{
		return;
	}

	ApplyChange(LOCTEXT("DeleteDefinition", "Delete Definition"), [this, Row]()
	{
		if (Tab == ETab::Locals)
		{
			TArray<FKzNamedVariant>* Locals = Source->GetLocals();
			if (Locals->IsValidIndex(Row->Index))
			{
				Locals->RemoveAt(Row->Index);
			}
			return;
		}
		if (Tab == ETab::Outputs)
		{
			TArray<FName>* Outputs = Source->GetOutputs();
			if (Outputs->IsValidIndex(Row->Index))
			{
				Outputs->RemoveAt(Row->Index);
			}
			return;
		}

		EditContext([Name = Row->ContextName](TArray<FKzParamDef>& Entries)
		{
			Entries.RemoveAll([Name](const FKzParamDef& Entry) { return Entry.Name == Name; });
		});
	});
}

FText SScriptableDefinitionsEditor::GetEntryName(const TSharedPtr<FRow>& Row) const
{
	return FText::FromName(GetRowName(Row));
}

FName SScriptableDefinitionsEditor::GetRowName(const TSharedPtr<FRow>& Row) const
{
	if (Tab == ETab::Context)
	{
		return Row->ContextName;
	}
	if (Tab == ETab::Outputs)
	{
		const FName* Output = GetOutput(*Row);
		return Output ? *Output : NAME_None;
	}
	const FKzNamedVariant* Local = GetLocal(*Row);
	return Local ? Local->GetName() : NAME_None;
}

FText SScriptableDefinitionsEditor::GetNameError(const TSharedPtr<FRow>& Row, FName Name) const
{
	if (Name.IsNone())
	{
		return LOCTEXT("EmptyName", "Enter a name.");
	}
	if (!FInstancedPropertyBag::IsPropertyNameValid(Name))
	{
		return LOCTEXT("InvalidName", "Names can't contain symbols such as . / : ' \" ( ) [ ] < > = @ #");
	}
	if (Source->GetReservedNames(Tab).Contains(Name))
	{
		return FText::Format(LOCTEXT("ReservedName", "{0} is built in."), FText::FromName(Name));
	}

	// The row's own name is in the list once.
	int32 Others = Name == GetRowName(Row) ? -1 : 0;
	if (Tab == ETab::Locals)
	{
		if (const TArray<FKzNamedVariant>* Locals = Source->GetLocals())
		{
			for (const FKzNamedVariant& Local : *Locals)
			{
				Others += Local.GetName() == Name ? 1 : 0;
			}
		}
	}
	else if (Tab == ETab::Outputs)
	{
		if (const TArray<FName>* Outputs = Source->GetOutputs())
		{
			for (const FName& Output : *Outputs)
			{
				Others += Output == Name ? 1 : 0;
			}
		}
	}
	else
	{
		for (const FKzParamDef& Def : Source->GetContext())
		{
			Others += Def.Name == Name ? 1 : 0;
		}
	}
	return Others > 0 ? FText::Format(LOCTEXT("DuplicateName", "Another entry is already named {0}."), FText::FromName(Name)) : FText::GetEmpty();
}

void SScriptableDefinitionsEditor::MoveEntry(const TSharedPtr<FRow>& SourceRow, const TSharedPtr<FRow>& TargetRow, bool bBelow)
{
	const int32 From = Rows.IndexOfByKey(SourceRow);
	const int32 To = Rows.IndexOfByKey(TargetRow);
	if (From == INDEX_NONE || To == INDEX_NONE)
	{
		return;
	}

	int32 Insert = To + (bBelow ? 1 : 0);
	if (From < Insert)
	{
		--Insert;
	}
	if (From == Insert)
	{
		return;
	}

	auto Move = [From, Insert](auto& Entries)
	{
		if (Entries.IsValidIndex(From))
		{
			auto Moved = MoveTemp(Entries[From]);
			Entries.RemoveAt(From);
			Entries.Insert(MoveTemp(Moved), FMath::Clamp(Insert, 0, Entries.Num()));
		}
	};

	ApplyChange(LOCTEXT("ReorderDefinitions", "Reorder Definitions"), [this, &Move]()
	{
		if (Tab == ETab::Locals)
		{
			Move(*Source->GetLocals());
		}
		else if (Tab == ETab::Outputs)
		{
			Move(*Source->GetOutputs());
		}
		else
		{
			EditContext([&Move](TArray<FKzParamDef>& Entries) { Move(Entries); });
		}
	});
}

void SScriptableDefinitionsEditor::AddEntry()
{
	const FText Description = ScriptableDefinitionsEditor::ByTab(Tab, LOCTEXT("AddContextTransaction", "Add Context Entry"), LOCTEXT("AddLocalTransaction", "Add Local"), LOCTEXT("AddOutputTransaction", "Add Output"));
	ApplyChange(Description, [this]()
	{
		if (Tab == ETab::Locals)
		{
			TArray<FKzNamedVariant>& Locals = *Source->GetLocals();
			const FName Name = ScriptableDefinitionsEditor::MakeUniqueName(TEXT("NewLocal"), [&Locals](FName Candidate)
			{
				return Locals.ContainsByPredicate([Candidate](const FKzNamedVariant& Local) { return Local.GetName() == Candidate; });
			});
			Locals.Add(FKzNamedVariant(Name, FKzVariant::Make<bool>(false)));
			return;
		}
		if (Tab == ETab::Outputs)
		{
			TArray<FName>& Outputs = *Source->GetOutputs();
			Outputs.Add(ScriptableDefinitionsEditor::MakeUniqueName(TEXT("NewOutput"), [&Outputs](FName Candidate) { return Outputs.Contains(Candidate); }));
			return;
		}

		EditContext([](TArray<FKzParamDef>& Entries)
		{
			const FName Name = ScriptableDefinitionsEditor::MakeUniqueName(TEXT("NewParameter"), [&Entries](FName Candidate)
			{
				return Entries.ContainsByPredicate([Candidate](const FKzParamDef& Entry) { return Entry.Name == Candidate; });
			});
			Entries.Add(FKzParamDef(Name, EPropertyBagPropertyType::Bool));
		});
	});
}

TSharedRef<ITableRow> SScriptableDefinitionsEditor::OnGenerateRow(TSharedPtr<FRow> Row, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(ScriptableDefinitionsEditor::SDefinitionRow, OwnerTable, Row, SharedThis(this));
}

void SScriptableDefinitionsEditor::OnGetChildren(TSharedPtr<FRow> Row, TArray<TSharedPtr<FRow>>& OutChildren)
{
	OutChildren = Row->Children;
}

void SScriptableDefinitionsEditor::NotifyPreChange(FProperty* PropertyAboutToChange)
{
	if (bValueChanging)
	{
		return;
	}

	bValueChanging = true;
	if (!GUndo)
	{
		ValueTransaction = MakeUnique<FScopedTransaction>(LOCTEXT("EditInitialValue", "Edit Initial Value"));
	}
	Source->BeginChange(ETab::Locals);
}

void SScriptableDefinitionsEditor::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	// Drags report every step; the owners catch up once the value settles.
	if (!bValueChanging || PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
	{
		return;
	}

	Source->EndChange(ETab::Locals);
	bValueChanging = false;
	ValueTransaction.Reset();
}

void SScriptableDefinitionsEditor::PostUndo(bool bSuccess)
{
	// Undo rebuilds the edited values, so the rows' widgets point at memory that is gone.
	Regenerate();
}

void SScriptableDefinitionsEditor::PostRedo(bool bSuccess)
{
	Regenerate();
}
#undef LOCTEXT_NAMESPACE