#include "NodeUpgradeSubsystem.h"

#include "Buildables/FGBuildableFrackingActivator.h"
#include "Buildables/FGBuildableGeneratorGeoThermal.h"
#include "Buildables/FGBuildableResourceExtractor.h"
#include "Buildables/FGBuildableResourceExtractorBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FGCharacterPlayer.h"
#include "NodeUpgrade.h"
#include "NodeUpgradeGameAccess.h"
#include "NodeUpgradeTargeting.h"
#include "Registry/ModContentRegistry.h"
#include "Resources/FGResourceNode.h"
#include "Resources/FGResourceNodeFrackingCore.h"
#include "Resources/FGResourceNodeFrackingSatellite.h"
#include "Subsystem/SubsystemActorManager.h"
#include "TimerManager.h"

ANodeUpgradeSubsystem::ANodeUpgradeSubsystem()
{
	PrimaryActorTick.bCanEverTick = false;
	ReplicationPolicy = ESubsystemReplicationPolicy::SpawnOnServer;
}

ANodeUpgradeSubsystem* ANodeUpgradeSubsystem::Get(const UObject* WorldContext)
{
	if (WorldContext == nullptr || GEngine == nullptr)
	{
		return nullptr;
	}
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	if (World == nullptr)
	{
		return nullptr;
	}
	USubsystemActorManager* Manager = World->GetSubsystem<USubsystemActorManager>();
	return Manager != nullptr ? Manager->GetSubsystemActor<ANodeUpgradeSubsystem>() : nullptr;
}

// --- Lifecycle -------------------------------------------------------------------------------------------------

void ANodeUpgradeSubsystem::Init()
{
	Super::Init();
	LoadCostTable();
}

void ANodeUpgradeSubsystem::BeginPlay()
{
	Super::BeginPlay();

	// One-shot, next frame: every actor (including the node randomization manager) has run BeginPlay by then.
	GetWorldTimerManager().SetTimerForNextTick(this, &ANodeUpgradeSubsystem::ReapplyStoredPurities);
}

void ANodeUpgradeSubsystem::PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion) {}
void ANodeUpgradeSubsystem::PostSaveGame_Implementation(int32 saveVersion, int32 gameVersion) {}
void ANodeUpgradeSubsystem::PreLoadGame_Implementation(int32 saveVersion, int32 gameVersion) {}

void ANodeUpgradeSubsystem::PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	const int32 LoadedCount = mRecords.Num();
	mRecords.RemoveAll([](const FNodeUpgradeRecord& Record)
	{
		return !IsValid(Record.Node)
			|| !NodeUpgradeTier::IsValidPurity(Record.OriginalPurity.GetValue())
			|| !NodeUpgradeTier::IsValidPurity(Record.CurrentPurity.GetValue());
	});
	if (mRecords.Num() != LoadedCount)
	{
		UE_LOG(LogNodeUpgrade, Warning, TEXT("Dropped %d invalid node records while loading (node missing from the map or corrupt data)"), LoadedCount - mRecords.Num());
	}
	UE_LOG(LogNodeUpgrade, Display, TEXT("Loaded %d modified node records (data version %d)"), mRecords.Num(), mSaveDataVersion);
}

void ANodeUpgradeSubsystem::GatherDependencies_Implementation(TArray<UObject*>& out_dependentObjects)
{
	// Nodes are level actors and always exist before save data is applied: no ordering dependency needed.
}

bool ANodeUpgradeSubsystem::NeedTransform_Implementation()
{
	return false;
}

bool ANodeUpgradeSubsystem::ShouldSave_Implementation() const
{
	return true;
}

