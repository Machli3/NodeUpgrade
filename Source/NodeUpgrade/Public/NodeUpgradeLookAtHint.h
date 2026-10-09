#pragma once

#include "CoreMinimal.h"

class AActor;
class AFGCharacterPlayer;

/**
 * Decides the hint line "Press {Key} to upgrade this node" shown under the game's look-at prompt (UNodeUpgradeHintRowWidget).
 * Runs only when the actor the player looks at changes (the game's own event) and when the menu closes, never in a Tick.
 */
class NODEUPGRADE_API FNodeUpgradeLookAtHint
{
public:
	/** Shows or hides the hint line for the actor the player now looks at (null: nothing). */
	static void Refresh(AFGCharacterPlayer* Character, AActor* LookedAt);

	/**
	 * Key to show for this actor (a node, or an extractor / geothermal generator built on one), or an empty text when
	 * the key cannot upgrade it: unsupported, already Pure, or no cost entry.
	 */
	static FText GetHintKey(AFGCharacterPlayer* Character, AActor* LookedAt);
};
