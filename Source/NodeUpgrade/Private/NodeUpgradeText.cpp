#include "NodeUpgradeText.h"

#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "NodeUpgrade.h"

#define LOCTEXT_NAMESPACE "NodeUpgrade"

namespace
{
	/** Keys of the plain purity names ("Impure", "Normal", "Pure") in the game's string table General_UI, translated in every game language. */
	const TCHAR* GamePurityKey(EResourcePurity Purity)
	{
		switch (Purity)
		{
		case RP_Inpure: return TEXT("World/Resources/PurityLevels/Impure");
		case RP_Normal: return TEXT("World/Resources/PurityLevels/Normal");
		case RP_Pure: return TEXT("World/Resources/PurityLevels/Pure");
		default: return nullptr;
		}
	}
}

namespace NodeUpgradeText
{
	FText Title() { return LOCTEXT("ui.title", "Resource node upgrade"); }
	FText CurrentPurityLabel() { return LOCTEXT("ui.current_purity", "Current purity"); }
	FText NewPurityLabel() { return LOCTEXT("ui.new_purity", "New purity"); }
	FText CostLabel() { return LOCTEXT("ui.cost", "Cost"); }
	FText RefundLabel() { return LOCTEXT("ui.refund", "Refund"); }
	FText UpgradeButton() { return LOCTEXT("ui.upgrade", "Upgrade"); }
	FText DowngradeButton() { return LOCTEXT("ui.downgrade", "Downgrade"); }
	FText CloseButton() { return LOCTEXT("ui.close", "Close"); }

	FText AmountOwned(int32 Owned, int32 Required)
	{
		return FText::FormatNamed(LOCTEXT("ui.amount_owned", "{Owned} / {Required}"),
			TEXT("Owned"), FText::AsNumber(Owned),
			TEXT("Required"), FText::AsNumber(Required));
	}

	FText Missing(int32 Count)
	{
		return FText::FormatNamed(LOCTEXT("ui.missing", "Missing: {Count}"), TEXT("Count"), FText::AsNumber(Count));
	}

	FText Amount(int32 Value)
	{
		return FText::AsNumber(Value);
	}

	FText ExtractionRate(float Before, float After)
	{
		return FText::FormatNamed(LOCTEXT("ui.extraction_rate", "Extraction rate ×{Before} → ×{After}"),
			TEXT("Before"), FText::AsNumber(Before),
			TEXT("After"), FText::AsNumber(After));
	}

	FText RefundHeader(EResourcePurity Purity)
	{
		return FText::FormatNamed(LOCTEXT("ui.refund_header", "Refund if downgraded to {Purity}"), TEXT("Purity"), PurityName(Purity));
	}

	FText RefundAmount(int32 Value)
	{
		return FText::FormatNamed(LOCTEXT("ui.refund_amount", "+{Amount}"), TEXT("Amount"), FText::AsNumber(Value));
	}

	FText PurityName(EResourcePurity Purity)
	{
		// Not the node's own mPurityTextArray: it holds rich-text variants ("<Bold>(Normal)</>") that show raw in plain text blocks.
		const FName TableId(TEXT("General_UI"));
		const FStringTableConstPtr Table = FStringTableRegistry::Get().FindStringTable(TableId);
		static bool bReportedSource = false;
		if (!bReportedSource)
		{
			bReportedSource = true;
			UE_LOG(LogNodeUpgrade, Display, TEXT("Purity names: game string table General_UI %s"), Table.IsValid() ? TEXT("found") : TEXT("not found, using the mod's own texts"));
		}
		if (const TCHAR* Key = GamePurityKey(Purity))
		{
			if (Table.IsValid() && Table->FindEntry(Key).IsValid())
			{
				return FText::FromStringTable(TableId, Key);
			}
		}
		switch (Purity)
		{
		case RP_Inpure: return LOCTEXT("purity.impure", "Impure");
		case RP_Normal: return LOCTEXT("purity.normal", "Normal");
		case RP_Pure: return LOCTEXT("purity.pure", "Pure");
		default: return FText::GetEmpty();
		}
	}

	FText MaxPurity() { return LOCTEXT("err.max_purity", "This node is already at maximum purity"); }
	FText BasePurity() { return LOCTEXT("err.base_purity", "This node cannot go below its original purity"); }

	FText InputOpenMenu() { return LOCTEXT("input.open_menu", "Open node upgrade menu"); }
	FText InputOpenMenuDescription() { return LOCTEXT("input.open_menu.description", "Opens the upgrade menu of the resource node you are looking at."); }
	FText InputCategory() { return Title(); }

	FText ResultMessage(ENodeUpgradeResult Result, bool bUpgrade, EResourcePurity NewPurity)
	{
		switch (Result)
		{
		case ENodeUpgradeResult::Success:
			return FText::FormatNamed(bUpgrade ? LOCTEXT("msg.upgraded", "Node upgraded to {Purity}") : LOCTEXT("msg.downgraded", "Node downgraded to {Purity}"),
				TEXT("Purity"), PurityName(NewPurity));
		case ENodeUpgradeResult::NotEnoughItems:
			return LOCTEXT("err.not_enough_items", "Not enough items");
		case ENodeUpgradeResult::InventoryFull:
			return LOCTEXT("err.inventory_full", "Not enough inventory space for the refund");
		case ENodeUpgradeResult::MaxPurity:
			return MaxPurity();
		case ENodeUpgradeResult::BasePurity:
			return BasePurity();
		case ENodeUpgradeResult::ConfigError:
			return LOCTEXT("err.config", "No valid upgrade cost is defined for this node");
		case ENodeUpgradeResult::InvalidTarget:
		case ENodeUpgradeResult::NotAuthority:
		default:
			return LOCTEXT("err.unsupported", "This node cannot be upgraded");
		}
	}
}

#undef LOCTEXT_NAMESPACE
