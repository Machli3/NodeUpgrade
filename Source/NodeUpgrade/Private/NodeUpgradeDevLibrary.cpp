#include "NodeUpgradeDevLibrary.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/PanelWidget.h"
#include "Components/RichTextBlock.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/DataTable.h"
#include "Engine/Font.h"
#include "UObject/UnrealType.h"

namespace
{
	FString DescribeFontInfo(const FSlateFontInfo& Font)
	{
		return FString::Printf(TEXT("font=%s typeface='%s' size=%g spacing=%d outline=%d skew=%g material=%s"),
			Font.FontObject != nullptr ? *Font.FontObject->GetPathName() : TEXT("none"),
			*Font.TypefaceFontName.ToString(), Font.Size, Font.LetterSpacing, Font.OutlineSettings.OutlineSize, Font.SkewAmount,
			Font.FontMaterial != nullptr ? *Font.FontMaterial->GetPathName() : TEXT("none"));
	}

	FString DescribeColor(const FSlateColor& Color)
	{
		return Color.IsColorSpecified() ? Color.GetSpecifiedColor().ToFColorSRGB().ToHex() : TEXT("foreground");
	}

	/** Font, color and text of a text widget, or an empty string for any other widget. */
	FString DescribeTextWidget(const UWidget* Widget)
	{
		if (const UTextBlock* Text = Cast<UTextBlock>(Widget))
		{
			return FString::Printf(TEXT("%s color=%s text='%s'"), *DescribeFontInfo(Text->GetFont()), *DescribeColor(Text->GetColorAndOpacity()), *Text->GetText().ToString().Left(40));
		}
		if (const URichTextBlock* RichText = Cast<URichTextBlock>(Widget))
		{
			// The default style is only valid once the widget is built, and the override's getter is protected: the override is read through reflection.
			const FStructProperty* OverrideProperty = CastField<FStructProperty>(URichTextBlock::StaticClass()->FindPropertyByName(TEXT("DefaultTextStyleOverride")));
			const FTextBlockStyle* Override = OverrideProperty != nullptr && OverrideProperty->Struct == FTextBlockStyle::StaticStruct()
				? OverrideProperty->ContainerPtrToValuePtr<FTextBlockStyle>(RichText) : nullptr;
			const UDataTable* StyleSet = RichText->GetTextStyleSet();
			return FString::Printf(TEXT("styleSet=%s override: %s text='%s'"), StyleSet != nullptr ? *StyleSet->GetPathName() : TEXT("none"),
				Override != nullptr ? *DescribeFontInfo(Override->Font) : TEXT("unreadable"), *RichText->GetText().ToString().Left(40));
		}
		return FString();
	}

	void DescribeTypeface(TArray<FString>& Lines, const TCHAR* Indent, const FTypeface& Typeface)
	{
		for (const FTypefaceEntry& Entry : Typeface.Fonts)
		{
			const UObject* Face = Entry.Font.GetFontFaceAsset();
			Lines.Add(FString::Printf(TEXT("%stypeface '%s' -> %s (%s)"), Indent, *Entry.Name.ToString(),
				Face != nullptr ? *Face->GetPathName() : *Entry.Font.GetFontFilename(),
				*StaticEnum<EFontLoadingPolicy>()->GetNameStringByValue(static_cast<int64>(Entry.Font.GetLoadingPolicy()))));
		}
	}

	FString DescribeFunctionSignature(const UFunction* Function)
	{
		TArray<FString> Params;
		FString Return = TEXT("void");
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				Return = It->GetCPPType();
				continue;
			}
			const bool bOut = It->HasAnyPropertyFlags(CPF_OutParm) && !It->HasAnyPropertyFlags(CPF_ConstParm | CPF_ReferenceParm);
			Params.Add(FString::Printf(TEXT("%s%s %s"), bOut ? TEXT("out ") : TEXT(""), *It->GetCPPType(), *It->GetName()));
		}
		return FString::Printf(TEXT("%s %s(%s) parmsSize=%d"), *Return, *Function->GetName(), *FString::Join(Params, TEXT(", ")), Function->ParmsSize);
	}
}

