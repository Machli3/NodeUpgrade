#include "NodeUpgradeHintRowWidget.h"

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/RichTextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "NodeUpgrade.h"
#include "NodeUpgradeText.h"

namespace
{
	// Pieces of the game's look-at prompt (Widget_PlayerInteraction, CL502094), read with UNodeUpgradeDevLibrary::DescribeClass (docs/FINDINGS.md).
	// They are copied into our own, not yet built panels: only the slots of those copies are set, never a slot of the game's widget.
	const TCHAR* const PromptBackgroundName = TEXT("BG");
	const TCHAR* const PromptRowName = TEXT("HorizontalBox_0");
	const TCHAR* const PromptLeftTextName = TEXT("mInteractionText_Left");
	const TCHAR* const PromptKeyBoxName = TEXT("mKeyContainer");
	const TCHAR* const PromptKeyBackgroundName = TEXT("mKeyBackground");
	const TCHAR* const PromptKeyTextName = TEXT("mKeyText");
	const TCHAR* const PromptRightTextName = TEXT("mInteractionText_Right");

	/** Space between the game's line and ours, in Slate units. */
	constexpr float LineGap = 4.0f;

	FText& CurrentHintKey()
	{
		static FText Key;
		return Key;
	}

	TArray<TWeakObjectPtr<UNodeUpgradeHintRowWidget>>& LiveRows()
	{
		static TArray<TWeakObjectPtr<UNodeUpgradeHintRowWidget>> Rows;
		return Rows;
	}

	/**
	 * Archetype of the prompt this line was inserted into. It holds the design-time look of every piece; the live pieces are
	 * animated and hidden by the game, so copying them could copy a faded or collapsed state.
	 */
	const UWidgetTree* FindPromptArchetype(const UUserWidget* Row)
	{
		const UUserWidget* Prompt = Row->GetTypedOuter<UUserWidget>();
		const UWidgetBlueprintGeneratedClass* PromptClass = Prompt != nullptr ? Cast<UWidgetBlueprintGeneratedClass>(Prompt->GetClass()) : nullptr;
		return PromptClass != nullptr ? PromptClass->GetWidgetTreeArchetype() : nullptr;
	}

	void CopySlotLayout(const UPanelSlot* From, UPanelSlot* To)
	{
		if (const UHorizontalBoxSlot* FromBox = Cast<UHorizontalBoxSlot>(From))
		{
			if (UHorizontalBoxSlot* ToBox = Cast<UHorizontalBoxSlot>(To))
			{
				ToBox->SetPadding(FromBox->GetPadding());
				ToBox->SetSize(FromBox->GetSize());
				ToBox->SetHorizontalAlignment(FromBox->GetHorizontalAlignment());
				ToBox->SetVerticalAlignment(FromBox->GetVerticalAlignment());
			}
		}
		else if (const UOverlaySlot* FromOverlay = Cast<UOverlaySlot>(From))
		{
			if (UOverlaySlot* ToOverlay = Cast<UOverlaySlot>(To))
			{
				ToOverlay->SetPadding(FromOverlay->GetPadding());
				ToOverlay->SetHorizontalAlignment(FromOverlay->GetHorizontalAlignment());
				ToOverlay->SetVerticalAlignment(FromOverlay->GetVerticalAlignment());
			}
		}
	}

	/**
	 * A copy of one piece of the game's line, constructed with the archetype as template (every property is copied: brush,
	 * rich text style set, font). Returns null if the piece is missing or has another type.
	 */
	template <typename T>
	T* CopyPiece(UWidgetTree* Tree, const UWidgetTree* Archetype, const TCHAR* Name, const UPanelSlot*& OutSourceSlot)
	{
		T* Source = Cast<T>(Archetype->FindWidget(FName(Name)));
		if (Source == nullptr)
		{
			return nullptr;
		}
		OutSourceSlot = Source->Slot;
		T* Copy = NewObject<T>(Tree, Source->GetClass(), NAME_None, RF_Transient, Source);
		// The copied slot pointer is the game's: the copy gets its own when it is added to our panel.
		Copy->Slot = nullptr;
		return Copy;
	}