void ANodeUpgradeSubsystem::ReapplyStoredPurities()
{
	TSet<AFGResourceNode*> ChangedNodes;
	for (const FNodeUpgradeRecord& Record : mRecords)
	{
		AFGResourceNode* Node = Record.Node;
		if (!IsValid(Node))
		{
			continue;
		}
		const EResourcePurity Stored = Record.CurrentPurity.GetValue();
		if (Node->GetResourcePurity() != Stored)
		{
			Node->SetResourcePurityOverride(Stored);
			Node->FlushNetDormancy();
			ChangedNodes.Add(Node);
		}
	}

	if (ChangedNodes.Num() > 0)
	{
		RefreshDependentBuildings(GetWorld(), ChangedNodes);
	}
	UE_LOG(LogNodeUpgrade, Display, TEXT("Restored %d modified nodes (%d needed their purity re-applied)"), mRecords.Num(), ChangedNodes.Num());

	ValidateCostItemClasses();
}

// --- Cost table ------------------------------------------------------------------------------------------------

void ANodeUpgradeSubsystem::LoadCostTable()
{
	TArray<FString> Errors;
	TArray<FString> Warnings;
	ReloadCostTable(Errors, Warnings);
}

bool ANodeUpgradeSubsystem::ReloadCostTable(TArray<FString>& OutErrors, TArray<FString>& OutWarnings)
{
	const FString Path = FNodeUpgradeCostTable::GetDefaultFilePath();
	const bool bLoaded = mCostTable.LoadFromFile(Path, OutErrors, OutWarnings);

	for (const FString& Warning : OutWarnings)
	{
		UE_LOG(LogNodeUpgrade, Warning, TEXT("costs.json: %s"), *Warning);
	}
	for (const FString& Error : OutErrors)
	{
		UE_LOG(LogNodeUpgrade, Error, TEXT("costs.json: %s"), *Error);
	}
	if (bLoaded)
	{
		UE_LOG(LogNodeUpgrade, Display, TEXT("Loaded %d cost entries from '%s' (refund ratio %.2f)"), mCostTable.GetEntries().Num(), *mCostTable.GetSourcePath(), mCostTable.GetRefundRatio());
	}
	else if (!mCostTable.IsLoaded())
	{
		UE_LOG(LogNodeUpgrade, Error, TEXT("No cost table loaded: upgrades are disabled until costs.json is fixed (downgrades still work)"));
	}

	// Item names may have changed: resolve again on next use.
	mResolvedItemClasses.Reset();
	mReportedUnresolvedClasses.Reset();
	return bLoaded;
}

void ANodeUpgradeSubsystem::ValidateCostItemClasses()
{
	int32 Total = 0;
	int32 Missing = 0;
	for (const FNodeUpgradeCostEntry& Entry : mCostTable.GetEntries())
	{
		for (int32 Tier = 0; Tier < NodeUpgradeTier::Count; ++Tier)
		{
			for (const FNodeUpgradeCostItem& Item : Entry.Tiers[Tier])
			{
				++Total;
				if (ResolveItemClass(Item.ItemClassName) == nullptr)
				{
					++Missing;
				}
			}
		}
	}
	if (Missing > 0)
	{
		UE_LOG(LogNodeUpgrade, Error, TEXT("%d of %d item classes in costs.json could not be found (see warnings above). Affected nodes cannot be upgraded."), Missing, Total);
	}
	else
	{
		UE_LOG(LogNodeUpgrade, Display, TEXT("All %d item classes in costs.json were found"), Total);
	}
}

TSubclassOf<UFGItemDescriptor> ANodeUpgradeSubsystem::ResolveItemClass(FName ItemClassName)
{
	if (ItemClassName.IsNone())
	{
		return nullptr;
	}
	if (const TSubclassOf<UFGItemDescriptor>* Cached = mResolvedItemClasses.Find(ItemClassName))
	{
		return *Cached;
	}

	UClass* Found = nullptr;

	// Every item the game (and other mods) registered, through SML's content registry.
	if (UModContentRegistry* Registry = UModContentRegistry::Get(this))
	{
		for (const FGameObjectRegistration& Registration : Registry->GetLoadedItemDescriptors())
		{
			UClass* ItemClass = Cast<UClass>(Registration.RegisteredObject.Get());
			if (ItemClass != nullptr && ItemClass->GetFName() == ItemClassName && ItemClass->IsChildOf(UFGItemDescriptor::StaticClass()))
			{
				Found = ItemClass;
				break;
			}
		}
	}

	// Fallback: any loaded class with that exact name (item descriptors are UBlueprintGeneratedClass, a UClass subclass).
	if (Found == nullptr)
	{
		UClass* ItemClass = FindFirstObject<UClass>(*ItemClassName.ToString(), EFindFirstObjectOptions::None);
		if (ItemClass != nullptr && ItemClass->IsChildOf(UFGItemDescriptor::StaticClass()))
		{
			Found = ItemClass;
		}
	}

	if (Found != nullptr)
	{
		mResolvedItemClasses.Add(ItemClassName, Found);
	}
	else if (!mReportedUnresolvedClasses.Contains(ItemClassName))
	{
		mReportedUnresolvedClasses.Add(ItemClassName);
		UE_LOG(LogNodeUpgrade, Warning, TEXT("costs.json: item class '%s' not found in the game"), *ItemClassName.ToString());
	}
	return Found;
}

