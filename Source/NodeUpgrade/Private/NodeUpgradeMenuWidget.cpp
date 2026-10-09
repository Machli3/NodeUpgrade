#include "NodeUpgradeMenuWidget.h"

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "FGCharacterPlayer.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "FGInventoryComponent.h"
#include "FGPlayerController.h"
#include "ItemAmount.h"
#include "NodeUpgrade.h"
#include "NodeUpgradeDevLibrary.h"
#include "NodeUpgradeInteractionComponent.h"
#include "NodeUpgradeSubsystem.h"
#include "NodeUpgradeText.h"
#include "Resources/FGItemDescriptor.h"
#include "Resources/FGResourceDescriptor.h"
#include "Resources/FGResourceNode.h"
#include "Styling/CoreStyle.h"
#include "UI/FGGameUI.h"
#include "UObject/UnrealType.h"

namespace
{
	// The game's own widgets and font (CL502094). Signatures checked with UNodeUpgradeDevLibrary::DescribeClass, see docs/FINDINGS.md.
	const TCHAR* const GameWindowClassPath = TEXT("/Game/FactoryGame/Interface/UI/Widget_Window_DarkMode.Widget_Window_DarkMode_C");
	const TCHAR* const GameButtonClassPath = TEXT("/Game/FactoryGame/Interface/UI/InGame/-Shared/Widget_StandardButton.Widget_StandardButton_C");
	const TCHAR* const GameCostSlotClassPath = TEXT("/Game/FactoryGame/Interface/UI/InGame/Widget_CostSlotWrapper.Widget_CostSlotWrapper_C");
	const TCHAR* const GameContentBackgroundClassPath = TEXT("/Game/FactoryGame/Interface/UI/InGame/Widget_Window_ContentBackground_DarkMode.Widget_Window_ContentBackground_DarkMode_C");
	/** Used only if the font cannot be read from the game's cost slot. */
	const TCHAR* const GameFontPath = TEXT("/Game/FactoryGame/Interface/Font/DescriptionText.DescriptionText");

	// Text styles of the game's own windows, read from their widgets (docs/FINDINGS.md, fonts): window title 13 Regular,
	// buttons 12 Regular, cost amounts 12 Bold, build menu item name 18 Regular, building window recipe name 14 SemiBold,
	// and the 10 / 12 / 16 / 20 rows of the game's FontStyle_StandardFactoryStyles and FontStyle_PerMinuteText tables.
	constexpr FNodeUpgradeTextStyle HeaderStyle{ 18, ENodeUpgradeTypeface::Regular };
	constexpr FNodeUpgradeTextStyle LabelStyle{ 12, ENodeUpgradeTypeface::Regular };
	constexpr FNodeUpgradeTextStyle ValueStyle{ 14, ENodeUpgradeTypeface::SemiBold };
	constexpr FNodeUpgradeTextStyle ArrowStyle{ 20, ENodeUpgradeTypeface::Regular };
	constexpr FNodeUpgradeTextStyle SectionStyle{ 12, ENodeUpgradeTypeface::SemiBold };
	constexpr FNodeUpgradeTextStyle BodyStyle{ 12, ENodeUpgradeTypeface::Regular };
	constexpr FNodeUpgradeTextStyle StrongStyle{ 12, ENodeUpgradeTypeface::SemiBold };
	constexpr FNodeUpgradeTextStyle SmallStyle{ 10, ENodeUpgradeTypeface::Regular };
	constexpr FNodeUpgradeTextStyle FallbackTitleStyle{ 16, ENodeUpgradeTypeface::Bold };

	// Colors of the game's dark UI (white, light grey and orange from FontStyle_StandardFactoryStyles).
	const FLinearColor OrangeColor = FLinearColor::FromSRGBColor(FColor(229, 147, 68));
	const FLinearColor LabelColor = FLinearColor::FromSRGBColor(FColor(204, 203, 203));
	const FLinearColor ValueColor = FLinearColor::FromSRGBColor(FColor(255, 255, 255));
	const FLinearColor ErrorColor = FLinearColor::FromSRGBColor(FColor(237, 91, 76));
	const FLinearColor SuccessColor = FLinearColor::FromSRGBColor(FColor(116, 214, 118));
	const FLinearColor FallbackBackgroundColor = FLinearColor::FromSRGBColor(FColor(30, 31, 34, 245));
	const FLinearColor IconBackgroundColor = FLinearColor::FromSRGBColor(FColor(45, 47, 52));

	/** Style of the game window's title (Widget_SlidingTabs_Button.mTitleObject), used to measure the translated title. */
	constexpr FNodeUpgradeTextStyle WindowTitleStyle{ 13, ENodeUpgradeTypeface::Regular };
	/**
	 * Width of the title bar around the title text: icon on the left, close button on the right, and a margin.
	 * Estimated on in-game screenshots (2026-10-08): 37 + 99 units, plus 24 of margin.
	 */
	constexpr float WindowTitleChromeWidth = 160.0f;
	/** Inner margins of the window body, like the game's building windows. */
	const FMargin GameWindowBodyPadding(28.0f, 20.0f, 28.0f, 24.0f);
	constexpr float FallbackWidth = 600.0f;
	constexpr float CostSlotSize = 64.0f;

