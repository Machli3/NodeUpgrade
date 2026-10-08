#pragma once

#include "CoreMinimal.h"
#include "NodeUpgradeTypes.h"

/**
 * Every player-visible text of the mod. Namespace "NodeUpgrade", keys identical to localization/strings.json.
 * Item, resource and purity names come from the game itself (already translated).
 */
namespace NodeUpgradeText
{
	FText Title();
	FText CurrentPurityLabel();
	FText NewPurityLabel();
	FText CostLabel();
	FText RefundLabel();
	FText UpgradeButton();
	FText DowngradeButton();
	FText CloseButton();

	/** "{Owned} / {Required}" */
	FText AmountOwned(int32 Owned, int32 Required);
	/** "Missing: {Count}" */
	FText Missing(int32 Count);
	/** "{Amount}" with the game's number formatting. */
	FText Amount(int32 Amount);

	/** "Extraction rate ×{Before} → ×{After}" (multipliers of the extractors on this node). */
	FText ExtractionRate(float Before, float After);

	/** Header of the refund section: "Refund if downgraded to {Purity}". */
	FText RefundHeader(EResourcePurity Purity);

	/** "+{Amount}" for a refunded item. */
	FText RefundAmount(int32 Amount);

	/** Game's own localized purity name (string table General_UI), with a fallback of the mod's own text. */
	FText PurityName(EResourcePurity Purity);

	/** Message shown after an action. bUpgrade selects "upgraded" vs "downgraded" on success. */
	FText ResultMessage(ENodeUpgradeResult Result, bool bUpgrade, EResourcePurity NewPurity);

	FText MaxPurity();
	FText BasePurity();

	/** Controls menu: name and description of the key action, and name of the key category (texts of the input assets). */
	FText InputOpenMenu();
	FText InputOpenMenuDescription();
	FText InputCategory();
}