const FNodeUpgradeCostEntry* ANodeUpgradeSubsystem::FindCostEntry(const AFGResourceNode* Node) const
{
	if (!IsValid(Node) || !mCostTable.IsLoaded())
	{
		return nullptr;
	}
	const TSubclassOf<UFGResourceDescriptor> ResourceClass = Node->GetResourceClass();
	const FName ResourceClassName = ResourceClass != nullptr ? ResourceClass->GetFName() : NAME_None;
	return mCostTable.FindEntry(GetNodeKind(Node), ResourceClassName);
}

bool ANodeUpgradeSubsystem::ResolveTierCost(const AFGResourceNode* Node, int32 Tier, TArray<FNodeUpgradeItemAmount>& OutCost)
{
	OutCost.Reset();
	const FNodeUpgradeCostEntry* Entry = FindCostEntry(Node);
	if (Entry == nullptr || Tier < 0 || Tier >= NodeUpgradeTier::Count || Entry->Tiers[Tier].Num() == 0)
	{
		return false;
	}
	for (const FNodeUpgradeCostItem& Item : Entry->Tiers[Tier])
	{
		const TSubclassOf<UFGItemDescriptor> ItemClass = ResolveItemClass(Item.ItemClassName);
		if (ItemClass == nullptr || Item.Amount <= 0)
		{
			OutCost.Reset();
			return false;
		}
		OutCost.Add(FNodeUpgradeItemAmount{ ItemClass, Item.Amount });
	}
	return true;
}

// --- Rules -----------------------------------------------------------------------------------------------------

ENodeUpgradeNodeKind ANodeUpgradeSubsystem::GetNodeKind(const AFGResourceNode* Node)
{
	if (!IsValid(Node))
	{
		return ENodeUpgradeNodeKind::Unknown;
	}
	switch (Node->GetResourceNodeType())
	{
	case EResourceNodeType::FrackingSatellite:
		return ENodeUpgradeNodeKind::FrackingSatellite;
	case EResourceNodeType::Geyser:
		return ENodeUpgradeNodeKind::Geyser;
	case EResourceNodeType::Node:
		return Node->GetResourceForm() == EResourceForm::RF_SOLID ? ENodeUpgradeNodeKind::Solid : ENodeUpgradeNodeKind::FluidNode;
	default:
		return ENodeUpgradeNodeKind::Unknown;
	}
}

const FNodeUpgradeRecord* ANodeUpgradeSubsystem::FindRecord(const AFGResourceNode* Node) const
{
	return mRecords.FindByPredicate([Node](const FNodeUpgradeRecord& Record) { return Record.Node.Get() == Node; });
}

FNodeUpgradeRecord* ANodeUpgradeSubsystem::FindRecordMutable(const AFGResourceNode* Node)
{
	return mRecords.FindByPredicate([Node](const FNodeUpgradeRecord& Record) { return Record.Node.Get() == Node; });
}