	FLinearColor PurityColor(EResourcePurity Purity)
	{
		switch (Purity)
		{
		case RP_Inpure: return ErrorColor;
		case RP_Normal: return FLinearColor::FromSRGBColor(FColor(245, 190, 65));
		case RP_Pure: return SuccessColor;
		default: return ValueColor;
		}
	}

	float PurityMultiplier(EResourcePurity Purity)
	{
		switch (Purity)
		{
		case RP_Inpure: return 0.5f;
		case RP_Pure: return 2.0f;
		default: return 1.0f;
		}
	}

	UClass* LoadGameWidgetClass(const TCHAR* Path)
	{
		UClass* Class = LoadClass<UUserWidget>(nullptr, Path);
		if (Class == nullptr)
		{
			UE_LOG(LogNodeUpgrade, Warning, TEXT("Game widget '%s' not found: using a plain widget instead"), Path);
		}
		return Class;
	}

	/** The font of the first text block of a game widget class, read from its archetype (no instance needed), or null. */
	const UFont* FindTextFont(UClass* WidgetClass)
	{
		const UWidgetBlueprintGeneratedClass* GeneratedClass = Cast<UWidgetBlueprintGeneratedClass>(WidgetClass);
		const UWidgetTree* Tree = GeneratedClass != nullptr ? GeneratedClass->GetWidgetTreeArchetype() : nullptr;
		const UFont* Found = nullptr;
		if (Tree != nullptr)
		{
			Tree->ForEachWidget([&Found](UWidget* Widget)
			{
				const UTextBlock* Text = Cast<UTextBlock>(Widget);
				if (Found == nullptr && Text != nullptr)
				{
					Found = Cast<UFont>(Text->GetFont().FontObject.Get());
				}
			});
		}
		return Found;
	}

	/** Reflection access to the game's Blueprint widgets. Every helper checks name and type and returns false instead of guessing. */
	namespace GameWidget
	{
		FProperty* FindProperty(UObject* Object, const TCHAR* Name)
		{
			return Object != nullptr ? Object->GetClass()->FindPropertyByName(FName(Name)) : nullptr;
		}

		bool SetText(UObject* Object, const TCHAR* Name, const FText& Value)
		{
			if (FTextProperty* Property = CastField<FTextProperty>(FindProperty(Object, Name)))
			{
				Property->SetPropertyValue_InContainer(Object, Value);
				return true;
			}
			return false;
		}

		bool SetBool(UObject* Object, const TCHAR* Name, bool bValue)
		{
			if (FBoolProperty* Property = CastField<FBoolProperty>(FindProperty(Object, Name)))
			{
				Property->SetPropertyValue_InContainer(Object, bValue);
				return true;
			}
			return false;
		}

		/** Binds a parameterless Blueprint event dispatcher (e.g. OnClicked, OnClose) to a UFUNCTION of Target. */
		bool BindEvent(UObject* Object, const TCHAR* DispatcherName, UObject* Target, FName FunctionName)
		{
			FMulticastDelegateProperty* Property = CastField<FMulticastDelegateProperty>(FindProperty(Object, DispatcherName));
			if (Property == nullptr || Property->SignatureFunction == nullptr || Property->SignatureFunction->NumParms != 0 || Target->FindFunction(FunctionName) == nullptr)
			{
				return false;
			}
			FScriptDelegate Delegate;
			Delegate.BindUFunction(Target, FunctionName);
			Property->AddDelegate(MoveTemp(Delegate), Object);
			return true;
		}

		/** Calls a Blueprint function. SetParam must fill every parameter (checked by name and type), otherwise nothing is called. */
		bool Call(UObject* Object, const TCHAR* FunctionName, TFunctionRef<bool(FProperty*, void*)> SetParam)
		{
			UFunction* Function = Object != nullptr ? Object->FindFunction(FName(FunctionName)) : nullptr;
			if (Function == nullptr)
			{
				return false;
			}
			if (Function->ParmsSize == 0)
			{
				Object->ProcessEvent(Function, nullptr);
				return true;
			}

			uint8* Params = static_cast<uint8*>(FMemory_Alloca_Aligned(Function->ParmsSize, Function->GetMinAlignment()));
			FMemory::Memzero(Params, Function->ParmsSize);
			bool bAllSet = true;
			for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				It->InitializeValue_InContainer(Params);
				if (!It->HasAnyPropertyFlags(CPF_ReturnParm) && !SetParam(*It, It->ContainerPtrToValuePtr<void>(Params)))
				{
					bAllSet = false;
				}
			}
			if (bAllSet)
			{
				Object->ProcessEvent(Function, Params);
			}
			else
			{
				UE_LOG(LogNodeUpgrade, Warning, TEXT("Game widget function %s.%s has an unexpected signature: not called"), *Object->GetClass()->GetName(), FunctionName);
			}
			for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				It->DestroyValue_InContainer(Params);
			}
			return bAllSet;
		}

		bool SetParamObject(FProperty* Param, void* Value, UObject* Object)
		{
			FObjectPropertyBase* Property = CastField<FObjectPropertyBase>(Param);
			if (Property == nullptr || (Object != nullptr && !Object->IsA(Property->PropertyClass)))
			{
				return false;
			}
			Property->SetObjectPropertyValue(Value, Object);
			return true;
		}

		bool SetParamInt(FProperty* Param, void* Value, int32 Number)
		{
			if (FIntProperty* Property = CastField<FIntProperty>(Param))
			{
				Property->SetPropertyValue(Value, Number);
				return true;
			}
			return false;
		}

		bool SetParamBool(FProperty* Param, void* Value, bool bValue)
		{
			if (FBoolProperty* Property = CastField<FBoolProperty>(Param))
			{
				Property->SetPropertyValue(Value, bValue);
				return true;
			}
			return false;
		}
	}
}

