// Copyright 2026 kirzo

#include "ScriptableFrameworkEd/Customization/ScriptableTaskSetLocalCustomization.h"
#include "ScriptableFrameworkEd/Customization/Widgets/SScriptableNamePicker.h"
#include "ScriptableFrameworkEditorHelpers.h"
#include "ScriptableTasks/ScriptableTask_SetLocal.h"
#include "Core/KzNamedVariant.h"
#include "Customizations/KzVariantCustomization.h"

#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "Widgets/Layout/SBox.h"

void FScriptableTaskSetLocalCustomization::ProcessPropertyHandle(TSharedRef<IPropertyHandle> SubPropertyHandle, IDetailChildrenBuilder& ChildBuilder, UScriptableObject* Obj, const TArray<FPropertyBindingBindableStructDescriptor>& AccessibleStructs)
{
	const FProperty* Prop = SubPropertyHandle->GetProperty();
	const FName PropName = Prop ? Prop->GetFName() : NAME_None;
	const FName VarNameName = GET_MEMBER_NAME_CHECKED(UScriptableTask_SetLocal, VarName);
	const FName ValueName = GET_MEMBER_NAME_CHECKED(UScriptableTask_SetLocal, Value);
	TWeakObjectPtr<UScriptableTask_SetLocal> Task = Cast<UScriptableTask_SetLocal>(Obj);
	if (!Task.IsValid() || (PropName != VarNameName && PropName != ValueName))
	{
		FScriptableTaskCustomization::ProcessPropertyHandle(SubPropertyHandle, ChildBuilder, Obj, AccessibleStructs);
		return;
	}

	// One row for both, built from Value: the local picker where the name goes, the value on the right.
	if (PropName == VarNameName || !ScriptableFrameworkEditor::IsPropertyVisible(SubPropertyHandle))
	{
		return;
	}

	const TSharedPtr<IPropertyHandle> ParentHandle = SubPropertyHandle->GetParentHandle();
	const TSharedPtr<IPropertyHandle> VarNameHandle = ParentHandle.IsValid() ? ParentHandle->GetChildHandle(VarNameName) : nullptr;
	if (!VarNameHandle.IsValid())
	{
		return;
	}

	const TSharedRef<SWidget> Picker = SNew(SScriptableNamePicker)
		.SectionTitle(INVTEXT("Locals"))
		.CurrentName_Lambda([Task]() { return Task.IsValid() ? Task->VarName : NAME_None; })
		.OnGetItems_Lambda([Task]()
		{
			return ScriptableNamePickerItems::FromLocals(Task.Get(), [](const FKzNamedVariant&) { return true; });
		})
		.OnNamePicked_Lambda([VarNameHandle, WeakUtilities = TWeakPtr<IPropertyUtilities>(GetPropertyUtilities())](FName Picked)
		{
			VarNameHandle->SetValue(Picked);

			// Picking a local retypes Value, whose row belongs to the old type.
			if (const TSharedPtr<IPropertyUtilities> Utilities = WeakUtilities.Pin())
			{
				Utilities->RequestForceRefresh();
			}
		});

	// The name column stretches its content; the picker keeps its own width.
	FKzVariantCustomization::AddVariantRow(SubPropertyHandle, SubPropertyHandle, ChildBuilder, SNew(SBox).HAlign(HAlign_Left)[Picker]);
}