// Copyright 2026 kirzo

#include "ScriptableFrameworkEd/Customization/Widgets/SScriptableDefinitionsEditor.h"
#include "ScriptableFrameworkEd/Customization/ScriptableContainerCustomization.h"
#include "ScriptableContainer.h"
#include "ScriptableObject.h"
#include "ScriptableRuntimeData.h"
#include "Core/KzNamedVariant.h"
#include "Core/KzParamDef.h"
#include "Widgets/SKzTypeSelector.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "Input/DragAndDrop.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "UObject/StructOnScope.h"
#include "UObject/UObjectHash.h"
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

	/** The editor open on each container, so its button brings it back instead of opening another. */
	TMap<const FScriptableContainer*, TWeakPtr<SScriptableDefinitionsEditor>> OpenEditors;

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

	/** Takes the Context entry out and back in around the edit, since the set hashes its entries by name and type. */
	void EditContext(FScriptableContainer& Container, FName Name, TFunctionRef<void(FKzParamDef&)> Edit)
	{
		for (auto It = Container.ContextDefinitions.CreateIterator(); It; ++It)
		{
			if (It->Name == Name)
			{
				FKzParamDef Def = *It;
				It.RemoveCurrent();
				Edit(Def);
				Container.ContextDefinitions.Add(Def);
				return;
			}
		}
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

void SScriptableDefinitionsEditor::Open(const TSharedRef<IPropertyHandle>& InContainerHandle, ETab InTab)
{
	TArray<void*> RawData;
	InContainerHandle->AccessRawData(RawData);
	if (RawData.Num() != 1 || !RawData[0])
	{
		return;
	}

	const FScriptableContainer* Key = static_cast<const FScriptableContainer*>(RawData[0]);
	if (const TSharedPtr<SScriptableDefinitionsEditor> Existing = ScriptableDefinitionsEditor::OpenEditors.FindRef(Key).Pin())
	{
		Existing->SetTab(InTab);
		if (const TSharedPtr<SWindow> ExistingWindow = FSlateApplication::Get().FindWidgetWindow(Existing.ToSharedRef()))
		{
			ExistingWindow->BringToFront();
		}
		return;
	}

	const TSharedRef<SScriptableDefinitionsEditor> Editor = SNew(SScriptableDefinitionsEditor, InContainerHandle).Tab(InTab);
	ScriptableDefinitionsEditor::OpenEditors.Add(Key, Editor);

	FVector2D Size(640.0, 440.0);
	GConfig->GetVector2D(ScriptableDefinitionsEditor::ConfigSection, TEXT("Size"), Size, GEditorPerProjectIni);

	const TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(Editor->GetTitle())
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

void SScriptableDefinitionsEditor::Construct(const FArguments& InArgs, const TSharedRef<IPropertyHandle>& InContainerHandle)
{
	ContainerHandle = InContainerHandle;
	Tab = InArgs._Tab;

	TArray<UObject*> OuterObjects;
	InContainerHandle->GetOuterObjects(OuterObjects);
	for (UObject* Outer : OuterObjects)
	{
		Owners.Add(Outer);
	}

	TArray<void*> RawData;
	InContainerHandle->AccessRawData(RawData);
	Container = RawData.Num() == 1 ? static_cast<FScriptableContainer*>(RawData[0]) : nullptr;
	const FStructProperty* StructProperty = CastField<FStructProperty>(InContainerHandle->GetProperty());
	ContainerStruct = StructProperty ? StructProperty->Struct.Get() : FScriptableContainer::StaticStruct();

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
		+ SHeaderRow::Column(ValueColumn)
		.DefaultLabel(LOCTEXT("ValueColumn", "Initial Value"))
		.FillWidth_Lambda([this]() { return ValueFill; })
		.OnWidthChanged_Lambda([this](float Fill) { ValueFill = Fill; ScriptableDefinitionsEditor::SaveFill(TEXT("ValueFill"), Fill); })
		.ShouldGenerateWidget_Lambda([this]() { return Tab == ETab::Locals; })
		+ SHeaderRow::Column(UsesColumn)
		.DefaultLabel(LOCTEXT("UsesColumn", "Uses"))
		.DefaultTooltip(LOCTEXT("UsesColumnTooltip", "Bindings and tasks that read the entry"))
		.FixedWidth(48.0f)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Center)
		.VAlignCell(VAlign_Center)
		+ SHeaderRow::Column(ActionsColumn)
		.DefaultLabel(FText::GetEmpty())
		.FixedWidth(32.0f);

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
				SNew(SSegmentedControl<ETab>)
				.Value_Lambda([this]() { return Tab; })
				.OnValueChanged(this, &SScriptableDefinitionsEditor::SetTab)
				+ SSegmentedControl<ETab>::Slot(ETab::Context)
				.Text_Lambda([this]()
				{
					const FScriptableContainer* Current = GetContainer();
					return FText::Format(LOCTEXT("ContextTab", "Context ({0})"), Current ? Current->ContextDefinitions.Num() : 0);
				})
				+ SSegmentedControl<ETab>::Slot(ETab::Locals)
				.Text_Lambda([this]()
				{
					const FScriptableContainer* Current = GetContainer();
					return FText::Format(LOCTEXT("LocalsTab", "Locals ({0})"), Current ? Current->LocalsDefinitions.Num() : 0);
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
					return Tab == ETab::Context
						? LOCTEXT("ContextDescription", "What the runner passes in each time it runs this. Tasks read it through bindings; nothing here writes it.")
						: LOCTEXT("LocalsDescription", "State each run starts with, at these values. Set Local changes it; tasks read it through bindings.");
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
						.Text_Lambda([this]() { return Tab == ETab::Context ? LOCTEXT("NoContext", "No context entries yet.") : LOCTEXT("NoLocals", "No locals yet."); })
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
						.Text_Lambda([this]() { return Tab == ETab::Context ? LOCTEXT("AddContext", "Add Context Entry") : LOCTEXT("AddLocal", "Add Local"); })
					]
				]
			]
		]
	];

	Regenerate();
}

