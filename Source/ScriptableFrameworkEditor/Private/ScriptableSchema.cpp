// Copyright 2026 kirzo

#include "ScriptableSchema.h"

void UScriptableSchema::GetScriptableTypeTree(TArray<TSharedPtr<FPinTypeTreeInfo>>& TypeTree, ETypeTreeFilter TypeTreeFilter) const
{
	GetVariableTypeTree(TypeTree, TypeTreeFilter);

	TypeTree.RemoveAll([](TSharedPtr<FPinTypeTreeInfo> Info)
	{
		return Info->GetPinType(false).PinCategory == PC_Interface;
	});
}

bool UScriptableSchema::SupportsPinTypeContainer(TWeakPtr<const FEdGraphSchemaAction> SchemaAction, const FEdGraphPinType& PinType, const EPinContainerType& ContainerType) const
{
	return ContainerType == EPinContainerType::None || ContainerType == EPinContainerType::Array;
}