void ANodeUpgradeSubsystem::ComputeRefund(const FNodeUpgradeRecord& Record, int32 Tier, TArray<FNodeUpgradeItemAmount>& OutRefund) const
{
	OutRefund.Reset();
	const double Ratio = static_cast<double>(mCostTable.GetRefundRatio());
	for (const FNodeUpgradeItemAmount& Paid : Record.GetPayment(Tier).Items)
	{
		if (Paid.ItemClass == nullptr || Paid.Amount <= 0)
		{
			continue;
		}
		// Rounded down and never more than what was paid.
		const int32 Amount = FMath::Clamp(FMath::FloorToInt32(static_cast<double>(Paid.Amount) * Ratio), 0, Paid.Amount);
		if (Amount > 0)
		{
			OutRefund.Add(FNodeUpgradeItemAmount{ Paid.ItemClass, Amount });
		}
	}
}

TArray<FInventoryStack> ANodeUpgradeSubsystem::MakeStacks(const TArray<FNodeUpgradeItemAmount>& Amounts)
{
	TArray<FInventoryStack> Stacks;
	for (const FNodeUpgradeItemAmount& Entry : Amounts)
	{
		if (Entry.ItemClass == nullptr || Entry.Amount <= 0)
		{
			continue;
		}
		const int32 StackSize = FMath::Max(1, UFGItemDescriptor::GetStackSize(Entry.ItemClass));
		int32 Remaining = Entry.Amount;
		while (Remaining > 0)
		{
			const int32 Count = FMath::Min(Remaining, StackSize);
			Stacks.Emplace(Count, Entry.ItemClass);
			Remaining -= Count;
		}
	}
	return Stacks;
}

FNodeUpgradePreview ANodeUpgradeSubsystem::BuildPreview(AFGResourceNode* Node, AFGCharacterPlayer* Character)
{
	FNodeUpgradePreview Preview;
	UFGInventoryComponent* Inventory = IsValid(Character) ? Character->GetInventory() : nullptr;
	if (!FNodeUpgradeTargeting::IsSupportedNode(Node) || Inventory == nullptr)
	{
		return Preview;
	}

	Preview.CurrentPurity = Node->GetResourcePurity();
	if (!NodeUpgradeTier::IsValidPurity(Preview.CurrentPurity))
	{
		return Preview;
	}

	const FNodeUpgradeRecord* Record = FindRecord(Node);
	Preview.OriginalPurity = Record != nullptr ? Record->OriginalPurity.GetValue() : Preview.CurrentPurity;
	Preview.Status = FindCostEntry(Node) != nullptr ? ENodeUpgradeResult::Success : ENodeUpgradeResult::ConfigError;

	const int32 UpgradeTier = NodeUpgradeTier::ForUpgradeFrom(Preview.CurrentPurity);
	if (UpgradeTier != INDEX_NONE)
	{
		Preview.bCanUpgrade = true;
		Preview.UpgradePurity = NodeUpgradeTier::Above(Preview.CurrentPurity);

		TArray<FNodeUpgradeItemAmount> Cost;
		if (ResolveTierCost(Node, UpgradeTier, Cost))
		{
			Preview.bUpgradeCostResolved = true;
			Preview.bHasAllItems = true;
			for (const FNodeUpgradeItemAmount& Item : Cost)
			{
				FNodeUpgradeCostLine& Line = Preview.UpgradeCost.AddDefaulted_GetRef();
				Line.ItemClass = Item.ItemClass;
				Line.Required = Item.Amount;
				Line.Owned = Inventory->GetNumItems(Item.ItemClass);
				Preview.bHasAllItems &= !Line.IsMissing();
			}
		}
	}

	const int32 DowngradeTier = NodeUpgradeTier::ForDowngradeFrom(Preview.CurrentPurity);
	if (Record != nullptr && DowngradeTier != INDEX_NONE && Preview.CurrentPurity > Record->OriginalPurity.GetValue())
	{
		Preview.bCanDowngrade = true;
		Preview.DowngradePurity = NodeUpgradeTier::Below(Preview.CurrentPurity);
		ComputeRefund(*Record, DowngradeTier, Preview.Refund);
		const TArray<FInventoryStack> Stacks = MakeStacks(Preview.Refund);
		Preview.bRefundFits = Stacks.Num() == 0 || Inventory->HasEnoughSpaceForStacks(Stacks);
	}

	return Preview;
}

