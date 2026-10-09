#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NodeUpgradeInteractionComponent.generated.h"

class AActor;
class AFGCharacterPlayer;
class APlayerController;
class UInputAction;
class UInputComponent;
class UNodeUpgradeMenuWidget;
struct FKey;
struct FKeyEvent;

/**
 * Added at runtime to the locally controlled player character. Listens to the mod key and opens the menu.
 * Never ticks: the node is looked up only when the key is pressed.
 */
UCLASS()
class NODEUPGRADE_API UNodeUpgradeInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNodeUpgradeInteractionComponent();

	/** Called each time the game sets up the player's input. Adds this component once per character and binds the key. */
	static void BindToPlayer(AFGCharacterPlayer* Character, UInputComponent* InputComponent);

	/** Opens the menu for the node the player is aiming at. Does nothing if no supported node is targeted. */
	void OpenMenuForTarget();

	/** Called by the menu when it closes, whoever closed it. */
	void NotifyMenuClosed(UNodeUpgradeMenuWidget* Menu);

	/** Bound to the character's mOnBestUseableActorUpdated (FNodeUpgradeGameAccess): updates the hint line under the game's prompt. */
	UFUNCTION()
	void HandleBestUsableActorUpdated(bool bIsValid, AActor* BestUsableActor);

	/** Current key of the menu action, player rebinding included, and its modifier keys. False if none is found. */
	static bool GetMenuKey(APlayerController* PlayerController, FKey& OutKey, TArray<FKey>& OutModifiers);

	/** That key written the way the game writes its own keys (e.g. "Y"), or an empty text if none is found. */
	static FText GetMenuKeyName(APlayerController* PlayerController);

	/** True if this key press is the menu key with its modifiers. Used by the open menu, which receives the keys itself. */
	static bool IsMenuKeyEvent(APlayerController* PlayerController, const FKeyEvent& KeyEvent);

private:
	void BindInput(UInputComponent* InputComponent);
	void HandleOpenMenuPressed();

	/** Input component the action is bound on. The game creates a new one when the pawn is possessed again. */
	TWeakObjectPtr<UInputComponent> mBoundInputComponent;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> mOpenMenuAction;

	/** Only exists while the menu is open. */
	UPROPERTY(Transient)
	TObjectPtr<UNodeUpgradeMenuWidget> mOpenMenu;
};
