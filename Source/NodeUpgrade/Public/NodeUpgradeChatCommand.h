#pragma once

#include "CoreMinimal.h"
#include "Command/ChatCommandInstance.h"
#include "NodeUpgradeChatCommand.generated.h"

class AFGCharacterPlayer;
class AFGResourceNode;
class UCommandSender;

/**
 * Chat command for checks and for players who edit costs.json. Type it in the in-game chat:
 *   /nodeupgrade info | upgrade | downgrade | validate | reload
 * No cheat: the free "setpurity" and "give" debug sub-commands were removed for the public release (1.0.0).
 * Alias: /nu. Targets the node the player is aiming at.
 * Messages are developer-only: they use LOCTEXT (namespace NodeUpgradeDebug) but are intentionally not translated.
 */
UCLASS()
class NODEUPGRADE_API ANodeUpgradeChatCommand : public AChatCommandInstance
{
	GENERATED_BODY()

public:
	ANodeUpgradeChatCommand();

	virtual EExecutionStatus ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label) override;

private:
	EExecutionStatus RunInfo(UCommandSender* Sender, AFGCharacterPlayer* Character);
	EExecutionStatus RunUpgrade(UCommandSender* Sender, AFGCharacterPlayer* Character, bool bUpgrade);
	EExecutionStatus RunValidate(UCommandSender* Sender);
	EExecutionStatus RunReload(UCommandSender* Sender);

	/** Node under the crosshair, or null after telling the sender. */
	AFGResourceNode* GetTargetOrReport(UCommandSender* Sender, AFGCharacterPlayer* Character) const;
};
