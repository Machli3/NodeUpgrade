#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "UI/FGInteractWidget.h"
#include "NodeUpgradeMenuWidget.generated.h"

class AFGCharacterPlayer;
class AFGPlayerController;
class AFGResourceNode;
class UFont;
class UImage;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UNodeUpgradeInteractionComponent;
struct FNodeUpgradePreview;

/** Typefaces of the game font used by the menu. Their names are checked in the font at run time. */
enum class ENodeUpgradeTypeface : uint8
{
	Regular,
	SemiBold,
	Bold,
};

/** Size and typeface of a menu text. */
struct FNodeUpgradeTextStyle
{
	int32 Size;
	ENodeUpgradeTypeface Typeface;
};

/**
 * The upgrade menu. Created when opened and released when closed; pushed on the game's own interaction stack (UFGGameUI).
 * Built in C++ out of the game's own widgets so that it looks native: the standard window (Widget_Window_DarkMode),
 * the standard buttons (Widget_StandardButton) and the build-menu cost slots (Widget_CostSlotWrapper).
 * Those Blueprint widgets are driven through reflection, every property and parameter being checked by name and type first;
 * if one is missing (game update), a plain engine widget is used instead of crashing.
 * Holds no rule of its own: everything comes from ANodeUpgradeSubsystem.
 */
UCLASS()
class NODEUPGRADE_API UNodeUpgradeMenuWidget : public UFGInteractWidget
{
	GENERATED_BODY()

public:
	UNodeUpgradeMenuWidget(const FObjectInitializer& ObjectInitializer);

	/** Creates the menu for this node and shows it. Returns null if it could not be opened. */
	static UNodeUpgradeMenuWidget* Open(AFGPlayerController* PlayerController, AFGCharacterPlayer* Character, AFGResourceNode* Node, UNodeUpgradeInteractionComponent* OwnerComponent);

	void CloseMenu();

	bool IsClosing() const { return bClosing; }

	/** Re-reads the node, inventory and records and updates every line. */
	void Refresh();

	// Begin UFGInteractWidget interface
	virtual void OnEscapePressed_Implementation() override;
	// End UFGInteractWidget interface

protected:
	// Begin UWidget / UUserWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	// End UWidget / UUserWidget interface

	// Begin UFGInteractWidget interface
	virtual bool NativeCanCallInit() override;
	// End UFGInteractWidget interface

private:
	void EnsureWidgetTree();
	void ResolveFont();
	FSlateFontInfo MakeFontInfo(const FNodeUpgradeTextStyle& Style) const;
	/** Width of a text in Slate units, or 0 without a renderer. */
	float MeasureText(const FText& Text, const FNodeUpgradeTextStyle& Style) const;
	UTextBlock* MakeText(const FText& Text, const FNodeUpgradeTextStyle& Style, const FLinearColor& Color);
	UImage* MakeIcon(UTexture2D* Texture, float Size);
	UWidget* MakeButton(const FText& Label, bool bPrimary, FName ClickHandler);
	void SetButtonEnabled(UWidget* Button, bool bEnabled, bool bPrimary);
	UWidget* MakeCostSlot(TSubclassOf<class UFGItemDescriptor> ItemClass, int32 Required, int32 Owned);
	void AddCostRow(TSubclassOf<class UFGItemDescriptor> ItemClass, int32 Required, int32 Owned);
	void AddRefundRow(TSubclassOf<class UFGItemDescriptor> ItemClass, int32 Amount);
	void NotifyOwnerClosed();

	UFUNCTION()
	void HandleUpgradeClicked();

	UFUNCTION()
	void HandleDowngradeClicked();

	UFUNCTION()
	void HandleCloseClicked();

	TWeakObjectPtr<AFGResourceNode> mNode;
	TWeakObjectPtr<AFGCharacterPlayer> mCharacter;
	TWeakObjectPtr<AFGPlayerController> mPlayerController;
	TWeakObjectPtr<UNodeUpgradeInteractionComponent> mOwnerComponent;

	bool bPushedToGameUI = false;
	bool bClosing = false;

	FText mStatusText;
	bool bStatusIsError = false;

	/** The font of the game's own texts (DescriptionText: Open Sans with Noto fallbacks for the other scripts), or null to use the engine font. */
	UPROPERTY(Transient)
	TObjectPtr<const UFont> mFont;

	/** Typeface name in mFont for each ENodeUpgradeTypeface; None (= the font's first typeface) when the font lacks it. */
	FName mTypefaceNames[3];

	/** The game's window, when available. Our content goes into its "WindowBody" slot. */
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> mGameWindow;

	UPROPERTY(Transient)
	TObjectPtr<UImage> mResourceIcon;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> mResourceNameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> mCurrentPurityValue;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> mPurityArrow;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> mNewPurityLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> mNewPurityValue;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> mRateText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> mCostHeader;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> mCostList;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> mRefundHeader;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> mRefundList;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> mStatusLine;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> mUpgradeButton;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> mDowngradeButton;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> mCloseButton;
};
