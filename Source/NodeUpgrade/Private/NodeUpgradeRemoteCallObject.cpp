#include "NodeUpgradeRemoteCallObject.h"

#include "FGCharacterPlayer.h"
#include "FGPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "NodeUpgrade.h"
#include "NodeUpgradeInteractionComponent.h"
#include "NodeUpgradeSubsystem.h"
#include "Resources/FGResourceNode.h"

void UNodeUpgradeRemoteCallObject::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UNodeUpgradeRemoteCallObject, mDummyReplicatedField);
}

void UNodeUpgradeRemoteCallObject::Server_RequestChange_Implementation(AFGResourceNode* Node, bool bUpgrade, uint8 SeenPurity)
{
	AFGCharacterPlayer* Character = GetOwnerPlayerCharacter();
	ANodeUpgradeSubsystem* Subsystem = ANodeUpgradeSubsystem::Get(this);
	const EResourcePurity Seen = static_cast<EResourcePurity>(SeenPurity);

	ENodeUpgradeResult Result = ENodeUpgradeResult::InvalidTarget;
	if (Subsystem != nullptr && Character != nullptr && NodeUpgradeTier::IsValidPurity(Seen))
	{
		Result = Subsystem->HandleRequest(Node, Character, bUpgrade, Seen);
	}
	UE_LOG(LogNodeUpgrade, Display, TEXT("Request from %s: %s %s -> result %d"), Character != nullptr ? *Character->GetName() : TEXT("(no character)"),
		bUpgrade ? TEXT("upgrade") : TEXT("downgrade"), IsValid(Node) ? *Node->GetName() : TEXT("(no node)"), static_cast<int32>(Result));

	const EResourcePurity NewPurity = IsValid(Node) ? Node->GetResourcePurity() : RP_MAX;
	Client_ChangeResult(Node, bUpgrade, Result, static_cast<uint8>(NewPurity));
}

void UNodeUpgradeRemoteCallObject::Client_ChangeResult_Implementation(AFGResourceNode* Node, bool bUpgrade, ENodeUpgradeResult Result, uint8 NewPurity)
{
	AFGPlayerController* PlayerController = GetOwnerPlayerController();
	AFGCharacterPlayer* Character = PlayerController != nullptr ? Cast<AFGCharacterPlayer>(PlayerController->GetPawn()) : nullptr;
	UNodeUpgradeInteractionComponent* Component = Character != nullptr ? Character->FindComponentByClass<UNodeUpgradeInteractionComponent>() : nullptr;
	if (Component != nullptr)
	{
		Component->HandleRemoteResult(Node, bUpgrade, Result, static_cast<EResourcePurity>(NewPurity));
	}
}