UNodeUpgradeMenuWidget::UNodeUpgradeMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Same behaviour as building windows: no movement, no mouse look, cursor visible.
	mUseKeyboard = true;
	mUseMouse = true;
	// Same placement as the game's building windows (Widget_ResourceExtractorUI, Widget_Manufacturing): a Fill slot,
	// centered. With Automatic, the game UI puts the menu against the left edge of the screen.
	mDesiredHorizontalAlignment = HAlign_Center;
	mDesiredVerticalAlignment = VAlign_Center;
	mDesiredAlignmentSize.SizeRule = ESlateSizeRule::Fill;
	mDesiredAlignmentSize.Value = 1.0f;
	mSupportsStacking = false;
	mSupportsCaching = false;
}

UNodeUpgradeMenuWidget* UNodeUpgradeMenuWidget::Open(AFGPlayerController* PlayerController, AFGCharacterPlayer* Character, AFGResourceNode* Node, UNodeUpgradeInteractionComponent* OwnerComponent)
{
	if (!IsValid(PlayerController) || !IsValid(Character) || !IsValid(Node))
	{
		return nullptr;
	}

	UNodeUpgradeMenuWidget* Menu = CreateWidget<UNodeUpgradeMenuWidget>(PlayerController, UNodeUpgradeMenuWidget::StaticClass());
	if (Menu == nullptr)
	{
		UE_LOG(LogNodeUpgrade, Error, TEXT("Could not create the upgrade menu widget"));
		return nullptr;
	}
	Menu->mNode = Node;
	Menu->mCharacter = Character;
	Menu->mPlayerController = PlayerController;
	Menu->mOwnerComponent = OwnerComponent;
	Menu->Refresh();

	if (UFGGameUI* GameUI = PlayerController->GetGameUI())
	{
		GameUI->PushWidget(Menu);
		Menu->bPushedToGameUI = true;
	}
	else
	{
		// No game HUD (should not happen in a game world): show it on the viewport and manage the cursor ourselves.
		UE_LOG(LogNodeUpgrade, Warning, TEXT("Game UI not available, showing the upgrade menu directly on the viewport"));
		Menu->AddToViewport(100);
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(Menu->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		PlayerController->SetShowMouseCursor(true);
	}
	return Menu;
}

void UNodeUpgradeMenuWidget::CloseMenu()
{
	if (bClosing)
	{
		return;
	}
	bClosing = true;

	AFGPlayerController* PlayerController = mPlayerController.Get();
	bool bPopped = false;
	if (bPushedToGameUI && PlayerController != nullptr)
	{
		if (UFGGameUI* GameUI = PlayerController->GetGameUI())
		{
			bPopped = GameUI->PopWidget(this);
		}
	}
	if (!bPopped)
	{
		RemoveFromParent();
	}
	if (!bPushedToGameUI && PlayerController != nullptr)
	{
		PlayerController->SetInputMode(FInputModeGameOnly());
		PlayerController->SetShowMouseCursor(false);
	}
	NotifyOwnerClosed();
}

void UNodeUpgradeMenuWidget::OnEscapePressed_Implementation()
{
	CloseMenu();
}

bool UNodeUpgradeMenuWidget::NativeCanCallInit()
{
	// The base class waits for an interact object; this menu has none and needs nothing more to initialize.
	return true;
}

TSharedRef<SWidget> UNodeUpgradeMenuWidget::RebuildWidget()
{
	EnsureWidgetTree();
	return Super::RebuildWidget();
}

void UNodeUpgradeMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Font, typeface and size of every text of the menu, ours and the game widgets', once per session (docs/FINDINGS.md, fonts).
	static bool bFontsLogged = false;
	if (!bFontsLogged)
	{
		bFontsLogged = true;
		UE_LOG(LogNodeUpgrade, Display, TEXT("Menu text fonts after construction:\n%s"), *UNodeUpgradeDevLibrary::DescribeTextFonts(this));
	}
}

void UNodeUpgradeMenuWidget::NativeDestruct()
{
	Super::NativeDestruct();
	// Also covers the game closing the menu by itself (death, another window...).
	NotifyOwnerClosed();
}

FReply UNodeUpgradeMenuWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// While the menu is open the game keeps the keyboard for its interface, so the menu key never reaches the input action:
	// the menu checks the key itself. Preview: seen before any child widget can take the key.
	if (!bClosing && UNodeUpgradeInteractionComponent::IsMenuKeyEvent(mPlayerController.Get(), InKeyEvent))
	{
		CloseMenu();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UNodeUpgradeMenuWidget::NotifyOwnerClosed()
{
	if (UNodeUpgradeInteractionComponent* Owner = mOwnerComponent.Get())
	{
		mOwnerComponent.Reset();
		Owner->NotifyMenuClosed(this);
	}
}

// --- Content ---------------------------------------------------------------------------------------------------

void UNodeUpgradeMenuWidget::Refresh()
{
	EnsureWidgetTree();
	if (mResourceNameText == nullptr)
	{
		return;
	}

	AFGResourceNode* Node = mNode.Get();
	AFGCharacterPlayer* Character = mCharacter.Get();
	ANodeUpgradeSubsystem* Subsystem = ANodeUpgradeSubsystem::Get(this);

	FNodeUpgradePreview Preview;
	if (Subsystem != nullptr && Node != nullptr && Character != nullptr)
	{
		Preview = Subsystem->BuildPreview(Node, Character);
	}
	const bool bValid = Preview.Status != ENodeUpgradeResult::InvalidTarget;

	// Header: the node's resource, with its icon.
	const TSubclassOf<UFGResourceDescriptor> ResourceClass = Node != nullptr ? Node->GetResourceClass() : nullptr;
	mResourceNameText->SetText(Node != nullptr ? Node->GetResourceName() : FText::GetEmpty());
	UTexture2D* ResourceTexture = ResourceClass != nullptr ? UFGItemDescriptor::GetBigIcon(ResourceClass) : nullptr;
	mResourceIcon->SetBrushFromTexture(ResourceTexture);
	mResourceIcon->SetVisibility(ResourceTexture != nullptr ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	// Purity: current -> after upgrade, and the effect on extractors.
	mCurrentPurityValue->SetText(bValid ? NodeUpgradeText::PurityName(Preview.CurrentPurity) : FText::GetEmpty());
	mCurrentPurityValue->SetColorAndOpacity(FSlateColor(PurityColor(Preview.CurrentPurity)));
	const bool bShowUpgrade = bValid && Preview.bCanUpgrade;
	if (bShowUpgrade)
	{
		mNewPurityLabel->SetText(NodeUpgradeText::NewPurityLabel());
		mNewPurityValue->SetText(NodeUpgradeText::PurityName(Preview.UpgradePurity));
		mNewPurityValue->SetColorAndOpacity(FSlateColor(PurityColor(Preview.UpgradePurity)));
		mRateText->SetText(NodeUpgradeText::ExtractionRate(PurityMultiplier(Preview.CurrentPurity), PurityMultiplier(Preview.UpgradePurity)));
	}
	else
	{
		// Already Pure: the message gets the full width line instead of the right column.
		mNewPurityLabel->SetText(FText::GetEmpty());
		mNewPurityValue->SetText(FText::GetEmpty());
		mRateText->SetText(bValid ? NodeUpgradeText::MaxPurity() : FText::GetEmpty());
	}
	mPurityArrow->SetVisibility(bShowUpgrade ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	mRateText->SetColorAndOpacity(FSlateColor(bShowUpgrade ? OrangeColor : LabelColor));
	mRateText->SetVisibility(mRateText->GetText().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);

	// Cost of the next tier, with what is missing.
	mCostList->ClearChildren();
	mCostHeader->SetVisibility(bShowUpgrade ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	mCostList->SetVisibility(bShowUpgrade ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (bShowUpgrade)
	{
		if (Preview.bUpgradeCostResolved)
		{
			for (const FNodeUpgradeCostLine& Line : Preview.UpgradeCost)
			{
				AddCostRow(Line.ItemClass, Line.Required, Line.Owned);
			}
		}
		else
		{
			mCostList->AddChildToVerticalBox(MakeText(NodeUpgradeText::ResultMessage(ENodeUpgradeResult::ConfigError, true, RP_MAX), BodyStyle, ErrorColor));
		}
	}

	// Refund of the current tier (only above the original purity).
	const bool bShowRefund = bValid && Preview.bCanDowngrade;
	mRefundList->ClearChildren();
	mRefundHeader->SetText(bShowRefund ? NodeUpgradeText::RefundHeader(Preview.DowngradePurity) : FText::GetEmpty());
	mRefundHeader->SetVisibility(bShowRefund ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	mRefundList->SetVisibility(bShowRefund ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (bShowRefund)
	{
		for (const FNodeUpgradeItemAmount& Item : Preview.Refund)
		{
			AddRefundRow(Item.ItemClass, Item.Amount);
		}
	}

	// Buttons stay clickable when items or space are missing: the click then shows the localized reason and changes nothing.
	// Enabled state first: the game's button makes itself visible again when disabled, so visibility is set last.
	SetButtonEnabled(mUpgradeButton, bShowUpgrade && Preview.bUpgradeCostResolved, true);
	SetButtonEnabled(mDowngradeButton, bShowRefund, false);
	mUpgradeButton->SetVisibility(bShowUpgrade ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	mDowngradeButton->SetVisibility(bShowRefund ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	FText Status = mStatusText;
	bool bError = bStatusIsError;
	if (Status.IsEmpty() && !bValid)
	{
		Status = NodeUpgradeText::ResultMessage(ENodeUpgradeResult::InvalidTarget, true, RP_MAX);
		bError = true;
	}
	mStatusLine->SetText(Status);
	mStatusLine->SetColorAndOpacity(FSlateColor(bError ? ErrorColor : SuccessColor));
	mStatusLine->SetVisibility(Status.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}

void UNodeUpgradeMenuWidget::AddCostRow(TSubclassOf<UFGItemDescriptor> ItemClass, int32 Required, int32 Owned)
{
	const bool bMissing = Owned < Required;
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	UHorizontalBoxSlot* SlotForIcon = Row->AddChildToHorizontalBox(MakeCostSlot(ItemClass, Required, Owned));
	SlotForIcon->SetVerticalAlignment(VAlign_Center);
	SlotForIcon->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));

	UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Texts->AddChildToVerticalBox(MakeText(UFGItemDescriptor::GetItemName(ItemClass), StrongStyle, ValueColor));
	Texts->AddChildToVerticalBox(MakeText(NodeUpgradeText::AmountOwned(Owned, Required), BodyStyle, bMissing ? ErrorColor : SuccessColor));
	if (bMissing)
	{
		Texts->AddChildToVerticalBox(MakeText(NodeUpgradeText::Missing(Required - Owned), SmallStyle, ErrorColor));
	}
	UHorizontalBoxSlot* SlotForTexts = Row->AddChildToHorizontalBox(Texts);
	SlotForTexts->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	SlotForTexts->SetVerticalAlignment(VAlign_Center);

	UVerticalBoxSlot* RowSlot = mCostList->AddChildToVerticalBox(Row);
	RowSlot->SetPadding(FMargin(0.0f, 4.0f));
}

void UNodeUpgradeMenuWidget::AddRefundRow(TSubclassOf<UFGItemDescriptor> ItemClass, int32 Amount)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	UHorizontalBoxSlot* SlotForIcon = Row->AddChildToHorizontalBox(MakeIcon(UFGItemDescriptor::GetSmallIcon(ItemClass), 40.0f));
	SlotForIcon->SetVerticalAlignment(VAlign_Center);
	SlotForIcon->SetPadding(FMargin(12.0f, 0.0f, 26.0f, 0.0f));

	UHorizontalBoxSlot* SlotForName = Row->AddChildToHorizontalBox(MakeText(UFGItemDescriptor::GetItemName(ItemClass), StrongStyle, ValueColor));
	SlotForName->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	SlotForName->SetVerticalAlignment(VAlign_Center);

	UHorizontalBoxSlot* SlotForAmount = Row->AddChildToHorizontalBox(MakeText(NodeUpgradeText::RefundAmount(Amount), StrongStyle, SuccessColor));
	SlotForAmount->SetVerticalAlignment(VAlign_Center);

	UVerticalBoxSlot* RowSlot = mRefundList->AddChildToVerticalBox(Row);
	RowSlot->SetPadding(FMargin(0.0f, 3.0f));
}

void UNodeUpgradeMenuWidget::HandleUpgradeClicked()
{
	ANodeUpgradeSubsystem* Subsystem = ANodeUpgradeSubsystem::Get(this);
	AFGResourceNode* Node = mNode.Get();
	AFGCharacterPlayer* Character = mCharacter.Get();
	if (Subsystem == nullptr || Node == nullptr || Character == nullptr)
	{
		CloseMenu();
		return;
	}

	const ENodeUpgradeResult Result = Subsystem->TryUpgrade(Node, Character);
	mStatusText = NodeUpgradeText::ResultMessage(Result, true, Node->GetResourcePurity());
	bStatusIsError = Result != ENodeUpgradeResult::Success;
	Refresh();
}

void UNodeUpgradeMenuWidget::HandleDowngradeClicked()
{
	ANodeUpgradeSubsystem* Subsystem = ANodeUpgradeSubsystem::Get(this);
	AFGResourceNode* Node = mNode.Get();
	AFGCharacterPlayer* Character = mCharacter.Get();
	if (Subsystem == nullptr || Node == nullptr || Character == nullptr)
	{
		CloseMenu();
		return;
	}

	const ENodeUpgradeResult Result = Subsystem->TryDowngrade(Node, Character);
	mStatusText = NodeUpgradeText::ResultMessage(Result, false, Node->GetResourcePurity());
	bStatusIsError = Result != ENodeUpgradeResult::Success;
	Refresh();
}

void UNodeUpgradeMenuWidget::HandleCloseClicked()
{
	CloseMenu();
}

// --- Widget tree -----------------------------------------------------------------------------------------------

void UNodeUpgradeMenuWidget::EnsureWidgetTree()
{
	if (WidgetTree == nullptr || WidgetTree->RootWidget != nullptr)
	{
		return;
	}

	ResolveFont();

	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Body"));

	// The game's window: title bar, background, close button and opening animation, exactly like a building's window.
	if (UClass* WindowClass = LoadGameWidgetClass(GameWindowClassPath))
	{
		mGameWindow = WidgetTree->ConstructWidget<UUserWidget>(WindowClass, TEXT("GameWindow"));
	}
	const bool bWindowOk = mGameWindow != nullptr
		&& GameWidget::SetText(mGameWindow, TEXT("mTitleText"), NodeUpgradeText::Title())
		&& mGameWindow->GetClass()->FindPropertyByName(FName(TEXT("WindowBody"))) != nullptr;
	if (bWindowOk)
	{
		// No player inventory panel next to this window.
		GameWidget::SetBool(mGameWindow, TEXT("mCreateInventoryAddonOnConstruction"), false);
		GameWidget::SetBool(mGameWindow, TEXT("mCanShowInventory"), false);
		if (!GameWidget::BindEvent(mGameWindow, TEXT("OnClose"), this, GET_FUNCTION_NAME_CHECKED(UNodeUpgradeMenuWidget, HandleCloseClicked)))
		{
			UE_LOG(LogNodeUpgrade, Warning, TEXT("Could not bind the game window's OnClose: use Escape to close the menu"));
		}
		// The window draws no background behind its body: the game's windows put this blurred background widget under their content.
		UOverlay* Content = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Content"));
		UClass* BackgroundClass = LoadGameWidgetClass(GameContentBackgroundClassPath);
		UWidget* Background = BackgroundClass != nullptr ? static_cast<UWidget*>(WidgetTree->ConstructWidget<UUserWidget>(BackgroundClass)) : nullptr;
		if (Background == nullptr)
		{
			UBorder* PlainBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			PlainBackground->SetBrushColor(FallbackBackgroundColor);
			Background = PlainBackground;
		}
		UOverlaySlot* BackgroundSlot = Content->AddChildToOverlay(Background);
		BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
		BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
		UOverlaySlot* BodySlot = Content->AddChildToOverlay(Body);
		BodySlot->SetHorizontalAlignment(HAlign_Fill);
		BodySlot->SetVerticalAlignment(VAlign_Fill);
		BodySlot->SetPadding(GameWindowBodyPadding);

		// The window takes the width of its body (frame and title bar included), whatever width it is given:
		// the body is kept wide enough for the translated title, measured in the title's own style.
		USizeBox* BodySize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BodySize"));
		BodySize->SetMinDesiredWidth(MeasureText(NodeUpgradeText::Title(), WindowTitleStyle) + WindowTitleChromeWidth);
		BodySize->SetContent(Content);
		mGameWindow->SetContentForSlot(FName(TEXT("WindowBody")), BodySize);
		Root->SetContent(mGameWindow);
	}
	else
	{
		mGameWindow = nullptr;
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
		Frame->SetBrushColor(FallbackBackgroundColor);
		Frame->SetPadding(FMargin(24.0f));
		Frame->SetContent(Body);
		Root->SetWidthOverride(FallbackWidth);
		Root->SetContent(Frame);
		Body->AddChildToVerticalBox(MakeText(NodeUpgradeText::Title(), FallbackTitleStyle, OrangeColor));
	}

	auto AddToBody = [Body](UWidget* Child, const FMargin& ChildPadding)
	{
		UVerticalBoxSlot* BodySlot = Body->AddChildToVerticalBox(Child);
		BodySlot->SetPadding(ChildPadding);
		BodySlot->SetHorizontalAlignment(HAlign_Fill);
	};

	// Resource: icon and name.
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Header"));
	mResourceIcon = MakeIcon(nullptr, 48.0f);
	UHorizontalBoxSlot* IconSlot = Header->AddChildToHorizontalBox(mResourceIcon);
	IconSlot->SetVerticalAlignment(VAlign_Center);
	IconSlot->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));
	mResourceNameText = MakeText(FText::GetEmpty(), HeaderStyle, ValueColor);
	UHorizontalBoxSlot* NameSlot = Header->AddChildToHorizontalBox(mResourceNameText);
	NameSlot->SetVerticalAlignment(VAlign_Center);
	NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AddToBody(Header, FMargin(0.0f, 4.0f, 0.0f, 14.0f));

	// Purity: current -> new, with the extraction rate change below. Both columns take the width of their text
	// and the arrow fills the space between them, so a long translation can never be cut.
	UHorizontalBox* PurityRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PurityRow"));
	UVerticalBox* CurrentColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	CurrentColumn->AddChildToVerticalBox(MakeText(NodeUpgradeText::CurrentPurityLabel(), LabelStyle, LabelColor));
	mCurrentPurityValue = MakeText(FText::GetEmpty(), ValueStyle, ValueColor);
	CurrentColumn->AddChildToVerticalBox(mCurrentPurityValue);
	PurityRow->AddChildToHorizontalBox(CurrentColumn)->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

	mPurityArrow = MakeText(FText::AsCultureInvariant(TEXT("→")), ArrowStyle, OrangeColor);
	UHorizontalBoxSlot* ArrowSlot = PurityRow->AddChildToHorizontalBox(mPurityArrow);
	ArrowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ArrowSlot->SetHorizontalAlignment(HAlign_Center);
	ArrowSlot->SetVerticalAlignment(VAlign_Center);
	ArrowSlot->SetPadding(FMargin(16.0f, 0.0f));

	UVerticalBox* NewColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	mNewPurityLabel = MakeText(FText::GetEmpty(), LabelStyle, LabelColor);
	NewColumn->AddChildToVerticalBox(mNewPurityLabel)->SetHorizontalAlignment(HAlign_Right);
	mNewPurityValue = MakeText(FText::GetEmpty(), ValueStyle, ValueColor);
	NewColumn->AddChildToVerticalBox(mNewPurityValue)->SetHorizontalAlignment(HAlign_Right);
	PurityRow->AddChildToHorizontalBox(NewColumn)->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	AddToBody(PurityRow, FMargin(0.0f, 0.0f, 0.0f, 4.0f));

	mRateText = MakeText(FText::GetEmpty(), BodyStyle, OrangeColor);
	AddToBody(mRateText, FMargin(0.0f, 0.0f, 0.0f, 16.0f));

	// Cost.
	mCostHeader = MakeText(NodeUpgradeText::CostLabel(), SectionStyle, OrangeColor);
	AddToBody(mCostHeader, FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	mCostList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CostList"));
	AddToBody(mCostList, FMargin(0.0f, 0.0f, 0.0f, 14.0f));

	// Refund.
	mRefundHeader = MakeText(FText::GetEmpty(), SectionStyle, OrangeColor);
	AddToBody(mRefundHeader, FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	mRefundList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RefundList"));
	AddToBody(mRefundList, FMargin(0.0f, 0.0f, 0.0f, 14.0f));

	mStatusLine = MakeText(FText::GetEmpty(), BodyStyle, SuccessColor);
	mStatusLine->SetAutoWrapText(true);
	AddToBody(mStatusLine, FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	// Buttons: the window's own close button and Escape close the menu; a Close button is added only without the game window.
	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Buttons"));
	mUpgradeButton = MakeButton(NodeUpgradeText::UpgradeButton(), true, GET_FUNCTION_NAME_CHECKED(UNodeUpgradeMenuWidget, HandleUpgradeClicked));
	mDowngradeButton = MakeButton(NodeUpgradeText::DowngradeButton(), false, GET_FUNCTION_NAME_CHECKED(UNodeUpgradeMenuWidget, HandleDowngradeClicked));
	TArray<UWidget*> ButtonList = { mUpgradeButton.Get(), mDowngradeButton.Get() };
	if (mGameWindow == nullptr)
	{
		mCloseButton = MakeButton(NodeUpgradeText::CloseButton(), false, GET_FUNCTION_NAME_CHECKED(UNodeUpgradeMenuWidget, HandleCloseClicked));
		ButtonList.Add(mCloseButton.Get());
	}
	for (int32 Index = 0; Index < ButtonList.Num(); ++Index)
	{
		UHorizontalBoxSlot* ButtonSlot = Buttons->AddChildToHorizontalBox(ButtonList[Index]);
		ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ButtonSlot->SetPadding(FMargin(Index == 0 ? 0.0f : 6.0f, 0.0f, 0.0f, 0.0f));
	}
	AddToBody(Buttons, FMargin(0.0f, 4.0f, 0.0f, 0.0f));
}

void UNodeUpgradeMenuWidget::ResolveFont()
{
	// The font of the game's own texts, read from the amount of its cost slot so that no font path has to follow game updates.
	const TCHAR* Source = TEXT("Widget_CostSlotWrapper");
	mFont = FindTextFont(LoadGameWidgetClass(GameCostSlotClassPath));
	if (mFont == nullptr)
	{
		mFont = LoadObject<UFont>(nullptr, GameFontPath);
		Source = GameFontPath;
	}
	if (mFont == nullptr)
	{
		UE_LOG(LogNodeUpgrade, Warning, TEXT("Game font not found (neither in Widget_CostSlotWrapper nor at '%s'): the menu texts use the engine's default font"), GameFontPath);
		return;
	}

	// Typeface names exactly as the font exposes them. A missing one becomes None, which Slate resolves to the font's first typeface.
	// Same order as ENodeUpgradeTypeface.
	static const TCHAR* const WantedTypefaces[] = { TEXT("Regular"), TEXT("SemiBold"), TEXT("Bold") };
	const TArray<FTypefaceEntry>& Typefaces = mFont->CompositeFont.DefaultTypeface.Fonts;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(WantedTypefaces); ++Index)
	{
		const FName Wanted(WantedTypefaces[Index]);
		const bool bFound = Typefaces.ContainsByPredicate([Wanted](const FTypefaceEntry& Entry) { return Entry.Name == Wanted; });
		mTypefaceNames[Index] = bFound ? Wanted : NAME_None;
		if (!bFound)
		{
			UE_LOG(LogNodeUpgrade, Warning, TEXT("Game font %s has no '%s' typeface: its first typeface is used instead"), *mFont->GetPathName(), WantedTypefaces[Index]);
		}
	}

	static bool bLogged = false;
	if (!bLogged)
	{
		bLogged = true;
		UE_LOG(LogNodeUpgrade, Display, TEXT("Menu font: %s (from %s)"), *mFont->GetPathName(), Source);
	}
}

FSlateFontInfo UNodeUpgradeMenuWidget::MakeFontInfo(const FNodeUpgradeTextStyle& Style) const
{
	if (mFont != nullptr)
	{
		// The game's composite font brings its own fallbacks for the other scripts (CJK, Thai, Arabic...).
		return FSlateFontInfo(mFont.Get(), Style.Size, mTypefaceNames[static_cast<int32>(Style.Typeface)]);
	}
	// Engine font (Roboto, with DroidSansFallback for CJK): it has no SemiBold.
	const FName Typeface = Style.Typeface == ENodeUpgradeTypeface::Regular ? FName(TEXT("Regular")) : FName(TEXT("Bold"));
	return FCoreStyle::GetDefaultFontStyle(Typeface, Style.Size);
}

float UNodeUpgradeMenuWidget::MeasureText(const FText& Text, const FNodeUpgradeTextStyle& Style) const
{
	FSlateRenderer* Renderer = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetRenderer() : nullptr;
	return Renderer != nullptr ? Renderer->GetFontMeasureService()->Measure(Text, MakeFontInfo(Style)).X : 0.0f;
}

UTextBlock* UNodeUpgradeMenuWidget::MakeText(const FText& Text, const FNodeUpgradeTextStyle& Style, const FLinearColor& Color)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(Text);
	Block->SetFont(MakeFontInfo(Style));
	Block->SetColorAndOpacity(FSlateColor(Color));
	return Block;
}

UImage* UNodeUpgradeMenuWidget::MakeIcon(UTexture2D* Texture, float Size)
{
	UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Image->SetBrushFromTexture(Texture);
	Image->SetDesiredSizeOverride(FVector2D(Size, Size));
	return Image;
}

UWidget* UNodeUpgradeMenuWidget::MakeButton(const FText& Label, bool bPrimary, FName ClickHandler)
{
	// The game's standard button (orange when primary), with its hover sound and shine.
	if (UClass* ButtonClass = LoadGameWidgetClass(GameButtonClassPath))
	{
		UUserWidget* GameButton = WidgetTree->ConstructWidget<UUserWidget>(ButtonClass);
		if (GameButton != nullptr
			&& GameWidget::SetText(GameButton, TEXT("mText"), Label)
			&& GameWidget::BindEvent(GameButton, TEXT("OnClicked"), this, ClickHandler))
		{
			GameWidget::SetBool(GameButton, TEXT("mIsOrangeButton"), bPrimary);
			GameWidget::SetBool(GameButton, TEXT("mIsCenterAligned"), true);
			return GameButton;
		}
	}

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetContent(MakeText(Label, BodyStyle, FLinearColor::Black));
	FScriptDelegate Delegate;
	Delegate.BindUFunction(this, ClickHandler);
	Button->OnClicked.Add(Delegate);
	return Button;
}

void UNodeUpgradeMenuWidget::SetButtonEnabled(UWidget* Button, bool bEnabled, bool bPrimary)
{
	if (Button == nullptr)
	{
		return;
	}
	const bool bGameButton = GameWidget::Call(Button, TEXT("SetIsDisabled"), [bEnabled](FProperty* Param, void* Value)
	{
		return GameWidget::SetParamBool(Param, Value, !bEnabled);
	});
	if (!bGameButton)
	{
		Button->SetIsEnabled(bEnabled);
		return;
	}
	// Re-enabling resets the game button to its default style: the primary button gets its orange style back.
	if (bEnabled && bPrimary)
	{
		GameWidget::Call(Button, TEXT("SetButtonStyle"), [](FProperty* Param, void* Value)
		{
			const FString Name = Param->GetName();
			return GameWidget::SetParamBool(Param, Value, Name == TEXT("mIsOrange"));
		});
	}
}

UWidget* UNodeUpgradeMenuWidget::MakeCostSlot(TSubclassOf<UFGItemDescriptor> ItemClass, int32 Required, int32 Owned)
{
	UTexture2D* Icon = UFGItemDescriptor::GetBigIcon(ItemClass);
	AFGCharacterPlayer* Character = mCharacter.Get();
	UFGInventoryComponent* Inventory = Character != nullptr ? Character->GetInventory() : nullptr;

	// The build menu's cost slot: item icon with the game's own amount box.
	if (UClass* CostSlotClass = LoadGameWidgetClass(GameCostSlotClassPath))
	{
		UUserWidget* CostSlot = WidgetTree->ConstructWidget<UUserWidget>(CostSlotClass);
		const bool bSetUp = CostSlot != nullptr && GameWidget::Call(CostSlot, TEXT("Setup CostIcon"), [&](FProperty* Param, void* Value)
		{
			const FString Name = Param->GetName();
			if (Name == TEXT("IconTexture")) { return GameWidget::SetParamObject(Param, Value, Icon); }
			if (Name == TEXT("CachedInventoryComponent")) { return GameWidget::SetParamObject(Param, Value, Inventory); }
			if (Name == TEXT("slotIdx")) { return GameWidget::SetParamInt(Param, Value, 0); }
			if (Name == TEXT("CurrentNumInSlot")) { return GameWidget::SetParamInt(Param, Value, Owned); }
			if (Name == TEXT("SmallSlot") || Name == TEXT("BigSlot") || Name == TEXT("ForceOrangeTextbox") || Name == TEXT("GrabFromStockpile") || Name == TEXT("OverwriteComponentNumInSlot"))
			{
				return GameWidget::SetParamBool(Param, Value, false);
			}
			if (Name == TEXT("ItemAmount"))
			{
				FStructProperty* StructProperty = CastField<FStructProperty>(Param);
				if (StructProperty != nullptr && StructProperty->Struct == FItemAmount::StaticStruct())
				{
					*static_cast<FItemAmount*>(Value) = FItemAmount(ItemClass, Required);
					return true;
				}
			}
			return false;
		});
		if (bSetUp)
		{
			USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Box->SetWidthOverride(CostSlotSize);
			Box->SetHeightOverride(CostSlotSize);
			Box->SetContent(CostSlot);
			return Box;
		}
	}

	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetBrushColor(IconBackgroundColor);
	Frame->SetPadding(FMargin(8.0f));
	Frame->SetContent(MakeIcon(Icon, CostSlotSize - 16.0f));
	return Frame;
}
