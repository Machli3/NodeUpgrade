#include "NodeUpgradeGameWorldModule.h"

#include "NodeUpgradeChatCommand.h"
#include "NodeUpgradeSubsystem.h"

UNodeUpgradeGameWorldModule::UNodeUpgradeGameWorldModule()
{
	bRootModule = true;
	ModSubsystems.Add(ANodeUpgradeSubsystem::StaticClass());
	mChatCommands.Add(ANodeUpgradeChatCommand::StaticClass());
}
