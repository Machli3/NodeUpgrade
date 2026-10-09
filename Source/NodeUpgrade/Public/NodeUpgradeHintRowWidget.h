#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NodeUpgradeHintRowWidget.generated.h"

class URichTextBlock;

/**
 * Second line of the game's look-at prompt (Widget_PlayerInteraction): "Press [Y] to upgrade this node".
 * Inserted by SML right under the game's own line (UNodeUpgradeGameInstanceModule), so the game shows and hides it with its prompt.
 * Built out of copies of the game line's own pieces (background, texts, key box) so that it looks exactly the same.
 * Never ticks: the key to show is pushed by SetHintKey when the looked-at actor changes.
 */
UCLASS()
class NODEUPGRADE_API UNodeUpgradeHintRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Key to show (e.g. "Y"), or an empty text to hide the line. Applied to the line on screen and kept for the ones built later. */
	static void SetHintKey(const FText& KeyName);

protected:
	// Begin UWidget / UUserWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	// End UWidget / UUserWidget interface

private:
	void EnsureWidgetTree();
	void Apply();

	UPROPERTY(Transient)
	TObjectPtr<URichTextBlock> mLeftText;

	UPROPERTY(Transient)
	TObjectPtr<URichTextBlock> mKeyText;

	UPROPERTY(Transient)
	TObjectPtr<URichTextBlock> mRightText;
};
