#include "NodeUpgradeChatCommand.h"

#include "Buildables/FGBuildableGeneratorGeoThermal.h"
#include "Buildables/FGBuildableResourceExtractor.h"
#include "Command/CommandSender.h"
#include "EngineUtils.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "FGPlayerController.h"
#include "NodeUpgradeCostTable.h"
#include "NodeUpgradeGameAccess.h"
#include "NodeUpgradeSubsystem.h"
#include "NodeUpgradeTargeting.h"
#include "NodeUpgradeText.h"
#include "Resources/FGItemDescriptor.h"
#include "Resources/FGResourceNode.h"

// Developer-only messages: localizable on purpose (no hard-coded display strings), not translated.
#define LOCTEXT_NAMESPACE "NodeUpgradeDebug"

namespace
{
	constexpr int32 MaxReportedErrors = 10;

	void Send(UCommandSender* Sender, const FText& Message, const FLinearColor& Color = FLinearColor::Green)
	{
		Sender->SendChatMessage(Message.ToString(), Color);
	}

	FText AsText(const FString& Value)
	{
		return FText::FromString(Value);
	}

	FText PurityOrNone(EResourcePurity Purity)
	{
		return NodeUpgradeTier::IsValidPurity(Purity) ? NodeUpgradeText::PurityName(Purity) : LOCTEXT("None", "none");
	}

	FText ItemsToText(const TArray<FNodeUpgradeItemAmount>& Items)
	{
		TArray<FText> Parts;
		for (const FNodeUpgradeItemAmount& Item : Items)
		{
			Parts.Add(FText::FormatNamed(LOCTEXT("ItemAmount", "{Amount} x {Item}"),
				TEXT("Amount"), FText::AsNumber(Item.Amount),
				TEXT("Item"), Item.ItemClass != nullptr ? UFGItemDescriptor::GetItemName(Item.ItemClass) : LOCTEXT("UnknownItem", "unknown item")));
		}
		return Parts.Num() > 0 ? FText::Join(LOCTEXT("ItemSeparator", ", "), Parts) : LOCTEXT("Nothing", "nothing");
	}
}

ANodeUpgradeChatCommand::ANodeUpgradeChatCommand()
{
	CommandName = TEXT("nodeupgrade");
	Aliases.Add(TEXT("nu"));
	MinNumberOfArguments = 1;
	bOnlyUsableByPlayer = true;
	Usage = LOCTEXT("Usage", "/nodeupgrade (alias /nu) info | upgrade | downgrade | validate | reload");
}

EExecutionStatus ANodeUpgradeChatCommand::ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
	if (Sender == nullptr || Arguments.Num() == 0)
	{
		return EExecutionStatus::BAD_ARGUMENTS;
	}

	AFGPlayerController* PlayerController = Sender->GetPlayer();
	AFGCharacterPlayer* Character = PlayerController != nullptr ? Cast<AFGCharacterPlayer>(PlayerController->GetPawn()) : nullptr;
	const FString SubCommand = Arguments[0].ToLower();

	if (SubCommand == TEXT("validate"))
	{
		return RunValidate(Sender);
	}
	if (SubCommand == TEXT("reload"))
	{
		return RunReload(Sender);
	}

	if (Character == nullptr)
	{
		Send(Sender, LOCTEXT("NoCharacter", "This command needs you on foot (not in a vehicle)."), FLinearColor::Red);
		return EExecutionStatus::UNCOMPLETED;
	}
	if (SubCommand == TEXT("info"))
	{
		return RunInfo(Sender, Character);
	}
	if (SubCommand == TEXT("upgrade"))
	{
		return RunUpgrade(Sender, Character, true);
	}
	if (SubCommand == TEXT("downgrade"))
	{
		return RunUpgrade(Sender, Character, false);
	}

	PrintCommandUsage(Sender);
	return EExecutionStatus::BAD_ARGUMENTS;
}

AFGResourceNode* ANodeUpgradeChatCommand::GetTargetOrReport(UCommandSender* Sender, AFGCharacterPlayer* Character) const
{
	AFGResourceNode* Node = FNodeUpgradeTargeting::FindTargetNode(Character);
	if (Node == nullptr)
	{
		Send(Sender, LOCTEXT("NoTarget", "Aim at a resource node (or at an extractor built on it) first."), FLinearColor::Red);
	}
	return Node;
}

