// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "EdGraphSchema_K2.h"
#include "ScriptableSchema.generated.h"

UCLASS()
class SCRIPTABLEFRAMEWORKEDITOR_API UScriptableSchema : public UEdGraphSchema_K2
{
	GENERATED_BODY()

public:
	void GetScriptableTypeTree(TArray<TSharedPtr<FPinTypeTreeInfo>>& TypeTree, ETypeTreeFilter TypeTreeFilter = ETypeTreeFilter::None) const;
	virtual bool SupportsPinTypeContainer(TWeakPtr<const FEdGraphSchemaAction> SchemaAction, const FEdGraphPinType& PinType, const EPinContainerType& ContainerType) const override;
};