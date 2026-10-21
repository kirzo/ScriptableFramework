// Copyright 2026 kirzo

#include "ScriptableFrameworkEd/Graph/ScriptableEdGraphNode_Task.h"
#include "ScriptableNodes/ScriptableNode_Task.h"
#include "ScriptableTasks/ScriptableTask.h"
#include "ScriptableFrameworkEditorStyle.h"

UScriptableEdGraphNode_Task::UScriptableEdGraphNode_Task()
{
	RuntimeNodeClass = UScriptableNode_Task::StaticClass();
}

FText UScriptableEdGraphNode_Task::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (UScriptableNode_Task* TaskNode = Cast<UScriptableNode_Task>(GetRuntimeNode()))
	{
		if (TaskNode->Task)
		{
			return TaskNode->Task->GetDisplayTitle();
		}
	}
	return NSLOCTEXT("ScriptableEdGraphNode_Task", "EmptyTaskTitle", "Task (empty)");
}

FLinearColor UScriptableEdGraphNode_Task::GetNodeTitleColor() const
{
	if (const UScriptableNode_Task* TaskNode = Cast<UScriptableNode_Task>(GetRuntimeNode()))
	{
		if (const UScriptableTask* Task = TaskNode->Task)
		{
			if (TOptional<FLinearColor> Custom = Task->GetNodeTitleColor()) return *Custom;
		}
	}

	return FScriptableFrameworkEditorStyle::TaskColor;
}

FText UScriptableEdGraphNode_Task::GetTooltipText() const
{
	/** Surface the inner task's class tooltip instead of the wrapper's, which is generic. */
	if (const UScriptableNode_Task* TaskNode = Cast<UScriptableNode_Task>(GetRuntimeNode()))
	{
		if (const UScriptableTask* Task = TaskNode->Task)
		{
			const FText TaskTooltip = Task->GetClass()->GetToolTipText(/*bShortTooltip*/ false);
			if (!TaskTooltip.IsEmpty()) return TaskTooltip;
		}
	}
	return Super::GetTooltipText();
}

FSlateIcon UScriptableEdGraphNode_Task::GetIconAndTint(FLinearColor& OutColor) const
{
	// The wrapped task's own icon, or the plain task block while the node is empty; white, since the title bar already wears the task colour
	const UScriptableNode_Task* TaskNode = Cast<UScriptableNode_Task>(GetRuntimeNode());
	const UClass* TaskClass = TaskNode && TaskNode->Task ? TaskNode->Task->GetClass() : UScriptableTask::StaticClass();
	const FSlateIcon Icon = FScriptableFrameworkEditorStyle::GetScriptableObjectIcon(TaskClass, OutColor);
	OutColor = FLinearColor::White;
	return Icon;
}

bool UScriptableEdGraphNode_Task::ShouldShowPinLabel(FName PinName) const
{
	if (PinName == UScriptableNode_Task::StartInputName) return false;
	if (PinName == UScriptableTask::CompletedOutputName) return false;
	return true;
}