#pragma once

#include "CoreMinimal.h"
#include "FGRemoteCallObject.h"
#include "NodeUpgradeTypes.h"
#include "NodeUpgradeRemoteCallObject.generated.h"

class AFGResourceNode;

/**
 * Hosted multiplayer: carries a client's upgrade / downgrade request to the host and the answer back.
 * One instance per player controller, created by the game for every RCO class registered by UNodeUpgradeGameInstanceModule.
 * The host always acts with the requesting player's own character (GetOwnerPlayerCharacter), never one sent by the client.
 */
UCLASS(NotBlueprintable)
class NODEUPGRADE_API UNodeUpgradeRemoteCallObject : public UFGRemoteCallObject
{
	GENERATED_BODY()

public:
	/** Runs on the host. SeenPurity: the purity the player's menu showed (see ANodeUpgradeSubsystem::HandleRequest). */
	UFUNCTION(Server, Reliable)
	void Server_RequestChange(AFGResourceNode* Node, bool bUpgrade, uint8 SeenPurity);

	/** Runs on the requesting client: shows the result in its menu. */
	UFUNCTION(Client, Reliable)
	void Client_ChangeResult(AFGResourceNode* Node, bool bUpgrade, ENodeUpgradeResult Result, uint8 NewPurity);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** Remote call objects need at least one replicated property to be replicated to their owner (same as SML's own RCO). */
	UPROPERTY(Replicated)
	int32 mDummyReplicatedField = 0;
};
