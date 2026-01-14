// Copyright 2026 kirzo

#include "ScriptableTasks/ScriptableTask_Debug.h"
#include "Engine/Engine.h"

// Define a static log category for the framework logic
DEFINE_LOG_CATEGORY_STATIC(LogScriptableLogic, Log, All);

void UScriptableTask_LogMessage::BeginTask()
{
	if (bPrintToLog)
	{
		switch (Severity)
		{
			case EScriptableLogSeverity::Info:
			UE_LOG(LogScriptableLogic, Log, TEXT("%s"), *Message);
			break;
			case EScriptableLogSeverity::Warning:
			UE_LOG(LogScriptableLogic, Warning, TEXT("%s"), *Message);
			break;
			case EScriptableLogSeverity::Error:
			UE_LOG(LogScriptableLogic, Error, TEXT("%s"), *Message);
			break;
		}
	}

	if (bPrintToScreen && GEngine)
	{
		FColor DisplayColor = TextColor;

		// If using default color, adjust based on severity
		if (TextColor == FColor::Cyan)
		{
			if (Severity == EScriptableLogSeverity::Warning) DisplayColor = FColor::Yellow;
			else if (Severity == EScriptableLogSeverity::Error) DisplayColor = FColor::Red;
		}

		GEngine->AddOnScreenDebugMessage(-1, Duration, DisplayColor, Message);
	}

	Finish();
}

#if WITH_EDITOR
FText UScriptableTask_LogMessage::GetDisplayTitle() const
{
	FString BindingName;
	if (GetBindingDisplayText(GET_MEMBER_NAME_CHECKED(UScriptableTask_LogMessage, Message), BindingName))
	{
		return FText::Format(INVTEXT("Log: {0}"), FText::FromString(BindingName));
	}

	return FText::Format(INVTEXT("Log: \"{0}\""), FText::FromString(Message));
}
#endif