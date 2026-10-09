#include "NodeUpgradeGameAccess.h"

#include "Buildables/FGBuildableFrackingActivator.h"
#include "Buildables/FGBuildableGeneratorGeoThermal.h"
#include "Buildables/FGBuildableResourceExtractor.h"
#include "FGCharacterPlayer.h"
#include "NodeUpgradeInteractionComponent.h"

void FNodeUpgradeGameAccess::RecalculateExtractor(AFGBuildableResourceExtractor* Extractor)
{
	if (IsValid(Extractor))
	{
		Extractor->CalculateProductionCycleTime();
	}
}

void FNodeUpgradeGameAccess::RecalculateFrackingActivator(AFGBuildableFrackingActivator* Activator)
{
	if (IsValid(Activator))
	{
		Activator->CalculateDefaultPotentialExtractionPerMinute();
	}
}

void FNodeUpgradeGameAccess::RecalculateGeoThermal(AFGBuildableGeneratorGeoThermal* Generator)
{
	if (IsValid(Generator) && Generator->mExtractableResource != nullptr)
	{
		Generator->OnExtractableResourceSet();
	}
}

AActor* FNodeUpgradeGameAccess::GetGeoThermalResource(const AFGBuildableGeneratorGeoThermal* Generator)
{
	return IsValid(Generator) ? Generator->mExtractableResource.Get() : nullptr;
}

void FNodeUpgradeGameAccess::BindBestUsableActorUpdated(AFGCharacterPlayer* Character, UNodeUpgradeInteractionComponent* Component)
{
	if (IsValid(Character) && IsValid(Component))
	{
		Character->mOnBestUseableActorUpdated.AddUniqueDynamic(Component, &UNodeUpgradeInteractionComponent::HandleBestUsableActorUpdated);
	}
}
