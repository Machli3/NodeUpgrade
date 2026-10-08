#pragma once

#include "CoreMinimal.h"
#include "NodeUpgradeTypes.h"

/** One item line of a tier, as written in costs.json. The class is resolved later (see ANodeUpgradeSubsystem::ResolveItemClass). */
struct FNodeUpgradeCostItem
{
	FName ItemClassName;
	int32 Amount = 0;
};

/** One "resources" entry of costs.json. */
struct FNodeUpgradeCostEntry
{
	FString Id;
	ENodeUpgradeNodeKind Kind = ENodeUpgradeNodeKind::Unknown;
	/** Resource descriptor class name (e.g. Desc_OreIron_C). NAME_None (JSON null) matches any resource of this kind. */
	FName ResourceClassName;
	/** Index 0: "Impure->Normal", index 1: "Normal->Pure". Never empty once loaded. */
	TArray<FNodeUpgradeCostItem> Tiers[NodeUpgradeTier::Count];
};

/**
 * Upgrade costs loaded from Resources/costs.json (copy of data/costs.json).
 * Plain data, no UObject reference: item classes are resolved and cached by the subsystem.
 */
class NODEUPGRADE_API FNodeUpgradeCostTable
{
public:
	/**
	 * Parses and validates the file. Invalid entries are skipped and reported in OutErrors.
	 * Returns false (and keeps the previous content) if the file is missing, is not valid JSON or has an unknown schema.
	 */
	bool LoadFromFile(const FString& FilePath, TArray<FString>& OutErrors, TArray<FString>& OutWarnings);

	/** Exact resource class match first, then the wildcard entry (resourceClass null) of the same kind. */
	const FNodeUpgradeCostEntry* FindEntry(ENodeUpgradeNodeKind Kind, FName ResourceClassName) const;

	bool IsLoaded() const { return bLoaded; }
	float GetRefundRatio() const { return RefundRatio; }
	const TArray<FNodeUpgradeCostEntry>& GetEntries() const { return Entries; }
	const FString& GetSourcePath() const { return SourcePath; }

	/** <plugin dir>/Resources/costs.json, or an empty string if the plugin cannot be found. */
	static FString GetDefaultFilePath();

	static ENodeUpgradeNodeKind ParseNodeKind(const FString& Value);
	static const TCHAR* NodeKindToString(ENodeUpgradeNodeKind Kind);

	/** Keys of the "tiers" object, indexed like FNodeUpgradeCostEntry::Tiers. */
	static const TCHAR* TierKey(int32 Tier);

private:
	TArray<FNodeUpgradeCostEntry> Entries;
	float RefundRatio = 0.5f;
	bool bLoaded = false;
	FString SourcePath;
};
