#pragma once

#include "CoreMinimal.h"
#include "Module/GameInstanceModule.h"
#include "NodeUpgradeGameInstanceModule.generated.h"

/**
 * Root game instance module of the mod, declared in C++ like UNodeUpgradeGameWorldModule (no Blueprint asset).
 * Registers the widget hook that inserts the hint line into the game's look-at prompt, once per game launch, before any world.
 */
UCLASS()
class NODEUPGRADE_API UNodeUpgradeGameInstanceModule : public UGameInstanceModule
{
	GENERATED_BODY()

public:
	UNodeUpgradeGameInstanceModule();
};
