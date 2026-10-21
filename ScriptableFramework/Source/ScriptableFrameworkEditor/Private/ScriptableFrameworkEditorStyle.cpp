// Copyright 2026 kirzo

#include "ScriptableFrameworkEditorStyle.h"
#include "ScriptableTasks/ScriptableTask.h"
#include "ScriptableConditions/ScriptableCondition.h"
#include "Styling/SlateIconFinder.h"

FLinearColor FScriptableFrameworkEditorStyle::EventColor = FLinearColor(FColor(0xF3, 0x9C, 0x32));
FLinearColor FScriptableFrameworkEditorStyle::ActionColor = FLinearColor(FColor(0x4C, 0x9B, 0xE8));
FLinearColor FScriptableFrameworkEditorStyle::GraphColor = FLinearColor(FColor(0x4C, 0x9B, 0xE8));
FLinearColor FScriptableFrameworkEditorStyle::TaskColor = FLinearColor(FColor(0x3B, 0xC4, 0x8D));
FLinearColor FScriptableFrameworkEditorStyle::RequirementColor = FLinearColor(FColor(0xE0, 0x4F, 0x72));
FLinearColor FScriptableFrameworkEditorStyle::ConditionColor = FLinearColor(FColor(0x9B, 0x6B, 0xDF));
FLinearColor FScriptableFrameworkEditorStyle::FlowColor = FLinearColor(FColor(0x4A, 0x5D, 0x70));
FLinearColor FScriptableFrameworkEditorStyle::ExitColor = FLinearColor(FColor(0x9B, 0x6B, 0xDF));

FLinearColor FScriptableFrameworkEditorStyle::InactiveColor = FLinearColor(0.2f, 0.2f, 0.2f, 0.5f);
FLinearColor FScriptableFrameworkEditorStyle::NegateColor = FLinearColor(0.8f, 0.1f, 0.1f);

FLinearColor FScriptableFrameworkEditorStyle::ContextColor = FLinearColor(0.32f, 0.65f, 0.52f);

