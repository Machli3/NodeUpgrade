#pragma once

#include "CoreMinimal.h"
#include "FGInventoryComponent.h"
#include "FGSaveInterface.h"
#include "NodeUpgradeCostTable.h"
#include "NodeUpgradeTypes.h"
#include "Subsystem/ModSubsystem.h"
#include "NodeUpgradeSubsystem.generated.h"

class AFGCharacterPlayer;
class AFGResourceNode;

/** One cost line shown in the menu. */
struct FNodeUpgradeCostLine
{
	TSubclassOf<UFGItemDescriptor> ItemClass = nullptr;
	int32 Required = 0;
	int32 Owned = 0;

	bool IsMissing() const { return Owned < Required; }
	int32 GetMissing() const { return FMath::Max(0, Required - Owned); }
};

/** Read-only snapshot used by the menu. Computed on demand, never cached. */
struct FNodeUpgradePreview
{
	/** InvalidTarget if the node cannot be handled at all, ConfigError if it has no usable cost entry, Success otherwise. */
	ENodeUpgradeResult Status = ENodeUpgradeResult::InvalidTarget;
	EResourcePurity CurrentPurity = RP_MAX;
	EResourcePurity OriginalPurity = RP_MAX;

	/** True while the node is below Pure. */
	bool bCanUpgrade = false;
	EResourcePurity UpgradePurity = RP_MAX;
	/** False if the tier cost could not be resolved (missing entry or unknown item class). */
	bool bUpgradeCostResolved = false;
	TArray<FNodeUpgradeCostLine> UpgradeCost;
	bool bHasAllItems = false;

	/** True while the node is above its original purity. */
	bool bCanDowngrade = false;
	EResourcePurity DowngradePurity = RP_MAX;
	TArray<FNodeUpgradeItemAmount> Refund;
	bool bRefundFits = false;
};

/**
 * World subsystem holding the state of modified nodes and all upgrade rules.
 * Spawned by SML from UNodeUpgradeGameWorldModule, saved with the game (IFGSaveInterface).
 * Never ticks: it only works on purchase / downgrade / debug command, and once after loading for modified nodes only.
 * All purchase logic is server-side (HasAuthority) so a multiplayer RPC can call it later without changes.
 */
UCLASS()
class NODEUPGRADE_API ANodeUpgradeSubsystem : public AModSubsystem, public IFGSaveInterface
{
	GENERATED_BODY()

public:
	ANodeUpgradeSubsystem();

	static ANodeUpgradeSubsystem* Get(const UObject* WorldContext);

	// Begin IFGSaveInterface
	virtual void PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void PostSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void PreLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void GatherDependencies_Implementation(TArray<UObject*>& out_dependentObjects) override;
	virtual bool NeedTransform_Implementation() override;
	virtual bool ShouldSave_Implementation() const override;
	// End IFGSaveInterface

	/** Everything the menu displays. Does not modify anything. */
	FNodeUpgradePreview BuildPreview(AFGResourceNode* Node, AFGCharacterPlayer* Character);

	/**
	 * Pays the next tier from the player's inventory and raises the purity by one step.
	 * Every condition is checked before anything changes: items and purity change together or not at all.
	 */
	ENodeUpgradeResult TryUpgrade(AFGResourceNode* Node, AFGCharacterPlayer* Character);

	/**
	 * Lowers the purity by one step and refunds floor(ratio x amount actually paid for that tier), in the same items.
	 * Refused if the refund does not fit in the inventory. Never goes below the original purity.
	 */
	ENodeUpgradeResult TryDowngrade(AFGResourceNode* Node, AFGCharacterPlayer* Character);

	const FNodeUpgradeRecord* FindRecord(const AFGResourceNode* Node) const;
	const TArray<FNodeUpgradeRecord>& GetRecords() const { return mRecords; }

	const FNodeUpgradeCostTable& GetCostTable() const { return mCostTable; }
	bool ReloadCostTable(TArray<FString>& OutErrors, TArray<FString>& OutWarnings);
	const FNodeUpgradeCostEntry* FindCostEntry(const AFGResourceNode* Node) const;

	/** Short class name from costs.json (e.g. Desc_IronPlate_C) to a loaded item class. Successful lookups are cached. */
	TSubclassOf<UFGItemDescriptor> ResolveItemClass(FName ItemClassName);

	static ENodeUpgradeNodeKind GetNodeKind(const AFGResourceNode* Node);

	/**
	 * Recomputes every building that reads the purity of these nodes: extractors (miners, oil, fracking),
	 * resource well pressurizers and geothermal generators. One pass over extractors, only on purchase / downgrade / debug / load.
	 */
	static void RefreshDependentBuildings(UWorld* World, const TSet<AFGResourceNode*>& Nodes);

	/** Splits amounts into inventory stacks that respect each item's stack size. */
	static TArray<FInventoryStack> MakeStacks(const TArray<FNodeUpgradeItemAmount>& Amounts);

protected:
	virtual void Init() override;
	virtual void BeginPlay() override;

private:
	FNodeUpgradeRecord* FindRecordMutable(const AFGResourceNode* Node);
	bool ResolveTierCost(const AFGResourceNode* Node, int32 Tier, TArray<FNodeUpgradeItemAmount>& OutCost);
	void ComputeRefund(const FNodeUpgradeRecord& Record, int32 Tier, TArray<FNodeUpgradeItemAmount>& OutRefund) const;
	void ApplyPurity(AFGResourceNode* Node, EResourcePurity Purity);
	void LoadCostTable();
	void ValidateCostItemClasses();
	void ReapplyStoredPurities();

	/** Modified nodes only. */
	UPROPERTY(SaveGame)
	TArray<FNodeUpgradeRecord> mRecords;

	/** Version of the saved data layout, for future migrations. */
	UPROPERTY(SaveGame)
	int32 mSaveDataVersion = 1;

	UPROPERTY(Transient)
	TMap<FName, TSubclassOf<UFGItemDescriptor>> mResolvedItemClasses;

	TSet<FName> mReportedUnresolvedClasses;
	FNodeUpgradeCostTable mCostTable;
};
