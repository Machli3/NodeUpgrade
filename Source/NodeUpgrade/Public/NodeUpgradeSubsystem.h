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

DECLARE_MULTICAST_DELEGATE(FOnNodeUpgradeStateChanged);

/**
 * World subsystem holding the state of modified nodes and all upgrade rules.
 * Spawned by SML from UNodeUpgradeGameWorldModule, saved with the game (IFGSaveInterface).
 * Never ticks: it only works on purchase / downgrade / debug command, and once after loading for modified nodes only.
 * Multiplayer (hosted game): spawned on the host and replicated. All purchase logic runs on the host (HasAuthority);
 * clients send their requests through UNodeUpgradeRemoteCallObject and receive the records and the host's cost table.
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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Everything the menu displays. Does not modify anything. */
	FNodeUpgradePreview BuildPreview(AFGResourceNode* Node, AFGCharacterPlayer* Character);

	/**
	 * Host side entry point of the menu (local host and remote players alike): refused with OutOfDate if the node's purity
	 * is no longer the one the player saw, then TryUpgrade / TryDowngrade with that player's own character.
	 */
	ENodeUpgradeResult HandleRequest(AFGResourceNode* Node, AFGCharacterPlayer* Character, bool bUpgrade, EResourcePurity SeenPurity);

	/** Fired on every machine when the records or the cost table change (purchase, downgrade, reload, data received from the host). */
	FOnNodeUpgradeStateChanged OnStateChanged;

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
	/** Broadcasts OnStateChanged and refreshes the local player's look-at hint. */
	void NotifyStateChanged();

	/** Client: applies the received purities locally (nodes are dormant actors) and refreshes the extractors shown on this machine. */
	UFUNCTION()
	void OnRep_Records();

	/** Client: the host's cost table replaces the local costs.json. */
	UFUNCTION()
	void OnRep_CostTable();

	/** Modified nodes only. Replicated to clients (menu preview, refunds, hint). */
	UPROPERTY(SaveGame, ReplicatedUsing = OnRep_Records)
	TArray<FNodeUpgradeRecord> mRecords;

	/** Client: the records as last received, to find the nodes a new replication changed or released. */
	UPROPERTY(Transient)
	TArray<FNodeUpgradeRecord> mPreviousRecords;

	/** Host: copy of mCostTable sent to clients. */
	UPROPERTY(ReplicatedUsing = OnRep_CostTable)
	FNodeUpgradeNetCostTable mNetCostTable;

	/** Version of the saved data layout, for future migrations. */
	UPROPERTY(SaveGame)
	int32 mSaveDataVersion = 1;

	UPROPERTY(Transient)
	TMap<FName, TSubclassOf<UFGItemDescriptor>> mResolvedItemClasses;

	TSet<FName> mReportedUnresolvedClasses;
	FNodeUpgradeCostTable mCostTable;
};