EExecutionStatus ANodeUpgradeChatCommand::RunInfo(UCommandSender* Sender, AFGCharacterPlayer* Character)
{
	AFGResourceNode* Node = GetTargetOrReport(Sender, Character);
	if (Node == nullptr)
	{
		return EExecutionStatus::UNCOMPLETED;
	}
	ANodeUpgradeSubsystem* Subsystem = ANodeUpgradeSubsystem::Get(this);
	const TSubclassOf<UFGResourceDescriptor> ResourceClass = Node->GetResourceClass();
	const FNodeUpgradeRecord* Record = Subsystem != nullptr ? Subsystem->FindRecord(Node) : nullptr;
	const FNodeUpgradeCostEntry* Entry = Subsystem != nullptr ? Subsystem->FindCostEntry(Node) : nullptr;

	Send(Sender, FText::FormatNamed(LOCTEXT("InfoNode", "Node {Name}: {Resource} ({Class}), kind {Kind}"),
		TEXT("Name"), AsText(Node->GetName()),
		TEXT("Resource"), Node->GetResourceName(),
		TEXT("Class"), AsText(ResourceClass != nullptr ? ResourceClass->GetName() : FString(TEXT("null"))),
		TEXT("Kind"), AsText(FNodeUpgradeCostTable::NodeKindToString(ANodeUpgradeSubsystem::GetNodeKind(Node)))));

	Send(Sender, FText::FormatNamed(LOCTEXT("InfoPurity", "Purity {Purity} | original {Original} | modified by the mod: {Modified} | cost entry: {Entry}"),
		TEXT("Purity"), PurityOrNone(Node->GetResourcePurity()),
		TEXT("Original"), PurityOrNone(Record != nullptr ? Record->OriginalPurity.GetValue() : Node->GetResourcePurity()),
		TEXT("Modified"), Record != nullptr ? LOCTEXT("Yes", "yes") : LOCTEXT("No", "no"),
		TEXT("Entry"), Entry != nullptr ? AsText(Entry->Id) : LOCTEXT("NoEntry", "none")));

	if (Record != nullptr)
	{
		for (int32 Tier = 0; Tier < NodeUpgradeTier::Count; ++Tier)
		{
			Send(Sender, FText::FormatNamed(LOCTEXT("InfoPaid", "Paid for {Tier}: {Items}"),
				TEXT("Tier"), AsText(FNodeUpgradeCostTable::TierKey(Tier)),
				TEXT("Items"), ItemsToText(Record->GetPayment(Tier).Items)));
		}
	}

	int32 BuildingCount = 0;
	for (TActorIterator<AFGBuildableResourceExtractor> It(GetWorld()); It; ++It)
	{
		if (It->GetExtractableResource().GetObject() == Node)
		{
			++BuildingCount;
			Send(Sender, FText::FormatNamed(LOCTEXT("InfoExtractor", "{Name}: {Rate}/min, cycle {Cycle} s"),
				TEXT("Name"), AsText(It->GetName()),
				TEXT("Rate"), FText::AsNumber(It->GetExtractionPerMinute()),
				TEXT("Cycle"), FText::AsNumber(It->GetProductionCycleTime())));
		}
	}
	for (TActorIterator<AFGBuildableGeneratorGeoThermal> It(GetWorld()); It; ++It)
	{
		if (FNodeUpgradeGameAccess::GetGeoThermalResource(*It) == Node)
		{
			++BuildingCount;
			Send(Sender, FText::FormatNamed(LOCTEXT("InfoGeoThermal", "{Name}: {Min} - {Max} MW"),
				TEXT("Name"), AsText(It->GetName()),
				TEXT("Min"), FText::AsNumber(It->GetMinPowerProduction()),
				TEXT("Max"), FText::AsNumber(It->GetMaxPowerProduction())));
		}
	}
	if (BuildingCount == 0)
	{
		Send(Sender, LOCTEXT("InfoNoBuilding", "No extractor or generator on this node."));
	}
	return EExecutionStatus::COMPLETED;
}

EExecutionStatus ANodeUpgradeChatCommand::RunUpgrade(UCommandSender* Sender, AFGCharacterPlayer* Character, bool bUpgrade)
{
	AFGResourceNode* Node = GetTargetOrReport(Sender, Character);
	ANodeUpgradeSubsystem* Subsystem = ANodeUpgradeSubsystem::Get(this);
	if (Node == nullptr || Subsystem == nullptr)
	{
		return EExecutionStatus::UNCOMPLETED;
	}
	const ENodeUpgradeResult Result = bUpgrade ? Subsystem->TryUpgrade(Node, Character) : Subsystem->TryDowngrade(Node, Character);
	const bool bSuccess = Result == ENodeUpgradeResult::Success;
	Send(Sender, NodeUpgradeText::ResultMessage(Result, bUpgrade, Node->GetResourcePurity()), bSuccess ? FLinearColor::Green : FLinearColor::Red);
	return bSuccess ? EExecutionStatus::COMPLETED : EExecutionStatus::UNCOMPLETED;
}

