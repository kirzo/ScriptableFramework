// Copyright 2026 kirzo

#include "ScriptableTasks/ScriptableTask_Flow.h"

void UScriptableTask_Wait::BeginTask()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		// Fallback: If no world (e.g. asset editor preview), finish immediately to avoid getting stuck.
		Finish();
		return;
	}

	float FinalDuration = Duration;

	if (RandomDeviation > UE_KINDA_SMALL_NUMBER)
	{
		FinalDuration += FMath::RandRange(-RandomDeviation, RandomDeviation);
	}

	// Clamp to ensure we don't wait for negative time
	FinalDuration = FMath::Max(0.0f, FinalDuration);

	if (FinalDuration <= UE_KINDA_SMALL_NUMBER)
	{
		Finish();
	}
	else
	{
		World->GetTimerManager().SetTimer(TimerHandle, this, &UScriptableTask_Wait::OnWaitFinished, FinalDuration, false);
	}
}

void UScriptableTask_Wait::OnWaitFinished()
{
	Finish();
}

#if WITH_EDITOR
FText UScriptableTask_Wait::GetDisplayTitle() const
{
	FText DurationText;
	FString BindingName;
	if (GetBindingDisplayText(GET_MEMBER_NAME_CHECKED(UScriptableTask_Wait, Duration), BindingName))
	{
		DurationText = FText::Format(INVTEXT("\"{0}\""), FText::FromString(BindingName));
	}
	else
	{
		DurationText = FText::AsNumber(Duration);
	}

	if (RandomDeviation > 0.0)
	{
		return FText::Format(INVTEXT("Wait {0}s (+/- {1})"), DurationText, FText::AsNumber(RandomDeviation));
	}

	return FText::Format(INVTEXT("Wait {0} s"), DurationText);
}
#endif

#undef LOCTEXT_NAMESPACE