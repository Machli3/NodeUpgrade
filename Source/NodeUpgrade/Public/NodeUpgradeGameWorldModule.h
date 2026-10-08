#pragma once

#include "CoreMinimal.h"
#include "Module/GameWorldModule.h"
#include "NodeUpgradeGameWorldModule.generated.h"

/**
 * Root game world module of the mod, declared in C++ (SML discovers native root modules: see FPluginModuleLoader::FindRootModulesOfType).
 * Registers the subsystem and the debug chat command. No Blueprint asset is needed.
 */
UCLASS()
class NODEUPGRADE_API UNodeUpgradeGameWorldModule : public UGameWorldModule
{
	GENERATED_BODY()

public:
	UNodeUpgradeGameWorldModule();
};
