// Copyright 2026 kirzo

#pragma once

#include "CoreMinimal.h"
#include "ScriptableTaskCustomization.h"

/** Customization for UScriptableTask_SetLocal: VarName and Value as one row, the type-aware local picker on the left and the value on the right. */
class FScriptableTaskSetLocalCustomization : public FScriptableTaskCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance() { return MakeShareable(new FScriptableTaskSetLocalCustomization); }

	virtual void ProcessPropertyHandle(TSharedRef<IPropertyHandle> SubPropertyHandle, IDetailChildrenBuilder& ChildBuilder, UScriptableObject* Obj, const TArray<FPropertyBindingBindableStructDescriptor>& AccessibleStructs) override;
};