FString UNodeUpgradeDevLibrary::DescribeClass(UClass* Class)
{
	if (Class == nullptr)
	{
		return TEXT("null class");
	}

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("class %s : %s"), *Class->GetPathName(), Class->GetSuperClass() != nullptr ? *Class->GetSuperClass()->GetName() : TEXT("-")));

	for (TFieldIterator<UFunction> It(Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		Lines.Add(FString::Printf(TEXT("  func %s"), *DescribeFunctionSignature(*It)));
	}

	for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		FString Extra;
		if (const FMulticastDelegateProperty* Delegate = CastField<FMulticastDelegateProperty>(*It))
		{
			Extra = Delegate->SignatureFunction != nullptr ? FString::Printf(TEXT(" signature: %s"), *DescribeFunctionSignature(Delegate->SignatureFunction)) : TEXT(" signature: none");
		}
		else if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(*It))
		{
			Extra = FString::Printf(TEXT(" -> %s"), ObjectProperty->PropertyClass != nullptr ? *ObjectProperty->PropertyClass->GetName() : TEXT("?"));
		}
		Lines.Add(FString::Printf(TEXT("  prop %s %s%s%s"), *It->GetCPPType(), *It->GetName(), It->HasAnyPropertyFlags(CPF_ExposeOnSpawn) ? TEXT(" [ExposeOnSpawn]") : TEXT(""), *Extra));
	}

	if (const UWidgetBlueprintGeneratedClass* WidgetClass = Cast<UWidgetBlueprintGeneratedClass>(Class))
	{
		Lines.Add(FString::Printf(TEXT("  namedSlots: %s"), *FString::JoinBy(WidgetClass->NamedSlots, TEXT(", "), [](const FName& Name) { return Name.ToString(); })));
		if (const UWidgetTree* Tree = WidgetClass->GetWidgetTreeArchetype())
		{
			Tree->ForEachWidget([&Lines](UWidget* Widget)
			{
				FString Size;
				if (const USizeBox* Box = Cast<USizeBox>(Widget))
				{
					// 0 = not overridden in practice.
					Size = FString::Printf(TEXT(" width=%g height=%g minW=%g maxW=%g"), Box->GetWidthOverride(), Box->GetHeightOverride(), Box->GetMinDesiredWidth(), Box->GetMaxDesiredWidth());
				}
				const FString TextFont = DescribeTextWidget(Widget);
				if (!TextFont.IsEmpty())
				{
					Size += TEXT(" ") + TextFont;
				}
				const FString Parent = Widget->GetParent() != nullptr ? Widget->GetParent()->GetName() : TEXT("-");
				const FString SlotClass = Widget->Slot != nullptr ? Widget->Slot->GetClass()->GetName() : TEXT("-");
				Lines.Add(FString::Printf(TEXT("  widget %s (%s)%s parent=%s slot=%s%s"), *Widget->GetName(), *Widget->GetClass()->GetName(), Widget->bIsVariable ? TEXT(" [variable]") : TEXT(""), *Parent, *SlotClass, *Size));
			});
		}
	}

	return FString::Join(Lines, TEXT("\n"));
}

FString UNodeUpgradeDevLibrary::DescribeFont(UFont* Font)
{
	if (Font == nullptr)
	{
		return TEXT("null font");
	}

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("font %s cache=%s legacyName='%s'"), *Font->GetPathName(),
		*StaticEnum<EFontCacheType>()->GetNameStringByValue(static_cast<int64>(Font->FontCacheType)), *Font->LegacyFontName.ToString()));
	const FCompositeFont& Composite = Font->CompositeFont;
	DescribeTypeface(Lines, TEXT("  default "), Composite.DefaultTypeface);
	if (Composite.FallbackTypeface.Typeface.Fonts.Num() > 0)
	{
		Lines.Add(FString::Printf(TEXT("  fallback scale=%g"), Composite.FallbackTypeface.ScalingFactor));
		DescribeTypeface(Lines, TEXT("    "), Composite.FallbackTypeface.Typeface);
	}
	for (const FCompositeSubFont& SubFont : Composite.SubTypefaces)
	{
		Lines.Add(FString::Printf(TEXT("  subfont cultures='%s' ranges=%d scale=%g"), *SubFont.Cultures, SubFont.CharacterRanges.Num(), SubFont.ScalingFactor));
		DescribeTypeface(Lines, TEXT("    "), SubFont.Typeface);
	}
	return FString::Join(Lines, TEXT("\n"));
}

FString UNodeUpgradeDevLibrary::DescribeTextStyleTable(UDataTable* Table)
{
	// FRichTextStyleRow is not exported by UMG: its TextStyle member is read through reflection.
	const UScriptStruct* RowStruct = Table != nullptr ? Table->GetRowStruct() : nullptr;
	const FStructProperty* StyleProperty = RowStruct != nullptr ? CastField<FStructProperty>(RowStruct->FindPropertyByName(TEXT("TextStyle"))) : nullptr;
	if (StyleProperty == nullptr || StyleProperty->Struct != FTextBlockStyle::StaticStruct())
	{
		return TEXT("not a rich text style table");
	}

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("text styles %s"), *Table->GetPathName()));
	for (const TPair<FName, uint8*>& Row : Table->GetRowMap())
	{
		const FTextBlockStyle& Style = *StyleProperty->ContainerPtrToValuePtr<FTextBlockStyle>(Row.Value);
		Lines.Add(FString::Printf(TEXT("  row '%s' %s color=%s"), *Row.Key.ToString(), *DescribeFontInfo(Style.Font), *DescribeColor(Style.ColorAndOpacity)));
	}
	return FString::Join(Lines, TEXT("\n"));
}

FString UNodeUpgradeDevLibrary::DescribeTextFonts(UUserWidget* Widget)
{
	if (Widget == nullptr || Widget->WidgetTree == nullptr)
	{
		return TEXT("no widget tree");
	}

	TArray<FString> Lines;
	TSet<const UWidget*> Seen;
	Widget->WidgetTree->ForEachWidgetAndDescendants([&Lines, &Seen](UWidget* Child)
	{
		bool bAlreadySeen = false;
		Seen.Add(Child, &bAlreadySeen);
		const FString TextFont = bAlreadySeen ? FString() : DescribeTextWidget(Child);
		if (!TextFont.IsEmpty())
		{
			const UUserWidget* Owner = Child->GetTypedOuter<UUserWidget>();
			Lines.Add(FString::Printf(TEXT("  %s/%s (%s) %s"), Owner != nullptr ? *Owner->GetClass()->GetName() : TEXT("?"), *Child->GetName(), *Child->GetClass()->GetName(), *TextFont));
		}
	});
	return FString::Join(Lines, TEXT("\n"));
}
