#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NodeUpgradeInteractionComponent.generated.h"

class AFGCharacterPlayer;
class UInputAction;
class UInputComponent;
class UNodeUpgradeMenuWidget;

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
