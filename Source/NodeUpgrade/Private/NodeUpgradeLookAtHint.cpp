#include "NodeUpgradeLookAtHint.h"

#include "FGCharacterPlayer.h"
#include "FGPlayerController.h"
#include "NodeUpgrade.h"
#include "NodeUpgradeHintRowWidget.h"
#include "NodeUpgradeInteractionComponent.h"
#include "NodeUpgradeSubsystem.h"
#include "NodeUpgradeTargeting.h"
#include "Resources/FGResourceNode.h"

void FNodeUpgradeLookAtHint::Refresh(AFGCharacterPlayer* Character, AActor* LookedAt)
{
	// Nothing to redo while the same actor is looked at with the same purity, however often the game sends its event.
	static TWeakObjectPtr<AActor> LastLookedAt;
	static EResourcePurity LastPurity = RP_MAX;
	static bool bHasLast = false;
	const AFGResourceNode* Node = FNodeUpgradeTargeting::ResolveNode(LookedAt);
	const EResourcePurity Purity = Node != nullptr ? Node->GetResourcePurity() : RP_MAX;
	if (bHasLast && LastLookedAt.Get() == LookedAt && LastPurity == Purity)
	{
		return;
	}
	bHasLast = true;
	LastLookedAt = LookedAt;
	LastPurity = Purity;
	UNodeUpgradeHintRowWidget::SetHintKey(GetHintKey(Character, LookedAt));
}

FText FNodeUpgradeLookAtHint::GetHintKey(AFGCharacterPlayer* Character, AActor* LookedAt)
{
	if (!IsValid(Character) || !Character->IsLocallyControlled())
	{
		return FText::GetEmpty();
	}
	// Same lookup as the menu key: the node itself, its rock, or the building on it.
	const AFGResourceNode* Node = FNodeUpgradeTargeting::ResolveNode(LookedAt);
	// Only when the key can actually upgrade the node: below Pure, with a cost entry (otherwise the menu would only show an error).
	if (Node == nullptr || Node->GetResourcePurity() >= RP_Pure)
	{
		return FText::GetEmpty();
	}
	const ANodeUpgradeSubsystem* Subsystem = ANodeUpgradeSubsystem::Get(Node);
	if (Subsystem == nullptr || Subsystem->FindCostEntry(Node) == nullptr)
	{
		return FText::GetEmpty();
	}

	const FText KeyName = UNodeUpgradeInteractionComponent::GetMenuKeyName(Character->GetFGPlayerController());
	static bool bReported = false;
	if (!bReported)
	{
		bReported = true;
		if (KeyName.IsEmpty())
		{
			UE_LOG(LogNodeUpgrade, Warning, TEXT("No key found for the menu action: the look-at hint is not shown"));
		}
		else
		{
			UE_LOG(LogNodeUpgrade, Display, TEXT("Look-at hint key: '%s' (first node: %s, looked at: %s)"), *KeyName.ToString(), *Node->GetName(), *LookedAt->GetName());
		}
	}
	return KeyName;
}