	void SetPieceText(URichTextBlock* Text, const FText& Value)
	{
		if (Text == nullptr)
		{
			return;
		}
		Text->SetText(Value);
		Text->SetVisibility(Value.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UNodeUpgradeHintRowWidget::SetHintKey(const FText& KeyName)
{
	CurrentHintKey() = KeyName;
	TArray<TWeakObjectPtr<UNodeUpgradeHintRowWidget>>& Rows = LiveRows();
	Rows.RemoveAll([](const TWeakObjectPtr<UNodeUpgradeHintRowWidget>& Row) { return !Row.IsValid(); });
	for (const TWeakObjectPtr<UNodeUpgradeHintRowWidget>& Row : Rows)
	{
		Row->Apply();
	}
}

TSharedRef<SWidget> UNodeUpgradeHintRowWidget::RebuildWidget()
{
	EnsureWidgetTree();
	return Super::RebuildWidget();
}

void UNodeUpgradeHintRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	LiveRows().AddUnique(this);
	// Our own slot in the prompt is not touched here: it is set on the archetype by UNodeUpgradeGameInstanceModule.
	// NativeConstruct runs while the game is still building that slot, and changing it then is an engine assertion (crash).
	Apply();
}

void UNodeUpgradeHintRowWidget::NativeDestruct()
{
	LiveRows().Remove(this);
	Super::NativeDestruct();
}

void UNodeUpgradeHintRowWidget::EnsureWidgetTree()
{
	if (WidgetTree == nullptr)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (WidgetTree->RootWidget != nullptr)
	{
		return;
	}

	const UWidgetTree* Archetype = FindPromptArchetype(this);
	if (Archetype == nullptr)
	{
		UE_LOG(LogNodeUpgrade, Warning, TEXT("Look-at hint line: not inside the game's prompt widget, nothing is shown"));
		return;
	}

	const UPanelSlot* BackgroundSlot = nullptr;
	const UPanelSlot* LeftSlot = nullptr;
	const UPanelSlot* KeyBackgroundSlot = nullptr;
	const UPanelSlot* KeyTextSlot = nullptr;
	const UPanelSlot* RightSlot = nullptr;
	UImage* Background = CopyPiece<UImage>(WidgetTree, Archetype, PromptBackgroundName, BackgroundSlot);
	mLeftText = CopyPiece<URichTextBlock>(WidgetTree, Archetype, PromptLeftTextName, LeftSlot);
	UImage* KeyBackground = CopyPiece<UImage>(WidgetTree, Archetype, PromptKeyBackgroundName, KeyBackgroundSlot);
	mKeyText = CopyPiece<URichTextBlock>(WidgetTree, Archetype, PromptKeyTextName, KeyTextSlot);
	mRightText = CopyPiece<URichTextBlock>(WidgetTree, Archetype, PromptRightTextName, RightSlot);
	const UWidget* GameRow = Archetype->FindWidget(FName(PromptRowName));
	const UWidget* GameKeyBox = Archetype->FindWidget(FName(PromptKeyBoxName));
	if (mLeftText == nullptr || KeyBackground == nullptr || mKeyText == nullptr || mRightText == nullptr)
	{
		// The prompt changed in a game update: no line rather than a broken one.
		UE_LOG(LogNodeUpgrade, Warning, TEXT("Look-at hint line: the game's prompt has changed (missing %s%s%s%s), nothing is shown"),
			mLeftText == nullptr ? PromptLeftTextName : TEXT(""), KeyBackground == nullptr ? PromptKeyBackgroundName : TEXT(""),
			mKeyText == nullptr ? PromptKeyTextName : TEXT(""), mRightText == nullptr ? PromptRightTextName : TEXT(""));
		mLeftText = nullptr;
		mKeyText = nullptr;
		mRightText = nullptr;
		return;
	}

	// A small gap under the game's line, inside our own (not yet built) box: the slot the game gave us is never changed.
	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HintRoot"));
	WidgetTree->RootWidget = Root;
	// Same structure as the game's line: background behind a row "text, key box, text".
	UOverlay* Line = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("HintLine"));
	if (UVerticalBoxSlot* LineSlot = Root->AddChildToVerticalBox(Line))
	{
		LineSlot->SetPadding(FMargin(0.0f, LineGap, 0.0f, 0.0f));
	}
	if (Background != nullptr)
	{
		CopySlotLayout(BackgroundSlot, Line->AddChild(Background));
	}
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HintRow"));
	CopySlotLayout(GameRow != nullptr ? GameRow->Slot : nullptr, Line->AddChild(Row));

	CopySlotLayout(LeftSlot, Row->AddChild(mLeftText));
	UOverlay* KeyBox = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("HintKeyBox"));
	CopySlotLayout(GameKeyBox != nullptr ? GameKeyBox->Slot : nullptr, Row->AddChild(KeyBox));
	CopySlotLayout(KeyBackgroundSlot, KeyBox->AddChild(KeyBackground));
	CopySlotLayout(KeyTextSlot, KeyBox->AddChild(mKeyText));
	CopySlotLayout(RightSlot, Row->AddChild(mRightText));

	static bool bReported = false;
	if (!bReported)
	{
		bReported = true;
		UE_LOG(LogNodeUpgrade, Display, TEXT("Look-at hint line built inside %s (background %s)"),
			*GetTypedOuter<UUserWidget>()->GetClass()->GetName(), Background != nullptr ? TEXT("copied") : TEXT("missing"));
	}
}

void UNodeUpgradeHintRowWidget::Apply()
{
	const FText& Key = CurrentHintKey();
	if (Key.IsEmpty() || mKeyText == nullptr)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	FText Before;
	FText After;
	NodeUpgradeText::LookAtHintParts(Before, After);
	SetPieceText(mLeftText, Before);
	SetPieceText(mRightText, After);
	// Same markup as the game's own key ("<Key>E</>"): the Key style of InteractTextStyle, dark text on the orange key background.
	SetPieceText(mKeyText, FText::Format(INVTEXT("<Key>{0}</>"), Key));
	SetVisibility(ESlateVisibility::HitTestInvisible);
}
