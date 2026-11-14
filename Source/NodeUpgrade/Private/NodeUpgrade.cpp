#include "NodeUpgrade.h"

#include "FGCharacterPlayer.h"
#include "Misc/CoreMisc.h"
#include "NodeUpgradeInteractionComponent.h"

DEFINE_LOG_CATEGORY(LogNodeUpgrade);

void FNodeUpgradeModule::StartupModule()
{
	// Roadmap step 0 success criterion: this exact line must appear in FactoryGame.log.
	UE_LOG(LogNodeUpgrade, Display, TEXT("NodeUpgrade loaded"));

	if (!IsRunningDedicatedServer())
	{
		PlayerInputInitializedHandle = AFGCharacterPlayer::OnPlayerInputInitialized.AddRaw(this, &FNodeUpgradeModule::HandlePlayerInputInitialized);
	}
}

void FNodeUpgradeModule::ShutdownModule()
{
	if (PlayerInputInitializedHandle.IsValid())
	{
		AFGCharacterPlayer::OnPlayerInputInitialized.Remove(PlayerInputInitializedHandle);
	}
	PlayerInputInitializedHandle.Reset();
}

void FNodeUpgradeModule::HandlePlayerInputInitialized(AFGCharacterPlayer* Character, UInputComponent* InputComponent)
{
	UNodeUpgradeInteractionComponent::BindToPlayer(Character, InputComponent);
}

IMPLEMENT_MODULE(FNodeUpgradeModule, NodeUpgrade)