FScriptableFrameworkEditorStyle::FScriptableFrameworkEditorStyle()
	: TKzEditorStyle_Base(FName("ScriptableFrameworkEditorStyle"))
{
	SetupPluginResources(TEXT("ScriptableFramework"));

	// White line icon for lists and menus, colour figure for the Content Browser; SVG, which the base's PNG helpers do not load
	for (const TCHAR* Asset : { TEXT("ScriptableActionAsset"), TEXT("ScriptableRequirementAsset"), TEXT("ScriptableGraph") })
	{
		Set(*FString::Printf(TEXT("ClassIcon.%s"), Asset), new FSlateVectorImageBrush(RootToContentDir(FString(TEXT("Icons/")) + Asset, TEXT(".svg")), FVector2D(16.0f, 16.0f)));
		Set(*FString::Printf(TEXT("ClassThumbnail.%s"), Asset), new FSlateVectorImageBrush(RootToContentDir(FString(TEXT("Icons/")) + Asset + TEXT("Color"), TEXT(".svg")), FVector2D(64.0f, 64.0f)));
	}

	// Tasks and conditions, white so they can be tinted. The base classes cover every task or condition without its own,
	// and the ones that run an asset wear that asset's icon
	const TCHAR* const ObjectIcons[][2] = {
		{ TEXT("ScriptableTask"), TEXT("ScriptableTask") },
		{ TEXT("ScriptableCondition"), TEXT("ScriptableCondition") },
		{ TEXT("ScriptableTask_CallFunction"), TEXT("ScriptableTask_CallFunction") },
		{ TEXT("ScriptableTask_LogMessage"), TEXT("ScriptableTask_LogMessage") },
		{ TEXT("ScriptableTask_Wait"), TEXT("ScriptableTask_Wait") },
		{ TEXT("ScriptableTask_SetLocal"), TEXT("ScriptableTask_SetLocal") },
		{ TEXT("ScriptableTask_NestedAction"), TEXT("ScriptableActionAsset") },
		{ TEXT("ScriptableTask_RunAsset"), TEXT("ScriptableActionAsset") },
		{ TEXT("ScriptableTask_RunGraph"), TEXT("ScriptableGraph") },
		{ TEXT("ScriptableCondition_Bool"), TEXT("ScriptableCondition_Bool") },
		{ TEXT("ScriptableCondition_CompareBooleans"), TEXT("ScriptableCondition_CompareBooleans") },
		{ TEXT("ScriptableCondition_CompareNumbers"), TEXT("ScriptableCondition_CompareNumbers") },
		{ TEXT("ScriptableCondition_Distance"), TEXT("ScriptableCondition_Distance") },
		{ TEXT("ScriptableCondition_IsValid"), TEXT("ScriptableCondition_IsValid") },
		{ TEXT("ScriptableCondition_Probability"), TEXT("ScriptableCondition_Probability") },
		{ TEXT("ScriptableCondition_NestedRequirement"), TEXT("ScriptableRequirementAsset") },
		{ TEXT("ScriptableCondition_Asset"), TEXT("ScriptableRequirementAsset") },
	};
	for (const auto& Pair : ObjectIcons)
	{
		Set(*FString::Printf(TEXT("ClassIcon.%s"), Pair[0]), new FSlateVectorImageBrush(RootToContentDir(FString(TEXT("Icons/")) + Pair[1], TEXT(".svg")), FVector2D(16.0f, 16.0f)));
	}

	// Bridge tasks and conditions, from each enabled bridge's own Resources; Run StateTree wears the engine's StateTree icon
	const TCHAR* const BridgeIcons[][3] = {
		{ TEXT("ScriptableFrameworkGAS"), TEXT("ScriptableTask_ActivateAbilityByTag"), TEXT("ScriptableTask_ActivateAbilityByTag") },
		{ TEXT("ScriptableFrameworkGAS"), TEXT("ScriptableTask_GrantAbility"), TEXT("ScriptableTask_GrantAbility") },
		{ TEXT("ScriptableFrameworkGAS"), TEXT("ScriptableTask_ManageGameplayTag"), TEXT("ScriptableTask_ManageGameplayTag") },
		{ TEXT("ScriptableFrameworkGAS"), TEXT("ScriptableTask_SendGameplayEvent"), TEXT("ScriptableTask_SendGameplayEvent") },
		{ TEXT("ScriptableFrameworkGAS"), TEXT("ScriptableCondition_CanActivateByTag"), TEXT("ScriptableCondition_CanActivateByTag") },
		{ TEXT("ScriptableFrameworkGAS"), TEXT("ScriptableCondition_HasAbility"), TEXT("ScriptableCondition_HasAbility") },
		{ TEXT("ScriptableFrameworkGAS"), TEXT("ScriptableCondition_MatchTagQuery"), TEXT("ScriptableCondition_MatchTagQuery") },
		{ TEXT("ScriptableFrameworkSequencer"), TEXT("ScriptableTask_CreateLevelSequencePlayer"), TEXT("ScriptableTask_CreateLevelSequencePlayer") },
		{ TEXT("ScriptableFrameworkSequencer"), TEXT("ScriptableTask_PlayLevelSequence"), TEXT("ScriptableTask_PlayLevelSequence") },
		{ TEXT("ScriptableFrameworkSequencer"), TEXT("ScriptableTask_SetLevelSequencePlaybackPosition"), TEXT("ScriptableTask_SetLevelSequencePlaybackPosition") },
		{ TEXT("ScriptableFrameworkSequencer"), TEXT("ScriptableTask_WaitForLevelSequenceFinish"), TEXT("ScriptableTask_WaitForLevelSequenceFinish") },
		{ TEXT("ScriptableFrameworkUI"), TEXT("ScriptableTask_CreateWidget"), TEXT("ScriptableTask_CreateWidget") },
		{ TEXT("ScriptableFrameworkUI"), TEXT("ScriptableTask_RemoveWidget"), TEXT("ScriptableTask_RemoveWidget") },
		{ TEXT("ScriptableFrameworkLimbic"), TEXT("ScriptableTask_SetLimbicSlot"), TEXT("ScriptableTask_SetLimbicSlot") },
		{ TEXT("ScriptableFrameworkLimbic"), TEXT("ScriptableTask_StopLimbicSlot"), TEXT("ScriptableTask_StopLimbicSlot") },
		{ TEXT("ScriptableFrameworkLimbic"), TEXT("ScriptableTask_StopLimbicSlotByHandle"), TEXT("ScriptableTask_StopLimbicSlot") },
		{ TEXT("ScriptableFrameworkLimbic"), TEXT("ScriptableTask_MakeLimbicTarget"), TEXT("ScriptableTask_MakeLimbicTarget") },
		{ TEXT("StateTree"), TEXT("ScriptableTask_RunStateTree"), TEXT("StateTree") },
	};
	for (const auto& Row : BridgeIcons)
	{
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindEnabledPlugin(Row[0]))
		{
			Set(*FString::Printf(TEXT("ClassIcon.%s"), Row[1]), new FSlateVectorImageBrush(Plugin->GetBaseDir() / TEXT("Resources/Icons") / Row[2] + TEXT(".svg"), FVector2D(16.0f, 16.0f)));
		}
	}

	// Parameter labels
	const FTextBlockStyle& NormalText = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");

	Set("ScriptableFramework.Param.Label", FTextBlockStyle(NormalText)
								.SetFont(FAppStyle::GetFontStyle(TEXT("PropertyWindow.BoldFont")))
								.SetFontSize(8));

	Set("ScriptableFramework.Param.Background", new FSlateRoundedBoxBrush(FStyleColors::Hover, 6.f));
}

FSlateIcon FScriptableFrameworkEditorStyle::GetScriptableObjectIcon(const UClass* Class, FLinearColor& OutTint)
{
	OutTint = FLinearColor::White;
	if (!Class)
	{
		return FSlateIcon();
	}

	if (Class->IsChildOf(UScriptableTask::StaticClass()))
	{
		OutTint = TaskColor;
	}
	else if (Class->IsChildOf(UScriptableCondition::StaticClass()))
	{
		OutTint = ConditionColor;
	}
	else
	{
		return FSlateIcon();
	}

	return FSlateIconFinder::FindIconForClass(Class);
}