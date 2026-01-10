// Copyright 2025 kirzo

#include "Factories/ScriptableTaskAssetFactory.h"
#include "ScriptableTasks/ScriptableActionAsset.h"

UScriptableTaskAssetFactory::UScriptableTaskAssetFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UScriptableActionAsset::StaticClass();
}

UObject* UScriptableTaskAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UScriptableActionAsset>(InParent, Class, Name, Flags | RF_Transactional);
}