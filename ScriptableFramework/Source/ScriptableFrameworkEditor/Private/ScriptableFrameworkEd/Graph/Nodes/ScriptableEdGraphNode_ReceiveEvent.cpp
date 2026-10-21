// Copyright 2026 kirzo

#include "ScriptableFrameworkEd/Graph/Nodes/ScriptableEdGraphNode_ReceiveEvent.h"
#include "ScriptableFrameworkEditorStyle.h"
#include "ScriptableNodes/ScriptableNode_ReceiveEvent.h"
#include "ScriptableNodes/ScriptableGraph.h"

#include "Styling/AppStyle.h"

UScriptableEdGraphNode_ReceiveEvent::UScriptableEdGraphNode_ReceiveEvent()
{
	RuntimeNodeClass = UScriptableNode_ReceiveEvent::StaticClass();
}

FText UScriptableEdGraphNode_ReceiveEvent::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const UScriptableNode_ReceiveEvent* Event = Cast<UScriptableNode_ReceiveEvent>(RuntimeNode);
	if (!Event)
	{
		return Super::GetNodeTitle(TitleType);
	}

	// Editing (F2) starts from the raw event name (blank when unset); otherwise show the display title.
	if (TitleType == ENodeTitleType::EditableTitle)
	{
		return Event->EventName.IsNone() ? FText::GetEmpty() : FText::FromName(Event->EventName);
	}
	return Event->GetDisplayTitle();
}

void UScriptableEdGraphNode_ReceiveEvent::OnRenameNode(const FString& NewName)
{
	UScriptableNode_ReceiveEvent* Event = Cast<UScriptableNode_ReceiveEvent>(RuntimeNode);
	if (!Event) return;

	const FName NewNameAsName(*NewName);
	if (Event->EventName == NewNameAsName) return;

	Event->Modify();

	/** Drive the change through PreEdit/PostEditChange so it (a) routes through ApplyTargetReferenceRename to
	 * fix local GoTos, and (b) broadcasts FCoreUObjectDelegates::OnObjectPropertyChanged so embedding SubGraph
	 * nodes in other open graphs refresh their event-input pins. */
	FProperty* EventNameProp = UScriptableNode_ReceiveEvent::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UScriptableNode_ReceiveEvent, EventName));
	Event->PreEditChange(EventNameProp);
	Event->EventName = NewNameAsName;
	FPropertyChangedEvent ChangeEvent(EventNameProp);
	Event->PostEditChangeProperty(ChangeEvent);
}

FLinearColor UScriptableEdGraphNode_ReceiveEvent::GetNodeTitleColor() const
{
	return FScriptableFrameworkEditorStyle::EventColor;
}

FSlateIcon UScriptableEdGraphNode_ReceiveEvent::GetIconAndTint(FLinearColor& OutColor) const
{
	OutColor = FLinearColor::White;
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Event_16x");
}