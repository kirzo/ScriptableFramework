// Copyright 2026 kirzo

#include "ScriptableParameterDefCustomization.h"
#include "Bindings/ScriptableParameterDef.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "SPinTypeSelector.h"
#include "ScriptableSchema.h"
#include "StructUtils/PropertyBag.h"

#define LOCTEXT_NAMESPACE "FScriptableParameterDefCustomization"

namespace ScriptableFrameworkEditor
{
	FEdGraphPinType PinTypeFromScriptableParam(const FScriptableParameterDef& ScriptableParam)
	{
		FEdGraphPinType PinType;

		switch (ScriptableParam.ContainerType)
		{
			case EPropertyBagContainerType::None:
			{
				PinType.ContainerType = EPinContainerType::None;
				break;
			}
			case EPropertyBagContainerType::Array:
			{
				PinType.ContainerType = EPinContainerType::Array;
				break;
			}
		}

		switch (ScriptableParam.ValueType)
		{
			case EPropertyBagPropertyType::Bool:
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
				break;
			}
			case EPropertyBagPropertyType::Byte:
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
				break;
			}
			case EPropertyBagPropertyType::Int32:
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
				break;
			}
			case EPropertyBagPropertyType::Int64:
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
				break;
			}
			case EPropertyBagPropertyType::Float:
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
				PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
				break;
			}
			case EPropertyBagPropertyType::Double:
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
				PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
				break;
			}
			case EPropertyBagPropertyType::Name:
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
				break;
			}
			case EPropertyBagPropertyType::String:
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_String;
				break;
			}
			case EPropertyBagPropertyType::Text:
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Text;
				break;
			}
			case EPropertyBagPropertyType::Enum:
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
				PinType.PinSubCategoryObject = const_cast<UObject*>(ScriptableParam.ValueTypeObject.Get());
				break;
			}
			case EPropertyBagPropertyType::Struct:
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
				PinType.PinSubCategoryObject = const_cast<UObject*>(ScriptableParam.ValueTypeObject.Get());
				break;
			}
			case EPropertyBagPropertyType::Object:
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
				PinType.PinSubCategoryObject = const_cast<UObject*>(ScriptableParam.ValueTypeObject.Get());
				break;
			}
			case EPropertyBagPropertyType::SoftObject:
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
				PinType.PinSubCategoryObject = const_cast<UObject*>(ScriptableParam.ValueTypeObject.Get());
				break;
			}
			case EPropertyBagPropertyType::Class:
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Class;
				PinType.PinSubCategoryObject = const_cast<UObject*>(ScriptableParam.ValueTypeObject.Get());
				break;
			}
			case EPropertyBagPropertyType::SoftClass:
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
				PinType.PinSubCategoryObject = const_cast<UObject*>(ScriptableParam.ValueTypeObject.Get());
				break;
			}
		}

		return PinType;
	}

	FScriptableParameterDef ScriptableParamFromPinType(const FName& Name, const FEdGraphPinType& PinType)
	{
		FScriptableParameterDef ScriptableParam;
		ScriptableParam.Name = Name;

		switch (PinType.ContainerType)
		{
			case EPinContainerType::None:
			ScriptableParam.ContainerType = EPropertyBagContainerType::None;
			break;
			case EPinContainerType::Array:
			ScriptableParam.ContainerType = EPropertyBagContainerType::Array;
			break;
			case EPinContainerType::Set:
			ensureMsgf(false, TEXT("Unsuported container type [Set]"));
			break;
			case EPinContainerType::Map:
			ensureMsgf(false, TEXT("Unsuported container type [Map]"));
			break;
		}

		if (PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
		{
			ScriptableParam.ValueType = EPropertyBagPropertyType::Bool;
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Byte)
		{
			if (UEnum* Enum = Cast<UEnum>(PinType.PinSubCategoryObject))
			{
				ScriptableParam.ValueType = EPropertyBagPropertyType::Enum;
				ScriptableParam.ValueTypeObject = PinType.PinSubCategoryObject.Get();
			}
			else
			{
				ScriptableParam.ValueType = EPropertyBagPropertyType::Byte;
			}
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
		{
			ScriptableParam.ValueType = EPropertyBagPropertyType::Int32;
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int64)
		{
			ScriptableParam.ValueType = EPropertyBagPropertyType::Int64;
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Real)
		{
			if (PinType.PinSubCategory == UEdGraphSchema_K2::PC_Float)
			{
				ScriptableParam.ValueType = EPropertyBagPropertyType::Float;
			}
			else if (PinType.PinSubCategory == UEdGraphSchema_K2::PC_Double)
			{
				ScriptableParam.ValueType = EPropertyBagPropertyType::Double;
			}
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Name)
		{
			ScriptableParam.ValueType = EPropertyBagPropertyType::Name;
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_String)
		{
			ScriptableParam.ValueType = EPropertyBagPropertyType::String;
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Text)
		{
			ScriptableParam.ValueType = EPropertyBagPropertyType::Text;
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Enum)
		{
			ScriptableParam.ValueType = EPropertyBagPropertyType::Enum;
			ScriptableParam.ValueTypeObject = PinType.PinSubCategoryObject.Get();
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
		{
			ScriptableParam.ValueType = EPropertyBagPropertyType::Struct;
			ScriptableParam.ValueTypeObject = PinType.PinSubCategoryObject.Get();
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Object)
		{
			ScriptableParam.ValueType = EPropertyBagPropertyType::Object;
			ScriptableParam.ValueTypeObject = PinType.PinSubCategoryObject.Get();
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_SoftObject)
		{
			ScriptableParam.ValueType = EPropertyBagPropertyType::SoftObject;
			ScriptableParam.ValueTypeObject = PinType.PinSubCategoryObject.Get();
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
		{
			ScriptableParam.ValueType = EPropertyBagPropertyType::Class;
			ScriptableParam.ValueTypeObject = PinType.PinSubCategoryObject.Get();
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_SoftClass)
		{
			ScriptableParam.ValueType = EPropertyBagPropertyType::SoftClass;
			ScriptableParam.ValueTypeObject = PinType.PinSubCategoryObject.Get();
		}
		else
		{
			ensureMsgf(false, TEXT("Unhandled pin category %s"), *PinType.PinCategory.ToString());
		}

		return ScriptableParam;
	}
}

void FScriptableParameterDefCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	StructHandle = PropertyHandle;
	TSharedPtr<IPropertyHandle> NameHandle = StructHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FScriptableParameterDef, Name));

	const UScriptableSchema* ScriptableSchema = GetDefault<UScriptableSchema>();

	if (NameHandle.IsValid())
	{
		NameHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FScriptableParameterDefCustomization::OnNameChanged));
	}

	PinType = GetTargetPinType();

	HeaderRow
		.NameContent()
		[
			StructHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 4, 0)
				[
					SNew(SBox)
						.WidthOverride(100.0f)
						[
							NameHandle->CreatePropertyValueWidget()
						]
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
						.MaxDesiredHeight(20.0f)
						[
							SNew(SPinTypeSelector, FGetPinTypeTree::CreateUObject(ScriptableSchema, &UScriptableSchema::GetScriptableTypeTree))
								.Schema(ScriptableSchema)
								.TargetPinType(this, &FScriptableParameterDefCustomization::GetTargetPinType)
								.OnPinTypeChanged(this, &FScriptableParameterDefCustomization::OnPinTypeChanged)
								.TypeTreeFilter(ETypeTreeFilter::None)
								.bAllowArrays(true)
								.Font(IDetailLayoutBuilder::GetDetailFont())
						]
				]
		];
}

void FScriptableParameterDefCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

void FScriptableParameterDefCustomization::OnNameChanged()
{
	if (StructHandle.IsValid())
	{
		StructHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	}
}

FEdGraphPinType FScriptableParameterDefCustomization::GetTargetPinType() const
{
	if (PinType.IsSet())
	{
		return PinType.Get();
	}

	if (!StructHandle.IsValid()) return FEdGraphPinType();

	TArray<void*> RawData;
	StructHandle->AccessRawData(RawData);

	if (RawData.Num() > 0 && RawData[0])
	{
		const FScriptableParameterDef* Def = static_cast<const FScriptableParameterDef*>(RawData[0]);
		return ScriptableFrameworkEditor::PinTypeFromScriptableParam(*Def);
	}

	return FEdGraphPinType();
}

void FScriptableParameterDefCustomization::OnPinTypeChanged(const FEdGraphPinType& InPinType)
{
	PinType = InPinType;

	if (!StructHandle.IsValid()) return;

	TArray<void*> RawData;
	StructHandle->AccessRawData(RawData);

	if (RawData.Num() > 0)
	{
		FScopedTransaction Transaction(LOCTEXT("ChangeParamType", "Change Parameter Type"));
		StructHandle->NotifyPreChange();

		for (void* DataPtr : RawData)
		{
			if (!DataPtr) continue;

			FScriptableParameterDef* Def = static_cast<FScriptableParameterDef*>(DataPtr);
			FScriptableParameterDef NewDef = ScriptableFrameworkEditor::ScriptableParamFromPinType(Def->Name, InPinType);
			*Def = NewDef;
		}

		StructHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	}
}

#undef LOCTEXT_NAMESPACE