ENodeUpgradeResult ANodeUpgradeSubsystem::TryUpgrade(AFGResourceNode* Node, AFGCharacterPlayer* Character)
{
	if (!HasAuthority())
	{
		return ENodeUpgradeResult::NotAuthority;
	}
	UFGInventoryComponent* Inventory = IsValid(Character) ? Character->GetInventory() : nullptr;
	if (!FNodeUpgradeTargeting::IsSupportedNode(Node) || Inventory == nullptr)
	{
		return ENodeUpgradeResult::InvalidTarget;
	}

	const EResourcePurity Current = Node->GetResourcePurity();
	if (!NodeUpgradeTier::IsValidPurity(Current))
	{
		return ENodeUpgradeResult::InvalidTarget;
	}
	const int32 Tier = NodeUpgradeTier::ForUpgradeFrom(Current);
	if (Tier == INDEX_NONE)
	{
		return ENodeUpgradeResult::MaxPurity;
	}

	TArray<FNodeUpgradeItemAmount> Cost;
	if (!ResolveTierCost(Node, Tier, Cost))
	{
		return ENodeUpgradeResult::ConfigError;
	}
	for (const FNodeUpgradeItemAmount& Item : Cost)
	{
		if (!Inventory->HasItems(Item.ItemClass, Item.Amount))
		{
			return ENodeUpgradeResult::NotEnoughItems;
		}
	}

	// Every condition holds: pay, record what was paid and change the purity in the same call.
	for (const FNodeUpgradeItemAmount& Item : Cost)
	{
		Inventory->Remove(Item.ItemClass, Item.Amount);
	}

	FNodeUpgradeRecord* Record = FindRecordMutable(Node);
	if (Record == nullptr)
	{
		Record = &mRecords.AddDefaulted_GetRef();
		Record->Node = Node;
		Record->OriginalPurity = Current;
	}
	const EResourcePurity NewPurity = NodeUpgradeTier::Above(Current);
	Record->GetPayment(Tier).Items = Cost;
	Record->CurrentPurity = NewPurity;

	ApplyPurity(Node, NewPurity);

	UE_LOG(LogNodeUpgrade, Display, TEXT("Upgraded %s: %d -> %d"), *Node->GetName(), static_cast<int32>(Current), static_cast<int32>(NewPurity));
	return ENodeUpgradeResult::Success;
}

ENodeUpgradeResult ANodeUpgradeSubsystem::TryDowngrade(AFGResourceNode* Node, AFGCharacterPlayer* Character)
{
	if (!HasAuthority())
	{
		return ENodeUpgradeResult::NotAuthority;
	}
	UFGInventoryComponent* Inventory = IsValid(Character) ? Character->GetInventory() : nullptr;
	if (!FNodeUpgradeTargeting::IsSupportedNode(Node) || Inventory == nullptr)
	{
		return ENodeUpgradeResult::InvalidTarget;
	}

	const EResourcePurity Current = Node->GetResourcePurity();
	if (!NodeUpgradeTier::IsValidPurity(Current))
	{
		return ENodeUpgradeResult::InvalidTarget;
	}

	const int32 RecordIndex = mRecords.IndexOfByPredicate([Node](const FNodeUpgradeRecord& Record) { return Record.Node.Get() == Node; });
	const int32 Tier = NodeUpgradeTier::ForDowngradeFrom(Current);
	if (RecordIndex == INDEX_NONE || Tier == INDEX_NONE || Current <= mRecords[RecordIndex].OriginalPurity.GetValue())
	{
		return ENodeUpgradeResult::BasePurity;
	}
	FNodeUpgradeRecord& Record = mRecords[RecordIndex];

	TArray<FNodeUpgradeItemAmount> Refund;
	ComputeRefund(Record, Tier, Refund);
	const TArray<FInventoryStack> Stacks = MakeStacks(Refund);
	if (Stacks.Num() > 0 && !Inventory->HasEnoughSpaceForStacks(Stacks))
	{
		return ENodeUpgradeResult::InventoryFull;
	}

	// Every condition holds: refund, forget that tier's payment and change the purity in the same call.
	if (Stacks.Num() > 0)
	{
		Inventory->AddStacks(Stacks);
	}
	const EResourcePurity NewPurity = NodeUpgradeTier::Below(Current);
	Record.GetPayment(Tier).Items.Reset();
	Record.CurrentPurity = NewPurity;
	const bool bBackToOriginal = NewPurity <= Record.OriginalPurity.GetValue();

	ApplyPurity(Node, NewPurity);

	if (bBackToOriginal)
	{
		// Nothing left to refund: the node is no longer "modified".
		mRecords.RemoveAt(RecordIndex);
	}

	UE_LOG(LogNodeUpgrade, Display, TEXT("Downgraded %s: %d -> %d (%d refund stacks)"), *Node->GetName(), static_cast<int32>(Current), static_cast<int32>(NewPurity), Stacks.Num());
	return ENodeUpgradeResult::Success;
}

