#pragma once

#include "CoreMinimal.h"
#include "Resources/FGItemDescriptor.h"
#include "Resources/FGResourceNode.h"
#include "NodeUpgradeTypes.generated.h"

/** Outcome of an upgrade / downgrade attempt. The UI turns each value into a localized message. */
UENUM()
enum class ENodeUpgradeResult : uint8
{
	Success,
	/** Null node, unsupported node type (deposit, fracking core...) or no player inventory. */
	InvalidTarget,
	/** No cost entry for this node in costs.json, or an item class of the entry could not be resolved. */
	ConfigError,
	MaxPurity,
	BasePurity,
	NotEnoughItems,
	InventoryFull,
	/** Called without authority (multiplayer client). Upgrades must run on the server. */
	NotAuthority
};

/** Node categories used by costs.json ("nodeKind"). */
UENUM()
enum class ENodeUpgradeNodeKind : uint8
{
	Unknown,
	/** "solid": ore nodes (iron, copper, limestone, coal, caterium, quartz, sulfur, bauxite, uranium, SAM). */
	Solid,
	/** "fluid_node": crude oil nodes (oil extractor). */
	FluidNode,
	/** "fracking_satellite": resource well satellites (oil, water, nitrogen). */
	FrackingSatellite,
	/** "geyser": geysers (geothermal generator). */
	Geyser
};

/** An item class and a quantity, as paid or refunded. */
USTRUCT()
struct NODEUPGRADE_API FNodeUpgradeItemAmount
{
	GENERATED_BODY()

	FNodeUpgradeItemAmount() = default;
	FNodeUpgradeItemAmount(TSubclassOf<UFGItemDescriptor> InItemClass, int32 InAmount) : ItemClass(InItemClass), Amount(InAmount) {}

	UPROPERTY(SaveGame)
	TSubclassOf<UFGItemDescriptor> ItemClass = nullptr;

	UPROPERTY(SaveGame)
	int32 Amount = 0;
};

/** What the player actually paid for one tier of one node. Refunds are always based on this, never on the current config. */
USTRUCT()
struct NODEUPGRADE_API FNodeUpgradeTierPayment
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	TArray<FNodeUpgradeItemAmount> Items;
};

/** Saved state of one modified node. Only nodes that were modified by the mod have a record. */
USTRUCT()
struct NODEUPGRADE_API FNodeUpgradeRecord
{
	GENERATED_BODY()

	/** Level-placed node. Saved as an object reference, exactly like extractors save their node. */
	UPROPERTY(SaveGame)
	TObjectPtr<AFGResourceNode> Node = nullptr;

	/** Effective purity of the node the first time the mod modified it. The node can never go below it. */
	UPROPERTY(SaveGame)
	TEnumAsByte<EResourcePurity> OriginalPurity = RP_MAX;

	/** Purity set by the mod. Re-applied on load in case something else (node randomization) overwrote it. */
	UPROPERTY(SaveGame)
	TEnumAsByte<EResourcePurity> CurrentPurity = RP_MAX;

	UPROPERTY(SaveGame)
	FNodeUpgradeTierPayment PaidImpureToNormal;

	UPROPERTY(SaveGame)
	FNodeUpgradeTierPayment PaidNormalToPure;

	FNodeUpgradeTierPayment& GetPayment(int32 Tier) { return Tier == 0 ? PaidImpureToNormal : PaidNormalToPure; }
	const FNodeUpgradeTierPayment& GetPayment(int32 Tier) const { return Tier == 0 ? PaidImpureToNormal : PaidNormalToPure; }
};

/**
 * Tier helpers. Tier 0 = Impure -> Normal, tier 1 = Normal -> Pure.
 * EResourcePurity values are ordered: RP_Inpure (0) < RP_Normal (1) < RP_Pure (2) < RP_MAX (3).
 */
namespace NodeUpgradeTier
{
	constexpr int32 Count = 2;

	inline bool IsValidPurity(EResourcePurity Purity)
	{
		return Purity == RP_Inpure || Purity == RP_Normal || Purity == RP_Pure;
	}

	/** Tier paid when upgrading from this purity, INDEX_NONE if it is already Pure (or invalid). */
	inline int32 ForUpgradeFrom(EResourcePurity Purity)
	{
		return (Purity == RP_Inpure || Purity == RP_Normal) ? static_cast<int32>(Purity) : INDEX_NONE;
	}

	/** Tier refunded when downgrading from this purity, INDEX_NONE if it is already Impure (or invalid). */
	inline int32 ForDowngradeFrom(EResourcePurity Purity)
	{
		return (Purity == RP_Normal || Purity == RP_Pure) ? static_cast<int32>(Purity) - 1 : INDEX_NONE;
	}

	inline EResourcePurity Above(EResourcePurity Purity)
	{
		return Purity == RP_Inpure ? RP_Normal : RP_Pure;
	}

	inline EResourcePurity Below(EResourcePurity Purity)
	{
		return Purity == RP_Pure ? RP_Normal : RP_Inpure;
	}
}
