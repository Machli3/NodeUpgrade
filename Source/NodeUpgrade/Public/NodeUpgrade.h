#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

NODEUPGRADE_API DECLARE_LOG_CATEGORY_EXTERN(LogNodeUpgrade, Log, All);

class AFGCharacterPlayer;
class UInputComponent;

class FNodeUpgradeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Fired by the game every time a local player's input component is set up (AFGCharacterPlayer::OnPlayerInputInitialized). */
	void HandlePlayerInputInitialized(AFGCharacterPlayer* Character, UInputComponent* InputComponent);

	FDelegateHandle PlayerInputInitializedHandle;
};