void SScriptableDefinitionsEditor::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// An owner gone, as a Blueprint compile reinstancing it, takes the container's memory with it.
	if (!GetContainer())
	{
		if (const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared()))
		{
			Window->RequestDestroyWindow();
		}
	}
}

void SScriptableDefinitionsEditor::SetTab(ETab InTab)
{
	Tab = InTab;
	HeaderRow->RefreshColumns();
	Regenerate();
}

FScriptableContainer* SScriptableDefinitionsEditor::GetContainer() const
{
	for (const TWeakObjectPtr<UObject>& Owner : Owners)
	{
		if (!Owner.IsValid())
		{
			return nullptr;
		}
	}
	return Owners.IsEmpty() ? nullptr : Container;
}

FKzNamedVariant* SScriptableDefinitionsEditor::GetLocal(const FRow& Row) const
{
	FScriptableContainer* Current = GetContainer();
	return Current && Current->LocalsDefinitions.IsValidIndex(Row.LocalIndex) ? &Current->LocalsDefinitions[Row.LocalIndex] : nullptr;
}

const FKzParamDef* SScriptableDefinitionsEditor::FindContext(FName Name) const
{
	if (const FScriptableContainer* Current = GetContainer())
	{
		for (const FKzParamDef& Def : Current->ContextDefinitions)
		{
			if (Def.Name == Name)
			{
				return &Def;
			}
		}
	}
	return nullptr;
}

