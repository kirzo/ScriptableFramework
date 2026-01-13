// Copyright 2025 kirzo

#include "Factories/ScriptableConditionAssetFactory.h"
#include "ScriptableConditions/ScriptableRequirementAsset.h"

UScriptableConditionAssetFactory::UScriptableConditionAssetFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UScriptableRequirementAsset::StaticClass();
}

UObject* UScriptableConditionAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UScriptableRequirementAsset>(InParent, Class, Name, Flags | RF_Transactional);
}