EExecutionStatus ANodeUpgradeChatCommand::RunValidate(UCommandSender* Sender)
{
	ANodeUpgradeSubsystem* Subsystem = ANodeUpgradeSubsystem::Get(this);
	if (Subsystem == nullptr)
	{
		return EExecutionStatus::UNCOMPLETED;
	}
	const FNodeUpgradeCostTable& Table = Subsystem->GetCostTable();
	if (!Table.IsLoaded())
	{
		Send(Sender, LOCTEXT("ValidateNotLoaded", "No cost table loaded. Check FactoryGame.log for LogNodeUpgrade errors."), FLinearColor::Red);
		return EExecutionStatus::UNCOMPLETED;
	}

	int32 Total = 0;
	int32 Missing = 0;
	for (const FNodeUpgradeCostEntry& Entry : Table.GetEntries())
	{
		for (int32 Tier = 0; Tier < NodeUpgradeTier::Count; ++Tier)
		{
			for (const FNodeUpgradeCostItem& Item : Entry.Tiers[Tier])
			{
				++Total;
				if (Subsystem->ResolveItemClass(Item.ItemClassName) == nullptr)
				{
					++Missing;
					Send(Sender, FText::FormatNamed(LOCTEXT("ValidateMissing", "{Entry} / {Tier}: item class {Class} not found"),
						TEXT("Entry"), AsText(Entry.Id),
						TEXT("Tier"), AsText(FNodeUpgradeCostTable::TierKey(Tier)),
						TEXT("Class"), AsText(Item.ItemClassName.ToString())), FLinearColor::Red);
				}
			}
		}
	}
	Send(Sender, FText::FormatNamed(LOCTEXT("ValidateSummary", "{Entries} entries, {Found} / {Total} item classes found, refund ratio {Ratio}. File: {Path}"),
		TEXT("Entries"), FText::AsNumber(Table.GetEntries().Num()),
		TEXT("Found"), FText::AsNumber(Total - Missing),
		TEXT("Total"), FText::AsNumber(Total),
		TEXT("Ratio"), FText::AsNumber(Table.GetRefundRatio()),
		TEXT("Path"), AsText(Table.GetSourcePath())),
		Missing == 0 ? FLinearColor::Green : FLinearColor::Red);
	return Missing == 0 ? EExecutionStatus::COMPLETED : EExecutionStatus::UNCOMPLETED;
}

EExecutionStatus ANodeUpgradeChatCommand::RunReload(UCommandSender* Sender)
{
	ANodeUpgradeSubsystem* Subsystem = ANodeUpgradeSubsystem::Get(this);
	if (Subsystem == nullptr)
	{
		return EExecutionStatus::UNCOMPLETED;
	}
	TArray<FString> Errors;
	TArray<FString> Warnings;
	const bool bLoaded = Subsystem->ReloadCostTable(Errors, Warnings);
	for (int32 Index = 0; Index < Errors.Num() && Index < MaxReportedErrors; ++Index)
	{
		Send(Sender, AsText(Errors[Index]), FLinearColor::Red);
	}
	Send(Sender, FText::FormatNamed(LOCTEXT("ReloadDone", "costs.json reloaded: {Result}, {Entries} entries, {Errors} errors, {Warnings} warnings. Already paid refunds are unchanged."),
		TEXT("Result"), bLoaded ? LOCTEXT("Ok", "ok") : LOCTEXT("Failed", "failed (previous table kept)"),
		TEXT("Entries"), FText::AsNumber(Subsystem->GetCostTable().GetEntries().Num()),
		TEXT("Errors"), FText::AsNumber(Errors.Num()),
		TEXT("Warnings"), FText::AsNumber(Warnings.Num())),
		bLoaded && Errors.Num() == 0 ? FLinearColor::Green : FLinearColor::Red);
	return bLoaded ? EExecutionStatus::COMPLETED : EExecutionStatus::UNCOMPLETED;
}

#undef LOCTEXT_NAMESPACE