void SScriptableDefinitionsEditor::Regenerate()
{
	Rows.Reset();
	if (const FScriptableContainer* Current = GetContainer())
	{
		if (Tab == ETab::Locals)
		{
			for (int32 Index = 0; Index < Current->LocalsDefinitions.Num(); ++Index)
			{
				Rows.Add(MakeLocalRow(Index));
			}
		}
		else
		{
			for (const FKzParamDef& Def : Current->ContextDefinitions)
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
	Row->LocalIndex = Index;

	const FKzVariant& Value = Container->LocalsDefinitions[Index].GetValue();
	if (!Value.IsValid())
	{
		return Row;
	}

	// The value is edited in place through its bag struct; this editor hears of the edits to notify the owners.
	FPropertyRowGeneratorArgs Args;
	Args.NotifyHook = this;
	Row->Generator = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreatePropertyRowGenerator(Args);

	const TSharedPtr<FStructOnScope> ValueStruct = MakeShared<FStructOnScope>(Value.GetBagStruct(), Value.GetBagMemory());
	if (const UObject* Owner = Owners[0].Get())
	{
		ValueStruct->SetPackage(Owner->GetPackage());
	}
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
					const FKzParamDef* Def = FindContext(Row->ContextName);
					return Def ? Def->Type.ValueType : EPropertyBagPropertyType::None;
				})
				.ValueTypeObject_Lambda([this, Row]() -> const UObject*
				{
					if (const FKzNamedVariant* Local = Tab == ETab::Locals ? GetLocal(*Row) : nullptr)
					{
						return Local->GetValue().GetTypeObject();
					}
					const FKzParamDef* Def = FindContext(Row->ContextName);
					return Def ? Def->Type.ValueTypeObject.Get() : nullptr;
				})
				.ContainerType_Lambda([this, Row]()
				{
					const FKzParamDef* Def = Tab == ETab::Context ? FindContext(Row->ContextName) : nullptr;
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
			.Text_Lambda([this, Row]() { return FText::AsNumber(CountUses(Row)); })
			.ColorAndOpacity_Lambda([this, Row]() { return CountUses(Row) > 0 ? FSlateColor::UseForeground() : FSlateColor::UseSubduedForeground(); });
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

void SScriptableDefinitionsEditor::ApplyChange(const FText& Description, TFunctionRef<void(FScriptableContainer&)> Edit)
{
	FScriptableContainer* Current = GetContainer();
	if (!Current)
	{
		return;
	}

	{
		const FScopedTransaction Transaction(Description);
		BeginChange();
		Edit(*Current);
		EndChange();
	}
	Regenerate();
}

void SScriptableDefinitionsEditor::BeginChange()
{
	if (ContainerHandle.IsValid() && ContainerHandle->IsValidHandle())
	{
		ContainerHandle->NotifyPreChange();
		return;
	}

	// The details panel the window came from rebuilt: the owners themselves are snapshot.
	for (const TWeakObjectPtr<UObject>& Owner : Owners)
	{
		if (UObject* Object = Owner.Get())
		{
			Object->Modify();
		}
	}
}

void SScriptableDefinitionsEditor::EndChange()
{
	FScriptableContainer* Current = GetContainer();
	if (!Current)
	{
		return;
	}

	Current->ConstructContext();
	Current->ConstructLocals();

	if (ContainerHandle.IsValid() && ContainerHandle->IsValidHandle())
	{
		ContainerHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	}
	else
	{
		for (const TWeakObjectPtr<UObject>& Owner : Owners)
		{
			if (UObject* Object = Owner.Get())
			{
				Object->PostEditChange();
			}
		}
	}

	// The owners hear of the change, but not the tasks nested in them, whose auto-bindings read the definitions.
	for (const TWeakObjectPtr<UObject>& Owner : Owners)
	{
		if (UObject* Object = Owner.Get())
		{
			TArray<UObject*> NestedObjects;
			GetObjectsWithOuter(Object, NestedObjects, /*bIncludeNestedObjects=*/true);
			for (UObject* Nested : NestedObjects)
			{
				if (UScriptableObject* Scriptable = Cast<UScriptableObject>(Nested))
				{
					Scriptable->BakeAutoBindings();
				}
			}
		}
	}
}

void SScriptableDefinitionsEditor::Rename(const TSharedPtr<FRow>& Row, FName NewName)
{
	const FName OldName = GetRowName(Row);
	if (NewName == OldName || (Tab == ETab::Locals && !GetLocal(*Row)) || !GetNameError(Row, NewName).IsEmpty())
	{
		return;
	}

	ApplyChange(LOCTEXT("RenameDefinition", "Rename Definition"), [this, Row, OldName, NewName](FScriptableContainer& Current)
	{
		const bool bLocals = Tab == ETab::Locals;
		if (bLocals)
		{
			Current.LocalsDefinitions[Row->LocalIndex].SetName(NewName);
		}
		else
		{
			ScriptableDefinitionsEditor::EditContext(Current, OldName, [NewName](FKzParamDef& Def) { Def.Name = NewName; });
		}

		// Bindings and the tasks naming a Local follow the rename.
		FScriptableContainerCustomization::RedirectBindingsInContainer(&Current, ContainerStruct, GetSourceID(), OldName, NewName);
	});
}

void SScriptableDefinitionsEditor::SetType(const TSharedPtr<FRow>& Row, EPropertyBagPropertyType Type, const UObject* TypeObject, EPropertyBagContainerType ContainerType)
{
	ApplyChange(LOCTEXT("ChangeDefinitionType", "Change Definition Type"), [this, Row, Type, TypeObject, ContainerType](FScriptableContainer& Current)
	{
		if (Tab == ETab::Locals)
		{
			if (Current.LocalsDefinitions.IsValidIndex(Row->LocalIndex))
			{
				Current.LocalsDefinitions[Row->LocalIndex].GetValue().SetType(Type, TypeObject);
			}
		}
		else
		{
			ScriptableDefinitionsEditor::EditContext(Current, Row->ContextName, [Type, TypeObject, ContainerType](FKzParamDef& Def) { Def.Type = FKzTypeDef(ContainerType, Type, TypeObject); });
		}
	});
}

void SScriptableDefinitionsEditor::Delete(const TSharedPtr<FRow>& Row)
{
	const int32 Uses = CountUses(Row);
	if (Uses > 0 && FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(LOCTEXT("DeleteInUse", "{0} is read by {1} {1}|plural(one=binding or task,other=bindings or tasks), which will stop working. Delete it anyway?"), GetEntryName(Row), Uses)) != EAppReturnType::Yes)
	{
		return;
	}

	ApplyChange(LOCTEXT("DeleteDefinition", "Delete Definition"), [this, Row](FScriptableContainer& Current)
	{
		if (Tab == ETab::Locals)
		{
			if (Current.LocalsDefinitions.IsValidIndex(Row->LocalIndex))
			{
				Current.LocalsDefinitions.RemoveAt(Row->LocalIndex);
			}
			return;
		}

		for (auto It = Current.ContextDefinitions.CreateIterator(); It; ++It)
		{
			if (It->Name == Row->ContextName)
			{
				It.RemoveCurrent();
				return;
			}
		}
	});
}

FText SScriptableDefinitionsEditor::GetEntryName(const TSharedPtr<FRow>& Row) const
{
	return FText::FromName(GetRowName(Row));
}

FName SScriptableDefinitionsEditor::GetRowName(const TSharedPtr<FRow>& Row) const
{
	const FKzNamedVariant* Local = Tab == ETab::Locals ? GetLocal(*Row) : nullptr;
	return Local ? Local->GetName() : Row->ContextName;
}

const FGuid& SScriptableDefinitionsEditor::GetSourceID() const
{
	return Tab == ETab::Locals ? ScriptableBindingSources::LocalsStructID : ScriptableBindingSources::ContextStructID;
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

	// The row's own name is in the list once.
	int32 Others = Name == GetRowName(Row) ? -1 : 0;
	if (const FScriptableContainer* Current = GetContainer())
	{
		if (Tab == ETab::Locals)
		{
			for (const FKzNamedVariant& Local : Current->LocalsDefinitions)
			{
				Others += Local.GetName() == Name ? 1 : 0;
			}
		}
		else
		{
			for (const FKzParamDef& Def : Current->ContextDefinitions)
			{
				Others += Def.Name == Name ? 1 : 0;
			}
		}
	}
	return Others > 0 ? FText::Format(LOCTEXT("DuplicateName", "Another entry is already named {0}."), FText::FromName(Name)) : FText::GetEmpty();
}

int32 SScriptableDefinitionsEditor::CountUses(const TSharedPtr<FRow>& Row) const
{
	FScriptableContainer* Current = GetContainer();
	return Current ? FScriptableContainerCustomization::CountReferencesInContainer(Current, ContainerStruct, GetSourceID(), GetRowName(Row)) : 0;
}

void SScriptableDefinitionsEditor::MoveEntry(const TSharedPtr<FRow>& Source, const TSharedPtr<FRow>& Target, bool bBelow)
{
	const int32 From = Rows.IndexOfByKey(Source);
	const int32 To = Rows.IndexOfByKey(Target);
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

	ApplyChange(LOCTEXT("ReorderDefinitions", "Reorder Definitions"), [this, From, Insert](FScriptableContainer& Current)
	{
		if (Tab == ETab::Locals)
		{
			if (Current.LocalsDefinitions.IsValidIndex(From))
			{
				FKzNamedVariant Moved = MoveTemp(Current.LocalsDefinitions[From]);
				Current.LocalsDefinitions.RemoveAt(From);
				Current.LocalsDefinitions.Insert(MoveTemp(Moved), FMath::Clamp(Insert, 0, Current.LocalsDefinitions.Num()));
			}
			return;
		}

		TArray<FKzParamDef> Ordered = Current.ContextDefinitions.Array();
		if (Ordered.IsValidIndex(From))
		{
			const FKzParamDef Moved = Ordered[From];
			Ordered.RemoveAt(From);
			Ordered.Insert(Moved, FMath::Clamp(Insert, 0, Ordered.Num()));
			Current.ContextDefinitions.Empty(Ordered.Num());
			Current.ContextDefinitions.Append(Ordered);
		}
	});
}

void SScriptableDefinitionsEditor::AddEntry()
{
	const FText Description = Tab == ETab::Locals ? LOCTEXT("AddLocalTransaction", "Add Local") : LOCTEXT("AddContextTransaction", "Add Context Entry");
	ApplyChange(Description, [this](FScriptableContainer& Current)
	{
		if (Tab == ETab::Locals)
		{
			const FName Name = ScriptableDefinitionsEditor::MakeUniqueName(TEXT("NewLocal"), [&Current](FName Candidate)
			{
				return Current.LocalsDefinitions.ContainsByPredicate([Candidate](const FKzNamedVariant& Local) { return Local.GetName() == Candidate; });
			});
			Current.LocalsDefinitions.Add(FKzNamedVariant(Name, FKzVariant::Make<bool>(false)));
			return;
		}

		const FName Name = ScriptableDefinitionsEditor::MakeUniqueName(TEXT("NewParameter"), [&Current](FName Candidate)
		{
			for (const FKzParamDef& Def : Current.ContextDefinitions)
			{
				if (Def.Name == Candidate)
				{
					return true;
				}
			}
			return false;
		});
		Current.ContextDefinitions.Add(FKzParamDef(Name, EPropertyBagPropertyType::Bool));
	});
}

FText SScriptableDefinitionsEditor::GetTitle() const
{
	const UObject* Owner = Owners.IsEmpty() ? nullptr : Owners[0].Get();
	const FText AssetName = Owner ? FText::FromString(FPackageName::GetShortName(Owner->GetPackage()->GetName())) : FText::GetEmpty();
	return FText::Format(LOCTEXT("Title", "{0} {1}: Context & Locals"), AssetName, ContainerHandle->GetPropertyDisplayName());
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
	BeginChange();
}

void SScriptableDefinitionsEditor::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	// Drags report every step; the owners catch up once the value settles.
	if (!bValueChanging || PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
	{
		return;
	}

	EndChange();
	bValueChanging = false;
	ValueTransaction.Reset();
}

void SScriptableDefinitionsEditor::PostUndo(bool bSuccess)
{
	// Undo rebuilds the container's values, so the rows' widgets point at memory that is gone.
	Regenerate();
}

void SScriptableDefinitionsEditor::PostRedo(bool bSuccess)
{
	Regenerate();
}

#undef LOCTEXT_NAMESPACE