void ANodeUpgradeSubsystem::ApplyPurity(AFGResourceNode* Node, EResourcePurity Purity)
{
	// mPurityOverride is SaveGame + Replicated on the node and wins over the level-authored mPurity in GetResourcePurity().
	Node->SetResourcePurityOverride(Purity);
	Node->FlushNetDormancy();

	TSet<AFGResourceNode*> Nodes;
	Nodes.Add(Node);
	RefreshDependentBuildings(GetWorld(), Nodes);
}

void ANodeUpgradeSubsystem::RefreshDependentBuildings(UWorld* World, const TSet<AFGResourceNode*>& Nodes)
{
	if (World == nullptr || Nodes.Num() == 0)
	{
		return;
	}

	// Miners, oil extractors and fracking extractors cache their cycle time: recompute it.
	int32 RefreshedExtractors = 0;
	for (TActorIterator<AFGBuildableResourceExtractorBase> It(World); It; ++It)
	{
		AFGResourceNode* ExtractedNode = Cast<AFGResourceNode>(It->GetExtractableResource().GetObject());
		if (ExtractedNode == nullptr || !Nodes.Contains(ExtractedNode))
		{
			continue;
		}
		if (AFGBuildableResourceExtractor* Extractor = Cast<AFGBuildableResourceExtractor>(*It))
		{
			FNodeUpgradeGameAccess::RecalculateExtractor(Extractor);
			++RefreshedExtractors;
		}
	}

	bool bHasGeyser = false;
	for (AFGResourceNode* Node : Nodes)
	{
		if (!IsValid(Node))
		{
			continue;
		}
		// Resource well: the pressurizer shows the sum of its satellites.
		if (AFGResourceNodeFrackingSatellite* Satellite = Cast<AFGResourceNodeFrackingSatellite>(Node))
		{
			const TWeakObjectPtr<AFGResourceNodeFrackingCore> Core = Satellite->GetCore();
			if (Core.IsValid())
			{
				const TWeakObjectPtr<AFGBuildableFrackingActivator> Activator = Core->GetActivator();
				if (Activator.IsValid())
				{
					FNodeUpgradeGameAccess::RecalculateFrackingActivator(Activator.Get());
				}
			}
		}
		bHasGeyser |= Node->GetResourceNodeType() == EResourceNodeType::Geyser;
	}

	int32 RefreshedGenerators = 0;
	if (bHasGeyser)
	{
		for (TActorIterator<AFGBuildableGeneratorGeoThermal> It(World); It; ++It)
		{
			AFGResourceNode* Geyser = Cast<AFGResourceNode>(FNodeUpgradeGameAccess::GetGeoThermalResource(*It));
			if (Geyser != nullptr && Nodes.Contains(Geyser))
			{
				FNodeUpgradeGameAccess::RecalculateGeoThermal(*It);
				++RefreshedGenerators;
			}
		}
	}

	UE_LOG(LogNodeUpgrade, Verbose, TEXT("Refreshed %d extractors and %d geothermal generators for %d nodes"), RefreshedExtractors, RefreshedGenerators, Nodes.Num());
}
