// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableTasks/ScriptableTask.h"
#include "ScriptableTask_Debug.generated.h"

UENUM(BlueprintType)
enum class EScriptableLogSeverity : uint8
{
	Info,
	Warning,
	Error
};

/** Prints a message to the Log and/or Screen. */
UCLASS(DisplayName = "Log Message", meta = (TaskCategory = "System|Debug"))
class SCRIPTABLEFRAMEWORK_API UScriptableTask_LogMessage : public UScriptableTask
{
	GENERATED_BODY()

public:
	/** The message string to print. */
	UPROPERTY(EditAnywhere, Category = "Config")
	FString Message = TEXT("Hello");

	/** The severity of the log (affects color and log verbosity). */
	UPROPERTY(EditAnywhere, Category = "Config")
	EScriptableLogSeverity Severity = EScriptableLogSeverity::Info;

	/** If true, prints on-screen debug messages. */
	UPROPERTY(EditAnywhere, Category = "Config")
	bool bPrintToScreen = true;

	/** If true, prints to the UE_LOG output. */
	UPROPERTY(EditAnywhere, Category = "Config")
	bool bPrintToLog = true;

	/** Duration for the on-screen message. */
	UPROPERTY(EditAnywhere, Category = "Config", meta = (EditCondition = "bPrintToScreen", EditConditionHides))
	float Duration = 2.0f;

	/** Color override for on-screen message. */
	UPROPERTY(EditAnywhere, Category = "Config", meta = (EditCondition = "bPrintToScreen", EditConditionHides))
	FColor TextColor = FColor::Cyan;

protected:
	virtual void BeginTask() override;

public:
#if WITH_EDITOR
	virtual FText GetDisplayTitle() const override;
#endif
};